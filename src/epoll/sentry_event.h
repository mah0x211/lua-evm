/*
 *  Copyright (C) 2015 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"), 
 *  to deal in the Software without restriction, including without limitation 
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 *  and/or sell copies of the Software, and to permit persons to whom the 
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.
 *
 *  epoll/sentry_event.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 *
 */

#ifndef SENTRY_EPOLL_EVENT_H
#define SENTRY_EPOLL_EVENT_H


#include "sentry.h"


static inline int sentry_wait( sentry_t *s, int timeout )
{
    return epoll_wait( s->fd, s->evs, s->nreg,
                       ( timeout < 0 ) ? -1 : timeout * 1000 );
}


static inline sentry_ev_t *sentry_getev( sentry_t *s, int *isdel )
{
    static uint8_t drain[sizeof( struct signalfd_siginfo )];
    sentry_ev_t *e = NULL;
    kevt_t *evt = NULL;
    int delflg = 0;

CHECK_NEXT:
    if( s->nevt > 0 )
    {
        evt = &s->evs[--s->nevt];
        // fetch sentry_ev_t from fdset
        if( !( e = (sentry_ev_t*)fdismember( &s->fds, evt->data.fd ) ) ){
            goto CHECK_NEXT;
        }
        
        e->evt = *evt;
        delflg = (e->reg.events & EPOLLONESHOT) |
                 (e->evt.events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR));
        switch( e->filter ){
            // drain data
            case EVFILT_SIGNAL:
                read( evt->data.fd, drain, sizeof( struct signalfd_siginfo ) );
                // remove from sigset
                if( delflg ){
                    sigdelset( &e->s->signals, e->ident );
                    goto DELETE_EVENT;
                }
            break;
            case EVFILT_TIMER:
                read( evt->data.fd, drain, sizeof( uint64_t ) );
            break;
        }
        
        // remove from kernel event
        if( delflg ){
DELETE_EVENT:
            *isdel = delflg;
            epoll_ctl( e->s->fd, EPOLL_CTL_DEL, e->reg.data.fd, NULL );
            fddelset( &e->s->fds, e->reg.data.fd );
        }
    }
    
    return e;
}


static inline int sentry_register( sentry_ev_t *e )
{
    // increase event-buffer and set event
    if( sentry_increase_evs( e->s, 1 ) == 0 &&
        fdset_realloc( &e->s->fds, e->reg.data.fd ) == 0 &&
        epoll_ctl( e->s->fd, EPOLL_CTL_ADD, e->reg.data.fd, &e->reg ) == 0 ){
        fdaddset( &e->s->fds, e->reg.data.fd, (void*)e );
        e->s->nreg++;
        return 0;
    }
    
    return -1;
}


// MARK: API for sentry_ev_t


static inline int sev_asfd( sentry_ev_t *e, int fd, int oneshot, int edge,
                            int filter )
{
    sentry_ev_t *sibling = fdismember( &e->s->fds, fd );
    kevt_t evt = {
        .events = filter|EPOLLRDHUP|
                  ( oneshot ? EPOLLONESHOT : 0 )|
                  ( edge ? EPOLLET : 0 ),
        .data = {
            .fd = fd
        }
    };

    // already watched
    if( sibling )
    {
        if( sibling->filter == filter ){
            errno = EALREADY;
            return -1;
        }
        // duplicate descriptor
        else if( ( evt.data.fd = dup( fd ) ) == -1 ){
            return -1;
        }
    }
    
    // set event fields
    e->ident = fd;
    e->filter = filter;
    e->reg = evt;
    if( sentry_register( e ) == 0 ){
        return 0;
    }

    // close duplicated
    if( evt.data.fd != fd ){
        close( evt.data.fd );
    }
    
    return -1;
}

static inline int sev_asreadable( sentry_ev_t *e, int fd, int oneshot,
                                  int edge )
{
    return sev_asfd( e, fd, oneshot, edge, EVFILT_READ );
}

static inline int sev_aswritable( sentry_ev_t *e, int fd, int oneshot,
                                  int edge )
{
    return sev_asfd( e, fd, oneshot, edge, EVFILT_WRITE );
}


