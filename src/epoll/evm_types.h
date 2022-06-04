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
 *  epoll/evm_types.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef evm_lua_types_h
#define evm_lua_types_h

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

// kernel event-loop fd creator
#if HAVE_EPOLL_CREATE1
# define evm_createfd() epoll_create1(EPOLL_CLOEXEC)
#else
# define evm_createfd() epoll_create(1)
#endif

// kernel event structure
typedef struct epoll_event kevt_t;

typedef struct evm_st evm_t;

enum {
    EVFILT_READ  = EPOLLIN,
    EVFILT_WRITE = EPOLLOUT,
    EVFILT_TIMER,
    EVFILT_SIGNAL
};

typedef struct {
    uintptr_t ident;
    evm_t *s;
    kevt_t reg;
    kevt_t evt;
    int filter;
    int ref;
    int ctx;
} evm_ev_t;

#define evm_ev_filter(e) ((e)->filter)

#endif
