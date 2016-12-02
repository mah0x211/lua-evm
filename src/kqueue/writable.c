/*
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
 *  kqueue/writable.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 2016/01/27.
 *
 */

#include "sentry_event.h"


static int unwatch_lua( lua_State *L )
{
    sentry_ev_t *e = NULL;
    int rc = sev_unwatch_lua( L, SENTRY_WRITABLE_MT, &e );

    // del fd from fdset
    if( e ){
        fddelset( &e->s->fds, e->reg.ident, FDSET_WRITE );
    }

    return rc;
}


static int watch_lua( lua_State *L )
{
    sentry_ev_t *e = NULL;
    int rc = sev_watch_lua( L, SENTRY_WRITABLE_MT, &e );

    // add fd to fdset
    if( e ){
        fdaddset( &e->s->fds, e->reg.ident, FDSET_WRITE );
    }

    return rc;
}


static int ident_lua( lua_State *L )
{
    return sev_ident_lua( L, SENTRY_WRITABLE_MT );
}


static int context_lua( lua_State *L )
{
    return sev_context_lua( L, SENTRY_WRITABLE_MT );
}


static int typeof_lua( lua_State *L )
{
    return sev_typeof_lua( L, SENTRY_WRITABLE_MT );
}


static int revert_lua( lua_State *L )
{
    unwatch_lua( L );
    sev_gc_lua( L );
    return sev_revert_lua( L );
}


static int tostring_lua( lua_State *L )
{
    return TOSTRING_MT( L, SENTRY_WRITABLE_MT );
}


LUALIB_API int luaopen_sentry_writable( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", sev_gc_lua },
        { "__tostring", tostring_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        { "revert", revert_lua },
        { "ident", ident_lua },
        { "typeof", typeof_lua },
        { "context", context_lua },
        { "watch", watch_lua },
        { "unwatch", unwatch_lua },
        { NULL, NULL }
    };

    sentry_define_mt( L, SENTRY_WRITABLE_MT, mmethod, method );

    return 0;
}

