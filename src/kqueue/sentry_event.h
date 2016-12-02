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
 *  kevent/sentry_event.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef SENTRY_KEVENT_EVENT_H
#define SENTRY_KEVENT_EVENT_H


#include "sentry.h"


static inline int sentry_wait( sentry_t *s, int timeout )
{
    struct timespec ts = {
        timeout, 0
    };
    struct timespec *tsp = ( timeout < 0 ) ? NULL : &ts;

    return kevent( s->fd, NULL, 0, s->evs, (int)s->nreg, tsp );
}


static inline sentry_ev_t *sentry_getev( sentry_t *s, int *isdel )
{
    sentry_ev_t *e = NULL;
    kevt_t *evt = NULL;
    int delflg = 0;

CHECK_NEXT:
    if( s->nevt > 0 )
    {
        evt = &s->evs[--s->nevt];
        delflg = evt->flags & (EV_ONESHOT|EV_EOF|EV_ERROR);

        switch( evt->filter )
        {
            case EVFILT_READ:
                if( fdismember( &s->fds, evt->ident, FDSET_READ ) != 1 ){
                    goto CHECK_NEXT;
                }
                else if( delflg ){
                    fddelset( &s->fds, evt->ident, FDSET_READ );
                }
            break;
            case EVFILT_WRITE:
                if( fdismember( &s->fds, evt->ident, FDSET_WRITE ) != 1 ){
                    goto CHECK_NEXT;
                }
                else if( delflg ){
                    fddelset( &s->fds, evt->ident, FDSET_WRITE );
                }
            break;
            case EVFILT_SIGNAL:
                if( !sigismember( &s->signals, evt->ident ) ){
                    goto CHECK_NEXT;
                }
                else if( delflg ){
                    sigdelset( &s->signals, evt->ident );
                }
            break;
        }

        // remove from kernel event
        if( delflg )
        {
            *isdel = delflg;
            // set errno
            if( delflg & EV_ERROR ){
                errno = evt->data;
            }
            evt->flags = EV_DELETE;
            // unregister if not oneshot event
            if( !( delflg & EV_ONESHOT ) ){
                kevent( s->fd, evt, 1, NULL, 0, NULL );
            }
        }

        e = (sentry_ev_t*)evt->udata;
        e->evt = *evt;
    }

    return e;
}


static inline int sentry_register( sentry_ev_t *e )
{
    // increase event-buffer and set event
    if( sentry_increase_evs( e->s, 1 ) == 0 &&
        kevent( e->s->fd, &e->reg, 1, NULL, 0, NULL ) == 0 ){
        e->s->nreg++;
        return 0;
    }

    return -1;
}


// MARK: API for sentry_ev_t


#define sev_asfd( e, fd, type, oneshot, edge ) do { \
    /* already watched */ \
    if( fdismember( &(e)->s->fds, (fd), FDSET_##type ) == 1 ){ \
        errno = EALREADY; \
    } \
    else if( fdset_realloc( &e->s->fds, fd ) == 0 && \
             fdaddset( &(e)->s->fds, (fd), FDSET_##type ) == 0 ){ \
        EV_SET( \
            &(e)->reg, \
            (uintptr_t)(fd), \
            EVFILT_##type, \
            EV_ADD|( (oneshot) ? EV_ONESHOT : 0 )|( (edge) ? EV_CLEAR : 0 ), \
            0, \
            0, \
            (void*)(e) \
        ); \
        if( sentry_register( e ) == 0 ){ \
            return 0; \
        } \
        fddelset( &(e)->s->fds, (fd), FDSET_##type ); \
    } \
    return -1; \
}while(0)


static inline int sev_aswritable( sentry_ev_t *e, int fd, int oneshot,
                                  int edge )
{
    sev_asfd( e, fd, WRITE, oneshot, edge );
}

static inline int sev_asreadable( sentry_ev_t *e, int fd, int oneshot,
                                  int edge )
{
    sev_asfd( e, fd, READ, oneshot, edge );
}


static inline int sev_assignal( sentry_ev_t *e, int signo, int oneshot )
{
    // already watched
    if( sigismember( &e->s->signals, signo ) ){
        errno = EALREADY;
        return -1;
    }

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

    if( sentry_register( e ) == 0 ){
        sigaddset( &e->s->signals, signo );
        return 0;
    }

    return -1;
}


static inline int sev_astimer( sentry_ev_t *e, double timeout, int oneshot )
{
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

    return sentry_register( e );
}


static inline int sev_is_oneshot( sentry_ev_t *e )
{
    return e->reg.flags & EV_ONESHOT;
}


static inline int sev_is_hup( sentry_ev_t *e )
{
    return e->evt.flags & (EV_EOF|EV_ERROR);
}


static inline int sev_ident_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    lua_pushinteger( L, e->reg.ident );

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
    sentry_ev_t *e = luaL_checkudata( (L), 1, (mt) );

    if( !lstate_isref( e->ref ) )
    {
        // register event
        if( sentry_register( e ) != 0 ){
            // got error
            lua_pushboolean( (L), 0 );
            lua_pushstring( (L), strerror( errno ) );
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

    if( lstate_isref( e->ref ) )
    {
        struct kevent evt = e->reg;

        // unregister event
        evt.flags = EV_DELETE;
        kevent( e->s->fd, &evt, 1, NULL, 0, NULL );
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


// implemented at kqueue/common.c
int sev_gc_lua( lua_State *L );

#endif
