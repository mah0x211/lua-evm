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
 *  kqueue/timer.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 2016/01/27.
 *
 */

#include "evm_event.h"

static int unwatch_lua(lua_State *L)
{
    return evm_ev_unwatch_lua(L, EVM_TIMER_MT, NULL);
}

static int watch_lua(lua_State *L)
{
    return evm_ev_watch_lua(L, EVM_TIMER_MT, NULL);
}

static int context_lua(lua_State *L)
{
    return evm_ev_context_lua(L, EVM_TIMER_MT);
}

static int asa_lua(lua_State *L)
{
    return evm_asa_lua(L, EVM_TIMER_MT);
}

static int ident_lua(lua_State *L)
{
    evm_ev_t *e = luaL_checkudata(L, 1, EVM_TIMER_MT);

    lua_pushinteger(L, e->reg.data);

    return 1;
}

static int renew_lua(lua_State *L)
{
    evm_ev_t *e = luaL_checkudata(L, 1, EVM_TIMER_MT);
    evm_t *s    = lauxh_optudata(L, 2, EVM_MT, NULL);

    unwatch_lua(L);
    if (s) {
        e->s = s;
    }

    return watch_lua(L);
}

static int revert_lua(lua_State *L)
{
    unwatch_lua(L);
    evm_ev_gc_lua(L);
    return evm_ev_revert_lua(L);
}

static int tostring_lua(lua_State *L)
{
    return TOSTRING_MT(L, EVM_TIMER_MT);
}

LUALIB_API int luaopen_evm_timer(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__gc",       evm_ev_gc_lua},
        {"__tostring", tostring_lua },
        {NULL,         NULL         }
    };
    struct luaL_Reg method[] = {
        {"revert",  revert_lua },
        {"renew",   renew_lua  },
        {"ident",   ident_lua  },
        {"asa",     asa_lua    },
        {"context", context_lua},
        {"watch",   watch_lua  },
        {"unwatch", unwatch_lua},
        {NULL,      NULL       }
    };

    evm_define_mt(L, EVM_TIMER_MT, mmethod, method);

    return 0;
}
