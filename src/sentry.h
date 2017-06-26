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
 *  sentry.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef SENTRY_LUA_H
#define SENTRY_LUA_H

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
// lualib
#include <lauxlib.h>
#include <lualib.h>

#include "lauxhlib.h"
// sentry headers
#include "config.h"
#include "sentry_types.h"
#include "fdset.h"


struct sentry_st {
    int fd;
    int nbuf;
    int nreg;
    int nevt;
    sigset_t signals;
    fdset_t fds;
    kevt_t *evs;
};


// memory alloc/dealloc
#define palloc(t)       (t*)malloc( sizeof(t) )
#define pnalloc(n,t)    (t*)malloc( (n) * sizeof(t) )
#define pcalloc(t)      (t*)calloc( 1, sizeof(t) )
#define pcnalloc(n,t)   (t*)calloc( n, sizeof(t) )
#define prealloc(n,t,p) (t*)realloc( p, (n) * sizeof(t) )
#define pdealloc(p)     free((void*)p)


// print message to stdout
#define plog(fmt,...) \
    printf( fmt "\n", ##__VA_ARGS__ )

#define pflog(f,fmt,...) \
    printf( #f "(): " fmt "\n", ##__VA_ARGS__ )

// print message to stderr
#define pelog(fmt,...) \
    fprintf( stderr, fmt "\n", ##__VA_ARGS__ )

#define pfelog(f,fmt,...) \
    fprintf( stderr, #f "(): " fmt "\n", ##__VA_ARGS__ )

// print message to stderr with strerror
#define pelogerr(fmt,...) \
    fprintf( stderr, fmt " : %s\n", ##__VA_ARGS__, strerror(errno) )

#define pfelogerr(f,fmt,...) \
    fprintf( stderr, #f "(): " fmt " : %s\n", ##__VA_ARGS__, strerror(errno) )


// helper macros
#define MSTRCAT(_msg)  #_msg


// helper macros for lua_State
#define TOSTRING_MT(L,tname) ({ \
    lua_pushfstring( L, tname ": %p", lua_touserdata( L, 1 ) ); \
    1; \
})


// define module names
#define SENTRY_MT           "sentry"
#define SENTRY_EVENT_MT     "sentry.event"
#define SENTRY_READABLE_MT  "sentry.readable"
#define SENTRY_WRITABLE_MT  "sentry.writable"
#define SENTRY_TIMER_MT     "sentry.timer"
#define SENTRY_SIGNAL_MT    "sentry.signal"


// define prototypes
LUALIB_API int luaopen_sentry( lua_State *L );
LUALIB_API int luaopen_sentry_event( lua_State *L );
LUALIB_API int luaopen_sentry_readable( lua_State *L );
LUALIB_API int luaopen_sentry_writable( lua_State *L );
LUALIB_API int luaopen_sentry_timer( lua_State *L );
LUALIB_API int luaopen_sentry_signal( lua_State *L );


// helper functions

// module definition register
static inline int sentry_define_mt( lua_State *L, const char *tname,
                                    struct luaL_Reg mmethod[],
                                    struct luaL_Reg method[] )
{
    struct luaL_Reg *ptr = mmethod;

    // create table __metatable
    luaL_newmetatable( L, tname );
    // metamethods
    while( ptr->name ){
        lauxh_pushfn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }

    // methods
    ptr = method;
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    while( ptr->name ){
        lauxh_pushfn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    lua_rawset( L, -3 );
    lua_pop( L, 1 );

    return 1;
}


static inline int sentry_increase_evs( sentry_t *s, uint8_t incr )
{
    // no buffer
    if( ( INT_MAX - s->nreg - incr ) <= 0 ){
        errno = ENOBUFS;
        return -1;
    }
    // realloc event container
    else if( ( s->nreg + incr ) > s->nbuf )
    {
        kevt_t *evs = prealloc( (size_t)s->nreg + incr, kevt_t, s->evs );

        if( !evs ){
            return -1;
        }
        s->nbuf = s->nreg + incr;
        s->evs = evs;
    }

    return 0;
}


static inline int sentry_retain_context( lua_State *L, int idx )
{
    int ctx = lauxh_refat( L, idx );

    if( ctx == LUA_REFNIL ){
        return luaL_argerror( L, idx, "could not retain a context" );
    }

    return ctx;
}


enum {
    SENTRY_EV_READABLE = 1,
    SENTRY_EV_WRITABLE,
    SENTRY_EV_TIMER,
    SENTRY_EV_SIGNAL
};


static inline int sev_type( sentry_ev_t *e )
{
    switch( sev_filter( e ) ){
        case EVFILT_READ:
            return SENTRY_EV_READABLE;

        case EVFILT_WRITE:
            return SENTRY_EV_WRITABLE;

        case EVFILT_TIMER:
            return SENTRY_EV_TIMER;

        case EVFILT_SIGNAL:
            return SENTRY_EV_SIGNAL;

        // unknown event
        default:
            return -1;
    }
}


static inline int sev_typeof_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    lua_pushinteger( L, sev_type( e ) );

    return 1;
}


static inline int sev_asa_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    switch( sev_filter( e ) ){
        case EVFILT_READ:
            lua_pushliteral( L, "asreadable" );
            return 1;

        case EVFILT_WRITE:
            lua_pushliteral( L, "aswritable" );
            return 1;

        case EVFILT_TIMER:
            lua_pushliteral( L, "astimer" );
            return 1;

        case EVFILT_SIGNAL:
            lua_pushliteral( L, "assignal" );
            return 1;

            // unknown event
        default:
            lua_pushnil( L );
            return 1;
    }
}


static inline int sev_context_lua( lua_State *L, const char *mt )
{
    sentry_ev_t *e = luaL_checkudata( L, 1, mt );

    if( lauxh_isref( e->ctx ) ){
        lauxh_pushref( L, e->ctx );
        return 1;
    }

    return 0;
}


static inline int sev_revert_lua( lua_State *L )
{
    lua_settop( L, 1 );
    // set event metatable
    lauxh_setmetatable( L, SENTRY_EVENT_MT );

    return 1;
}


// implemented at <epoll or kqueue>/common.c
int sev_gc_lua( lua_State *L );


#endif

