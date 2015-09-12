/*
 *  Copyright (C) 2015 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *
 *  kevent/sentry_event.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef SENTRY_KEVENT_EVENT_H
#define SENTRY_KEVENT_EVENT_H


#include "sentry_types.h"


static inline int sev_is_oneshot( sentry_ev_t *e )
{
    return e->reg.flags & EV_ONESHOT;
}


static inline int sev_is_hup( sentry_ev_t *e )
{
    return e->evt.flags & (EV_EOF|EV_ERROR);
}


static inline int sentry_wait( sentry_t *s, int timeout )
{
    struct timespec ts = {
        timeout, 0
    };
    struct timespec *tsp = ( timeout < 0 ) ? NULL : &ts;
    
    return kevent( s->fd, NULL, 0, s->evs, (int)s->nreg, tsp );
}


static inline sentry_ev_t *sentry_getev( sentry_t *s, int *ishup )
{
    sentry_ev_t *e = NULL;
    kevt_t *evt = NULL;
    int hupflg = 0;
    
CHECK_NEXT:
    if( s->nevt > 0 )
    {
        evt = &s->evs[--s->nevt];
        hupflg = evt->flags & (EV_ONESHOT|EV_EOF|EV_ERROR);
        
        switch( evt->filter )
        {
            case EVFILT_READ:
                if( fdismember( &s->fds, evt->ident, FDSET_READ ) != 1 ){
                    goto CHECK_NEXT;
                }
                else if( hupflg ){
                    fddelset( &s->fds, evt->ident, FDSET_READ );
                }
            break;
            case EVFILT_WRITE:
                if( fdismember( &s->fds, evt->ident, FDSET_WRITE ) != 1 ){
                    goto CHECK_NEXT;
                }
                else if( hupflg ){
                    fddelset( &s->fds, evt->ident, FDSET_WRITE );
                }
            break;
            case EVFILT_SIGNAL:
                if( !sigismember( &s->signals, evt->ident ) ){
                    goto CHECK_NEXT;
                }
                else if( hupflg ){
                    sigdelset( &s->signals, evt->ident );
                }
            break;
        }
        
        // check hup flag
        if( ( *ishup = ( hupflg & (EV_EOF|EV_ERROR) ) ) )
        {
            // set errno
            if( hupflg & EV_ERROR ){
                errno = evt->data;
            }
            evt->flags = EV_DELETE;
            kevent( s->fd, evt, 1, NULL, 0, NULL );
        }
        
        e = (sentry_ev_t*)evt->udata;
        e->evt = *evt;
    }
    
    return e;
}


static inline int sentry_register( sentry_t *s, sentry_ev_t *e )
{
    // increase event-buffer and set event
    if( sentry_increase_evs( s, 1 ) == 0 &&
        kevent( s->fd, &e->reg, 1, NULL, 0, NULL ) == 0 ){
        s->nreg++;
        return 0;
    }
    
    return -1;
}


#define sev_fd_new( L, s, ctx, fd, type, oneshot, edge ) do { \
    /* already watched */ \
    if( fdismember( &s->fds, fd, FDSET_##type ) == 1 ){ \
        errno = EALREADY; \
    } \
    else if( fdset_realloc( &s->fds, fd ) == 0 && \
             fdaddset( &s->fds, fd, FDSET_##type ) == 0 ){ \
        /* create event */ \
        sentry_ev_t *e = lua_newuserdata( L, sizeof( sentry_ev_t ) ); \
        if( e ){ \
            /* set metatable */ \
            lstate_setmetatable( L, SENTRY_EVENT_MT ); \
            e->s = s; \
            e->ctx = ctx; \
            EV_SET( \
                &e->reg, \
                (uintptr_t)fd, \
                EVFILT_##type, \
                EV_ADD|( oneshot ? EV_ONESHOT : 0 )|( edge ? EV_CLEAR : 0 ), \
                0, \
                0, \
                (void*)e \
            ); \
            if( sentry_register( s, e ) == 0 ){ \
                e->ref = lstate_refat( L, -1 ); \
                return 0; \
            } \
            /* remove event */ \
            lua_pop( L, 1 ); \
        } \
        fddelset( &s->fds, fd, FDSET_##type ); \
    } \
    return -1; \
}while(0)


static inline int sev_writable_new( lua_State *L, sentry_t *s, int ctx, int fd,
                                    int oneshot, int edge )
{
    sev_fd_new( L, s, ctx, fd, WRITE, oneshot, edge );
}

static inline int sev_readable_new( lua_State *L, sentry_t *s, int ctx, int fd,
                                    int oneshot, int edge )
{
    sev_fd_new( L, s, ctx, fd, READ, oneshot, edge );
}


static inline int sev_signal_new( lua_State *L, sentry_t *s, int ctx, int signo,
                                  int oneshot )
{
    sentry_ev_t *e = NULL;
    
    // already watched
    if( sigismember( &s->signals, signo ) ){
        errno = EALREADY;
    }
    // create event
    else if( ( e = lua_newuserdata( L, sizeof( sentry_ev_t ) ) ) )
    {
        // set metatable
        lstate_setmetatable( L, SENTRY_EVENT_MT );
        e->s = s;
        e->ctx = ctx;
        // set event fields
        EV_SET(
            &e->reg,
            (uintptr_t)signo,
            EVFILT_SIGNAL,
            EV_ADD|( oneshot ? EV_ONESHOT : 0 ),
            0,
            0,
            (void*)e
        );
        if( sentry_register( s, e ) == 0 ){
            sigaddset( &s->signals, signo );
            e->ref = lstate_refat( L, -1 );
            return 0;
        }
        // remove event
        lua_pop( L, 1 );
    }
    
    return -1;
}


static inline int sev_timer_new( lua_State *L, sentry_t *s, int ctx,
                                 double timeout, int oneshot )
{
    // create event
    sentry_ev_t *e = lua_newuserdata( L, sizeof( sentry_ev_t ) );

    if( e )
    {
        // set metatable
        lstate_setmetatable( L, SENTRY_EVENT_MT );
        e->s = s;
        e->ctx = ctx;
        // set event fields
        EV_SET(
            &e->reg,
            (uintptr_t)e,
            EVFILT_TIMER,
            EV_ADD|( oneshot ? EV_ONESHOT : 0 ),
            NOTE_NSECONDS,
            (intptr_t)(timeout * 1000000000.0),
            (void*)e
        );
        if( sentry_register( s, e ) == 0 ){
            e->ref = lstate_refat( L, -1 );
            return 0;
        }
        // remove event
        lua_pop( L, 1 );
    }

    return -1;
}


#endif