static inline int sev_assignal( sentry_ev_t *e, int signo, int oneshot )
{
    // already watched
    if( sigismember( &e->s->signals, signo ) ){
        errno = EALREADY;
    }
    else
    {
        int fd = 0;
        sigset_t ss;

        // init sigset with signo
        sigemptyset( &ss );
        sigaddset( &ss, signo );
        
        // create event and signalfd with sigset_t
        if( ( fd = signalfd( -1, &ss, SFD_NONBLOCK|SFD_CLOEXEC ) ) != -1 )
        {
            // set event fields
            e->ident = signo;
            e->filter = EVFILT_SIGNAL;
            e->reg.data.fd = fd;
            e->reg.events = EPOLLRDHUP|EPOLLIN|( oneshot ? EPOLLONESHOT : 0 );
            // register to sentry
            if( sentry_register( e ) == 0 ){
                sigaddset( &e->s->signals, signo );
                return 0;
            }

            // close fd
            close( fd );
        }
    }
    
    return -1;
}


static inline int sev_astimer( sentry_ev_t *e, double timeout, int oneshot )
{
    // create timerfd
    int fd = timerfd_create( CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC );
    
    if( fd )
    {
        struct itimerspec its;
        
        // convert double to interval-timespec
        sentry_dbl2timespec( timeout, &its.it_interval );
        // set first invocation time
        its.it_value = its.it_interval;
        
        // set event fields
        e->ident = (uintptr_t)timeout;
        e->filter = EVFILT_TIMER;
        e->reg.data.fd = fd;
        e->reg.events = EPOLLRDHUP|EPOLLIN|( oneshot ? EPOLLONESHOT : 0 );

        // set timespec and register to sentry
        if( timerfd_settime( fd, 0, &its, NULL ) == 0 &&
            sentry_register( e ) == 0 ){
            return 0;
        }
        
        // close fd
        close( fd );
    }
    
    return -1;
}


static inline int sev_is_oneshot( sentry_ev_t *e )
{
    return e->reg.events & EPOLLONESHOT;
}


static inline int sev_is_hup( sentry_ev_t *e )
{
    return e->evt.events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR);
}


static inline int sev_ident_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    lua_pushinteger( L, e->ident );

    return 1;
}


static inline int sev_typeof_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    lua_pushinteger( L, sev_type( e ) );
    
    return 1;
}


static inline int sev_context_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    if( lstate_isref( e->ctx ) ){
        lstate_pushref( L, e->ctx );
        return 1;
    }
    
    return 0;
}


static inline int sev_watch_lua( lua_State *L, const char *mt,
                                 sentry_ev_t **ev )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );
    
    if( !lstate_isref( e->ref ) )
    {
        // register event
        if( sentry_register( e ) != 0 ){
            // got error
            lua_pushboolean( L, 0 );
            lua_pushstring( L, strerror( errno ) );
            return 2;
        }
        
        // retain event
        e->ref = lstate_ref( L );
        if( ev ){
            *ev = e;
        }
    }
    
    lua_pushboolean( L, 1 );
    
    return 1;
}


static inline int sev_unwatch_lua( lua_State *L, const char *mt,
                                   sentry_ev_t **ev )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );
    
    if( lstate_isref( e->ref ) ){
        struct epoll_event evt = e->reg;

        // del fd from fdset
        fddelset( &e->s->fds, e->reg.data.fd );
        // unregister event
        epoll_ctl( e->s->fd, EPOLL_CTL_DEL, e->reg.data.fd, &evt );
        e->s->nreg--;
        e->ref = lstate_unref( L, e->ref );
        if( ev ){
            *ev = e;
        }
    }

    lua_pushboolean( L, 1 );
    
    return 1;
}


static inline int sev_revert_lua( lua_State *L )
{
    lua_settop( L, 1 );
    // set event metatable
    lstate_setmetatable( L, SENTRY_EVENT_MT );

    return 1;
}


// implemented at epoll/common.c

int sev_gc_lua( lua_State *L );

// gc for readable/writable event
int sev_rwgc_lua( lua_State *L );


#endif
