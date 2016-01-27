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
// sentry headers
#include "config.h"
#include "fdset.h"
#include "sentry_types.h"


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
#define lstate_setmetatable(L,tname) \
    (luaL_getmetatable(L,tname),lua_setmetatable(L,-2))

#define lstate_ref(L) \
    luaL_ref(L,LUA_REGISTRYINDEX)

#define lstate_refat(L,idx) \
    (lua_pushvalue(L,idx),luaL_ref(L,LUA_REGISTRYINDEX))

#define lstate_isref(ref) \
    ((ref) >= 0)

#define lstate_pushref(L,ref) \
    lua_rawgeti( L, LUA_REGISTRYINDEX, ref )

#define lstate_unref(L,ref) \
    (luaL_unref( L, LUA_REGISTRYINDEX, ref ),LUA_NOREF)

#define lstate_fn2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushcfunction(L,v); \
    lua_rawset(L,-3); \
}while(0)

#define lstate_str2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushstring(L,v); \
    lua_rawset(L,-3); \
}while(0)

#define lstate_num2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushnumber(L,v); \
    lua_rawset(L,-3); \
}while(0)


#define TOSTRING_MT(L,tname) ({ \
    lua_pushfstring( L, tname ": %p", lua_touserdata( L, 1 ) ); \
    1; \
})


// define module names
#define SENTRY_MT       "sentry"
#define SENTRY_EVENT_MT "sentry.event"


// define prototypes
LUALIB_API int luaopen_sentry( lua_State *L );
LUALIB_API int luaopen_sentry_event( lua_State *L );


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
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    
    // methods
    ptr = method;
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    while( ptr->name ){
        lstate_fn2tbl( L, ptr->name, ptr->func );
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
    int ctx = lstate_refat( L, idx );
    
    if( ctx == LUA_REFNIL ){
        return luaL_argerror( L, idx, "could not retain a context" );
    }
    
    return ctx;
}


static inline void sentry_dbl2timespec( double tval, struct timespec *ts )
{
    double sec = 0, nsec = 0;
    
    nsec = modf( tval, &sec );
    ts->tv_sec = (time_t)sec;
    ts->tv_nsec = (long)(nsec * 1000000000);
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


#endif

