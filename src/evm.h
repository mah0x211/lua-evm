/**
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
 *  evm.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef evm_lua_h
#define evm_lua_h

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// lualib
#include <lauxlib.h>
#include <lualib.h>

#include "lauxhlib.h"
// evm headers
#include "config.h"
#include "evm_types.h"
#include "fdset.h"

struct evm_st {
    int fd;
    int nbuf;
    int nreg;
    int nevt;
    sigset_t signals;
    fdset_t fds;
    kevt_t *evs;
};

// memory alloc/dealloc
#define palloc(t)         (t *)malloc(sizeof(t))
#define pnalloc(n, t)     (t *)malloc((n) * sizeof(t))
#define pcalloc(t)        (t *)calloc(1, sizeof(t))
#define pcnalloc(n, t)    (t *)calloc(n, sizeof(t))
#define prealloc(n, t, p) (t *)realloc(p, (n) * sizeof(t))
#define pdealloc(p)       free((void *)p)

// print message to stdout
#define plog(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define pflog(f, fmt, ...) printf(#f "(): " fmt "\n", ##__VA_ARGS__)

// print message to stderr
#define pelog(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#define pfelog(f, fmt, ...) fprintf(stderr, #f "(): " fmt "\n", ##__VA_ARGS__)

// print message to stderr with strerror
#define pelogerr(fmt, ...)                                                     \
 fprintf(stderr, fmt " : %s\n", ##__VA_ARGS__, strerror(errno))

#define pfelogerr(f, fmt, ...)                                                 \
 fprintf(stderr, #f "(): " fmt " : %s\n", ##__VA_ARGS__, strerror(errno))

// helper macros
#define MSTRCAT(_msg) #_msg

// helper macros for lua_State
#define TOSTRING_MT(L, tname)                                                  \
 (                                                                             \
     {                                                                         \
lua_pushfstring(L, tname ": %p", lua_touserdata(L, 1));                        \
1;                                                                             \
     })

// define module names
#define EVM_MT          "evm"
#define EVM_EVENT_MT    "evm.event"
#define EVM_READABLE_MT "evm.readable"
#define EVM_WRITABLE_MT "evm.writable"
#define EVM_TIMER_MT    "evm.timer"
#define EVM_SIGNAL_MT   "evm.signal"

// define prototypes
LUALIB_API int luaopen_evm(lua_State *L);
LUALIB_API int luaopen_evm_event(lua_State *L);
LUALIB_API int luaopen_evm_readable(lua_State *L);
LUALIB_API int luaopen_evm_writable(lua_State *L);
LUALIB_API int luaopen_evm_timer(lua_State *L);
LUALIB_API int luaopen_evm_signal(lua_State *L);

// helper functions

// module definition register
static inline int evm_define_mt(lua_State *L, const char *tname,
                                struct luaL_Reg mmethod[],
                                struct luaL_Reg method[])
{
    struct luaL_Reg *ptr = mmethod;

    // create table __metatable
    luaL_newmetatable(L, tname);
    // metamethods
    while (ptr->name) {
        lauxh_pushfn2tbl(L, ptr->name, ptr->func);
        ptr++;
    }

    // methods
    ptr = method;
    lua_pushstring(L, "__index");
    lua_newtable(L);
    while (ptr->name) {
        lauxh_pushfn2tbl(L, ptr->name, ptr->func);
        ptr++;
    }
    lua_rawset(L, -3);
    lua_pop(L, 1);

    return 1;
}

static inline int evm_increase_evs(evm_t *s, uint8_t incr)
{
    // no buffer
    if ((INT_MAX - s->nreg - incr) <= 0) {
        errno = ENOBUFS;
        return -1;
    }
    // realloc event container
    else if ((s->nreg + incr) > s->nbuf) {
        kevt_t *evs = prealloc((size_t)s->nreg + incr, kevt_t, s->evs);

        if (!evs) {
            return -1;
        }
        s->nbuf = s->nreg + incr;
        s->evs  = evs;
    }

    return 0;
}

static inline int evm_retain_context(lua_State *L, int idx)
{
    int ctx = lauxh_refat(L, idx);

    if (ctx == LUA_REFNIL) {
        return luaL_argerror(L, idx, "could not retain a context");
    }

    return ctx;
}

static inline int sev_asa_lua(lua_State *L, const char *mt)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);

    switch (sev_filter(e)) {
    case EVFILT_READ:
        lua_pushliteral(L, "asreadable");
        return 1;

    case EVFILT_WRITE:
        lua_pushliteral(L, "aswritable");
        return 1;

    case EVFILT_TIMER:
        lua_pushliteral(L, "astimer");
        return 1;

    case EVFILT_SIGNAL:
        lua_pushliteral(L, "assignal");
        return 1;

        // unknown event
    default:
        lua_pushnil(L);
        return 1;
    }
}

static inline int sev_context_lua(lua_State *L, const char *mt)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);
    int ctx     = LUA_NOREF;

    // check passed argument
    if (lua_gettop(L) > 1) {
        // evaluate as nil
        if (lua_isnoneornil(L, 2)) {
            lua_settop(L, 0);
            ctx = LUA_REFNIL;
        } else {
            lua_settop(L, 2);
            ctx = evm_retain_context(L, 2);
        }
    }

    if (lauxh_isref(e->ctx)) {
        lauxh_pushref(L, e->ctx);
        // replace current context with passed argument
        if (ctx != LUA_NOREF) {
            lauxh_unref(L, e->ctx);
            e->ctx = ctx;
        }
    }
    // replace current context with passed argument
    else {
        lua_pushnil(L);
        e->ctx = ctx;
    }

    return 1;
}

static inline int sev_revert_lua(lua_State *L)
{
    lua_settop(L, 1);
    // set event metatable
    lauxh_setmetatable(L, EVM_EVENT_MT);

    return 1;
}

// implemented at <epoll or kqueue>/common.c
int sev_gc_lua(lua_State *L);

#endif
