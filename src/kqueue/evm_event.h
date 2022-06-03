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
 *  kevent/evm_event.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef evm_kevent_event_h
#define evm_kevent_event_h

#include "evm.h"

static inline int evm_wait(evm_t *s, lua_Integer timeout)
{
    if (timeout > -1) {
        struct timespec ts = {.tv_sec  = timeout / 1000,
                              .tv_nsec = (timeout % 1000) * 1000000};

        return kevent(s->fd, NULL, 0, s->evs, (int)s->nreg, &ts);
    }

    return kevent(s->fd, NULL, 0, s->evs, (int)s->nreg, NULL);
}

static inline evm_ev_t *evm_getev(evm_t *s, int *isdel)
{
    evm_ev_t *e = NULL;
    kevt_t *evt = NULL;
    int delflg  = 0;

CHECK_NEXT:
    if (s->nevt > 0) {
        evt    = &s->evs[--s->nevt];
        delflg = evt->flags & (EV_ONESHOT | EV_EOF | EV_ERROR);

        switch (evt->filter) {
        case EVFILT_READ:
            if (fdismember(&s->fds, evt->ident, FDSET_READ) != 1) {
                goto CHECK_NEXT;
            } else if (delflg) {
                fddelset(&s->fds, evt->ident, FDSET_READ);
            }
            break;
        case EVFILT_WRITE:
            if (fdismember(&s->fds, evt->ident, FDSET_WRITE) != 1) {
                goto CHECK_NEXT;
            } else if (delflg) {
                fddelset(&s->fds, evt->ident, FDSET_WRITE);
            }
            break;
        case EVFILT_SIGNAL:
            if (!sigismember(&s->signals, evt->ident)) {
                goto CHECK_NEXT;
            } else if (delflg) {
                sigdelset(&s->signals, evt->ident);
            }
            break;
        }

        // remove from kernel event
        if (delflg) {
            *isdel = delflg;
            // set errno
            if (delflg & EV_ERROR) {
                errno = evt->data;
            }
            evt->flags = EV_DELETE;
            // unregister if not oneshot event
            if (!(delflg & EV_ONESHOT)) {
                kevent(s->fd, evt, 1, NULL, 0, NULL);
            }
        }

        e      = (evm_ev_t *)evt->udata;
        e->evt = *evt;
    }

    return e;
}

static inline int evm_register(evm_ev_t *e)
{
    // increase event-buffer and set event
    if (evm_increase_evs(e->s, 1) == 0 &&
        kevent(e->s->fd, &e->reg, 1, NULL, 0, NULL) == 0) {
        e->s->nreg++;
        return 0;
    }

    return -1;
}

// MARK: API for evm_ev_t

#define evm_ev_as_fd(e, fd, type, oneshot, edge)                               \
 do {                                                                          \
  /* already watched */                                                        \
  if (fdismember(&(e)->s->fds, (fd), FDSET_##type) == 1) {                     \
   errno = EALREADY;                                                           \
  } else if (fdset_realloc(&e->s->fds, fd) == 0 &&                             \
             fdaddset(&(e)->s->fds, (fd), FDSET_##type) == 0) {                \
   EV_SET(&(e)->reg, (uintptr_t)(fd), EVFILT_##type,                           \
          EV_ADD | ((oneshot) ? EV_ONESHOT : 0) | ((edge) ? EV_CLEAR : 0), 0,  \
          0, (void *)(e));                                                     \
   if (evm_register(e) == 0) {                                                 \
    return 0;                                                                  \
   }                                                                           \
   fddelset(&(e)->s->fds, (fd), FDSET_##type);                                 \
  }                                                                            \
  return -1;                                                                   \
 } while (0)

static inline int evm_ev_as_writable(evm_ev_t *e, int fd, int oneshot, int edge)
{
    evm_ev_as_fd(e, fd, WRITE, oneshot, edge);
}

static inline int evm_ev_as_readable(evm_ev_t *e, int fd, int oneshot, int edge)
{
    evm_ev_as_fd(e, fd, READ, oneshot, edge);
}

static inline int evm_ev_as_signal(evm_ev_t *e, int signo, int oneshot)
{
    // already watched
    if (sigismember(&e->s->signals, signo)) {
        errno = EALREADY;
        return -1;
    }

    // set event fields
    EV_SET(&e->reg, (uintptr_t)signo, EVFILT_SIGNAL,
           EV_ADD | (oneshot ? EV_ONESHOT : 0), 0, 0, (void *)e);

    if (evm_register(e) == 0) {
        sigaddset(&e->s->signals, signo);
        return 0;
    }

    return -1;
}

static inline int evm_ev_as_timer(evm_ev_t *e, lua_Integer timeout, int oneshot)
{
    // set event fields
    EV_SET(&e->reg, (uintptr_t)e, EVFILT_TIMER,
           EV_ADD | (oneshot ? EV_ONESHOT : 0), NOTE_NSECONDS,
           (intptr_t)(timeout * 1000000LL), (void *)e);

    return evm_register(e);
}

static inline int evm_ev_is_oneshot(evm_ev_t *e)
{
    return e->reg.flags & EV_ONESHOT;
}

static inline int evm_ev_is_hup(evm_ev_t *e)
{
    return e->evt.flags & (EV_EOF | EV_ERROR);
}

static inline int evm_ev_ident_lua(lua_State *L, const char *mt)
{
    evm_ev_t *e = luaL_checkudata(L, 1, mt);

    lua_pushinteger(L, e->reg.ident);

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
            lua_pushstring(L, strerror(errno));
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
        struct kevent evt = e->reg;

        // unregister event
        evt.flags = EV_DELETE;
        kevent(e->s->fd, &evt, 1, NULL, 0, NULL);
        e->s->nreg--;
        e->ref = lauxh_unref(L, e->ref);
        if (ev) {
            *ev = e;
        }
    }

    lua_pushboolean(L, 1);

    return 1;
}

#endif
