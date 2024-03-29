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
 *  epoll/evm_event.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 *
 */

#ifndef evm_epoll_event_h
#define evm_epoll_event_h

#include "evm.h"

static inline int evm_wait(evm_t *s, lua_Integer timeout)
{
    return epoll_wait(s->fd, s->evs, s->nreg, timeout);
}

static inline evm_ev_t *evm_getev(evm_t *s, int *isdel)
{
    static uint8_t drain[sizeof(struct signalfd_siginfo)];
    evm_ev_t *e = NULL;
    kevt_t *evt = NULL;
    int delflg  = 0;

CHECK_NEXT:
    if (s->nevt > 0) {
        evt = &s->evs[--s->nevt];
        // fetch evm_ev_t from fdset
        if (!(e = (evm_ev_t *)fdismember(&s->fds, evt->data.fd))) {
            goto CHECK_NEXT;
        }

        e->evt = *evt;
        delflg = (e->reg.events & EPOLLONESHOT) |
                 (e->evt.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR));
        switch (e->filter) {
        // drain data
        case EVFILT_SIGNAL:
            (void)read(evt->data.fd, drain, sizeof(struct signalfd_siginfo));
            // remove from sigset
            if (delflg) {
                sigdelset(&e->s->signals, e->ident);
                goto DELETE_EVENT;
            }
            break;
        case EVFILT_TIMER:
            (void)read(evt->data.fd, drain, sizeof(uint64_t));
            break;
        }

        // remove from kernel event
        if (delflg) {
DELETE_EVENT:
            *isdel = delflg;
            epoll_ctl(e->s->fd, EPOLL_CTL_DEL, e->reg.data.fd, NULL);
            fddelset(&e->s->fds, e->reg.data.fd);
        }
    }

    return e;
}

static inline int evm_register(evm_ev_t *e)
{
    // increase event-buffer and set event
    if (evm_increase_evs(e->s, 1) == 0 &&
        fdset_realloc(&e->s->fds, e->reg.data.fd) == 0 &&
        epoll_ctl(e->s->fd, EPOLL_CTL_ADD, e->reg.data.fd, &e->reg) == 0) {
        fdaddset(&e->s->fds, e->reg.data.fd, (void *)e);
        e->s->nreg++;
        return 0;
    }

    return -1;
}

// MARK: API for evm_ev_t

static inline int evm_ev_as_fd(evm_ev_t *e, int fd, int oneshot, int edge,
                               int filter)
{
    evm_ev_t *sibling = fdismember(&e->s->fds, fd);
    kevt_t evt = {.events = filter | EPOLLRDHUP | (oneshot ? EPOLLONESHOT : 0) |
                            (edge ? EPOLLET : 0),
                  .data = {.fd = fd}};

    // already watched
    if (sibling) {
        if (sibling->filter == filter) {
            errno = EALREADY;
            return -1;
        }
        // duplicate descriptor
        else if ((evt.data.fd = dup(fd)) == -1) {
            return -1;
        }
    }

    // set event fields
    e->ident  = fd;
    e->filter = filter;
    e->reg    = evt;
    if (evm_register(e) == 0) {
        return 0;
    }

    // close duplicated
    if (evt.data.fd != fd) {
        close(evt.data.fd);
    }

    return -1;
}

static inline int evm_ev_as_readable(evm_ev_t *e, int fd, int oneshot, int edge)
{
    return evm_ev_as_fd(e, fd, oneshot, edge, EVFILT_READ);
}

static inline int evm_ev_as_writable(evm_ev_t *e, int fd, int oneshot, int edge)
{
    return evm_ev_as_fd(e, fd, oneshot, edge, EVFILT_WRITE);
}

static inline int evm_ev_as_signal(evm_ev_t *e, int signo, int oneshot)
{
    // already watched
    if (sigismember(&e->s->signals, signo)) {
        errno = EALREADY;
    } else {
        int fd = 0;
        sigset_t ss;

        // init sigset with signo
        sigemptyset(&ss);
        sigaddset(&ss, signo);

        // create event and signalfd with sigset_t
        if ((fd = signalfd(-1, &ss, SFD_NONBLOCK | SFD_CLOEXEC)) != -1) {
            // set event fields
            e->ident       = signo;
            e->filter      = EVFILT_SIGNAL;
            e->reg.data.fd = fd;
            e->reg.events = EPOLLRDHUP | EPOLLIN | (oneshot ? EPOLLONESHOT : 0);
            // register to evm
            if (evm_register(e) == 0) {
                sigaddset(&e->s->signals, signo);
                return 0;
            }

            // close fd
            close(fd);
        }
    }

    return -1;
}

static inline int evm_ev_as_timer(evm_ev_t *e, lua_Integer timeout, int oneshot)
{
    // create timerfd
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if (fd) {
        struct itimerspec its;

        // convert msec to interval-timespec
        its.it_interval =
            (struct timespec){.tv_sec  = (time_t)timeout / 1000,
                              .tv_nsec = (long)((timeout % 1000) * 1000000)};
        // set first invocation time
        its.it_value = its.it_interval;

        // set event fields
        e->ident       = (uintptr_t)timeout;
        e->filter      = EVFILT_TIMER;
        e->reg.data.fd = fd;
        e->reg.events  = EPOLLRDHUP | EPOLLIN | (oneshot ? EPOLLONESHOT : 0);

        // set timespec and register to evm
        if (timerfd_settime(fd, 0, &its, NULL) == 0 && evm_register(e) == 0) {
            return 0;
        }

        // close fd
        close(fd);
    }

    return -1;
}

static inline int evm_ev_is_oneshot(evm_ev_t *e)
{
    return e->reg.events & EPOLLONESHOT;
}

static inline int evm_ev_is_hup(evm_ev_t *e)
{
    return e->evt.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
}

static inline int evm_ev_ident_lua(lua_State *L, const char *mt)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);

    lua_pushinteger(L, e->ident);

    return 1;
}

static inline int evm_ev_watch_lua(lua_State *L, const char *mt, evm_ev_t **ev)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);

    if (!lauxh_isref(e->ref)) {
        // register event
        if (evm_register(e) != 0) {
            // got error
            lua_pushboolean(L, 0);
            lua_errno_new(L, errno, "watch");
            return 2;
        }

        // retain event
        lua_settop(L, 1);
        e->ref = lauxh_ref(L);
        if (ev) {
            *ev = e;
        }
    }

    lua_pushboolean(L, 1);

    return 1;
}

static inline int evm_ev_unwatch_lua(lua_State *L, const char *mt,
                                     evm_ev_t **ev)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);

    if (lauxh_isref(e->ref)) {
        struct epoll_event evt = e->reg;

        // del fd from fdset
        fddelset(&e->s->fds, e->reg.data.fd);
        // unregister event
        epoll_ctl(e->s->fd, EPOLL_CTL_DEL, e->reg.data.fd, &evt);
        e->s->nreg--;
        e->ref = lauxh_unref(L, e->ref);
        if (ev) {
            *ev = e;
        }
    }

    lua_pushboolean(L, 1);

    return 1;
}

// implemented at epoll/common.c

// gc for readable/writable event
int evm_ev_rwgc_lua(lua_State *L);

#endif
