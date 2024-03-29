/**
 *  Copyright (C) 2016 Masatoshi Teruya
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
 *  event.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 2016/02/28.
 *
 */

#include "evm_event.h"

typedef int (*fd_initializer)(evm_ev_t *e, int fd, int oneshot, int edge);

static int asfd_lua(lua_State *L, fd_initializer proc, const char *mt,
                    const char *op)
{
    int argc    = lua_gettop(L);
    evm_ev_t *e = luaL_checkudata(L, 1, EVM_EVENT_MT);
    int fd      = (int)lauxh_checkinteger(L, 2);
    int ctx     = LUA_NOREF;
    int oneshot = 0;
    int edge    = 0;

    // check arguments
    if (argc > 5) {
        argc = 5;
    }
    switch (argc) {
    // arg#5 edge-trigger (default level-trigger)
    case 5:
        edge = lauxh_optboolean(L, 5, edge);
    case 4:
        // arg#4 oneshot
        oneshot = lauxh_optboolean(L, 4, oneshot);
    case 3:
        // arg#3 context
        if (!lua_isnoneornil(L, 3)) {
            ctx = evm_retain_context(L, 3);
        }
    case 2:
        // arg#2 descriptor
        if (fd < 0 || fd > INT_MAX) {
            return luaL_argerror(
                L, 2, "fd value range must be 0 to " MSTRCAT(INT_MAX));
        }
        break;
    }

    // set out-event
    if (proc(e, fd, oneshot, edge) == 0) {
        e->ctx = ctx;
        lua_settop(L, 1);
        // set metatable
        lauxh_setmetatable(L, mt);
        e->ref = lauxh_ref(L);
        lua_pushboolean(L, 1);
        return 1;
    }

    // got error
    lauxh_unref(L, ctx);
    lua_pushboolean(L, 0);
    lua_errno_new(L, errno, op);
    return 2;
}

static int aswritable_lua(lua_State *L)
{
    return asfd_lua(L, evm_ev_as_writable, EVM_WRITABLE_MT, "aswritable");
}

static int asreadable_lua(lua_State *L)
{
    return asfd_lua(L, evm_ev_as_readable, EVM_READABLE_MT, "asreadable");
}

static int assignal_lua(lua_State *L)
{
    int argc    = lua_gettop(L);
    evm_ev_t *e = luaL_checkudata(L, 1, EVM_EVENT_MT);
    int signo   = (int)lauxh_checkinteger(L, 2);
    int ctx     = LUA_NOREF;
    int oneshot = 0;

    // check arguments
    if (argc > 4) {
        argc = 4;
    }

    switch (argc) {
    case 4:
        // arg#4 oneshot
        oneshot = lauxh_optboolean(L, 4, oneshot);
    case 3:
        // arg#3 context
        if (!lua_isnoneornil(L, 3)) {
            ctx = evm_retain_context(L, 3);
        }
    case 2:
        // arg#2 signo
        if (signo <= 0 || signo >= NSIG) {
            return luaL_argerror(L, 2, "invalid signal number");
        }
        break;
    }

    // set signal-event
    if (evm_ev_as_signal(e, signo, oneshot) == 0) {
        e->ctx = ctx;
        lua_settop(L, 1);
        // set signal metatable
        lauxh_setmetatable(L, EVM_SIGNAL_MT);
        e->ref = lauxh_ref(L);
        lua_pushboolean(L, 1);
        return 1;
    }

    // got error
    lauxh_unref(L, ctx);
    lua_pushboolean(L, 0);
    lua_errno_new(L, errno, "assignal");
    return 2;
}

static int astimer_lua(lua_State *L)
{
    int argc            = lua_gettop(L);
    evm_ev_t *e         = luaL_checkudata(L, 1, EVM_EVENT_MT);
    lua_Integer timeout = lauxh_checkinteger(L, 2);
    int ctx             = LUA_NOREF;
    int oneshot         = 0;

    // check arguments
    if (argc > 4) {
        argc = 4;
    }
    switch (argc) {
    case 4:
        // arg#4 oneshot
        oneshot = lauxh_optboolean(L, 4, oneshot);
    case 3:
        // arg#3 context
        if (!lua_isnoneornil(L, 3)) {
            ctx = evm_retain_context(L, 3);
        }
    case 2:
        // arg#2 timeout
        if (timeout <= 0) {
            return luaL_argerror(
                L, 2, "timeout must be greater than or equal to 0 msec");
        }
        break;
    }

    // create timer-event
    if (evm_ev_as_timer(e, timeout, oneshot) == 0) {
        e->ctx = ctx;
        lua_settop(L, 1);
        // set timer metatable
        lauxh_setmetatable(L, EVM_TIMER_MT);
        e->ref = lauxh_ref(L);
        lua_pushboolean(L, 1);
        return 1;
    }

    // got error
    lauxh_unref(L, ctx);
    lua_pushboolean(L, 0);
    lua_errno_new(L, errno, "astimer");
    return 2;
}

// common method
static int renew_lua(lua_State *L)
{
    evm_ev_t *e = luaL_checkudata(L, 1, EVM_EVENT_MT);
    evm_t *s    = lauxh_optudata(L, 2, EVM_MT, NULL);

    if (s) {
        e->s = s;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int revert_lua(lua_State *L)
{
    (void)L;
    return 1;
}

static int tostring_lua(lua_State *L)
{
    return TOSTRING_MT(L, EVM_EVENT_MT);
}

LUALIB_API int luaopen_evm_event(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__tostring", tostring_lua},
        {NULL,         NULL        }
    };
    struct luaL_Reg method[] = {
        {"revert",     revert_lua    },
        {"renew",      renew_lua     },
        {"astimer",    astimer_lua   },
        {"assignal",   assignal_lua  },
        {"asreadable", asreadable_lua},
        {"aswritable", aswritable_lua},
        {NULL,         NULL          }
    };

    evm_define_mt(L, EVM_EVENT_MT, mmethod, method);

    return 0;
}
