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
 *  evm.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#include "evm_event.h"

static pid_t EVM_PID   = -1;
static int DEFAULT_EVM = LUA_NOREF;

static int wait_lua(lua_State *L)
{
    evm_t *s            = luaL_checkudata(L, 1, EVM_MT);
    // default timeout: -1(never timeout)
    lua_Integer timeout = lauxh_optinteger(L, 2, -1);
    evm_ev_t *e         = NULL;
    int isdel           = 0;

    // check arguments
    // cleanup current events
    while ((e = evm_getev(s, &isdel))) {
        if (isdel) {
            isdel  = 0;
            e->ref = lauxh_unref(L, e->ref);
            s->nreg--;
        }
    }

    // wait event
    s->nevt = evm_wait(s, timeout);
    // got errno
    if (s->nevt == -1) {
        switch (errno) {
        // ignore error
        case ENOENT:
        case EINTR:
            s->nevt = 0;
            errno   = 0;
            break;

        // return error
        default:
            lua_pushinteger(L, 0);
            lua_pushstring(L, strerror(errno));
            return 2;
        }
    }

    // return number of event
    lua_pushinteger(L, s->nevt);

    return 1;
}

static int getevent_lua(lua_State *L)
{
    evm_t *s    = luaL_checkudata(L, 1, EVM_MT);
    int isdel   = 0;
    evm_ev_t *e = evm_getev(s, &isdel);

    // return event, isdel and context
    if (e) {
        lauxh_pushref(L, e->ref);
        // push context if retained
        if (lauxh_isref(e->ctx)) {
            lauxh_pushref(L, e->ctx);
        } else {
            lua_pushnil(L);
        }

        // release reference if deleted
        if (isdel) {
            lua_pushboolean(L, isdel);
            e->ref = lauxh_unref(L, e->ref);
            s->nreg--;
            return 3;
        }

        return 2;
    }

    return 0;
}

static inline void allocevent(lua_State *L, evm_t *s)
{
    evm_ev_t *e = lua_newuserdata(L, sizeof(evm_ev_t));

    // clear
    memset((void *)e, 0, sizeof(evm_ev_t));
    e->s   = s;
    e->ctx = LUA_NOREF;
    e->ref = LUA_NOREF;
    // set metatable
    lauxh_setmetatable(L, EVM_EVENT_MT);
}

static int newevents_lua(lua_State *L)
{
    evm_t *s = luaL_checkudata(L, 1, EVM_MT);
    int nevt = (int)lauxh_optinteger(L, 2, 1);

    if (nevt > 0) {
        int i = 0;

        lua_settop(L, 0);
        lua_createtable(L, nevt, 0);
        for (; i < nevt; i++) {
            allocevent(L, s);
            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    lua_settop(L, 0);
    lua_pushnil(L);
    lua_pushstring(L, strerror(EINVAL));

    return 2;
}

static int newevent_lua(lua_State *L)
{
    evm_t *s = luaL_checkudata(L, 1, EVM_MT);

    allocevent(L, s);
    return 1;
}

static int renew_lua(lua_State *L)
{
    evm_t *s = luaL_checkudata(L, 1, EVM_MT);
    int fd   = evm_createfd();

    if (fd != -1) {
        // close unused descriptor
        if (s->fd != fd) {
            close(s->fd);
        }
        s->fd = fd;
        lua_pushboolean(L, 1);

        return 1;
    }

    // got error
    lua_pushboolean(L, 0);
    lua_pushstring(L, strerror(errno));

    return 2;
}

static int len_lua(lua_State *L)
{
    evm_t *s = luaL_checkudata(L, 1, EVM_MT);

    lua_pushinteger(L, s->nreg);

    return 1;
}

static int tostring_lua(lua_State *L)
{
    return TOSTRING_MT(L, EVM_MT);
}

static int gc_lua(lua_State *L)
{
    evm_t *s = lua_touserdata(L, 1);

    // close if not invalid value
    if (s->fd != -1) {
        close(s->fd);
    }
    pdealloc(s->evs);
    fdset_dealloc(&s->fds);

    return 0;
}

// allocate evm data
static int new_lua(lua_State *L)
{
    evm_t *s = NULL;
    int nbuf = 128;

    // check arguments
    // arg#1 number of event buffer size
    if (!lua_isnoneornil(L, 1)) {
        nbuf = (int)lauxh_checkinteger(L, 1);
        if (nbuf < 1 || nbuf > INT_MAX) {
            return luaL_error(L, "event buffer value range must be 1 to %d",
                              INT_MAX);
        }
    }

    // create and init evm_t
    s = lua_newuserdata(L, sizeof(evm_t));
    if ((s->evs = pnalloc((size_t)nbuf, kevt_t))) {
        if (fdset_alloc(&s->fds, (size_t)nbuf) == 0) {
            // create event descriptor
            if ((s->fd = evm_createfd()) != -1) {
                lauxh_setmetatable(L, EVM_MT);
                s->nbuf = nbuf;
                s->nreg = 0;
                s->nevt = 0;
                sigemptyset(&s->signals);

                return 1;
            }
            fdset_dealloc(&s->fds);
        }
        pdealloc(s->evs);
    }

    // got error
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));

    return 2;
}

// create default evm
static int default_lua(lua_State *L)
{
    if (lauxh_isref(DEFAULT_EVM)) {
        lauxh_pushref(L, DEFAULT_EVM);
        // return current default evm
        if (EVM_PID == getpid()) {
            return 1;
        }
        // create new default evm
        else {
            evm_t *s = luaL_checkudata(L, -1, EVM_MT);

            // should close event descriptor
            close(s->fd);
            // invalid value
            s->fd       = -1;
            // release reference
            DEFAULT_EVM = lauxh_unref(L, DEFAULT_EVM);
            lua_pop(L, 1);
        }
    }

    switch (new_lua(L)) {
    case 1:
        DEFAULT_EVM = lauxh_refat(L, -1);
        EVM_PID     = getpid();
        return 1;

    default:
        return 2;
    }
}

LUALIB_API int luaopen_evm(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__gc",       gc_lua      },
        {"__tostring", tostring_lua},
        {"__len",      len_lua     },
        {NULL,         NULL        }
    };
    struct luaL_Reg method[] = {
        {"renew",     renew_lua    },
        {"newevent",  newevent_lua },
        {"newevents", newevents_lua},
        {"getevent",  getevent_lua },
        {"wait",      wait_lua     },
        {NULL,        NULL         }
    };

    // register event metatables
    luaopen_evm_event(L);
    luaopen_evm_readable(L);
    luaopen_evm_writable(L);
    luaopen_evm_timer(L);
    luaopen_evm_signal(L);

    // register evm-metatable
    evm_define_mt(L, EVM_MT, mmethod, method);
    // create table
    lua_newtable(L);
    lauxh_pushfn2tbl(L, "new", new_lua);
    lauxh_pushfn2tbl(L, "default", default_lua);

    return 1;
}
