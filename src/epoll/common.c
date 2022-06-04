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
 *  epoll/common.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 2016/01/27.
 *
 */

#include "evm_event.h"

int evm_ev_gc_lua(lua_State *L)
{
    evm_ev_t *e = lua_touserdata(L, 1);

    // close descriptor
    close(e->reg.data.fd);

    // release context
    e->ctx = lauxh_unref(L, e->ctx);

    return 0;
}

int evm_ev_rwgc_lua(lua_State *L)
{
    evm_ev_t *e = lua_touserdata(L, 1);

    // close descriptor
    if ((int)e->ident != e->reg.data.fd) {
        close(e->reg.data.fd);
    }

    // release context
    e->ctx = lauxh_unref(L, e->ctx);

    return 0;
}
