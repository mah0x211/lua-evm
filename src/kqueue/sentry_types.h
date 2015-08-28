/*
 *  Copyright (C) 2015 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *
 *  kevent/sentry_types.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */

#ifndef SENTRY_KEVENT_TYPES_H
#define SENTRY_KEVENT_TYPES_H

#include <sys/event.h>
#include "fdset.h"


// kernel event-loop fd creator
#define sentry_createfd()   kqueue()


// kernel event structure
typedef struct kevent   kevt_t;

typedef struct {
    int fd;
    int nbuf;
    int nreg;
    int nevt;
    sigset_t signals;
    fdset_t fds;
    kevt_t *evs;
} sentry_t;


typedef struct {
    sentry_t *s;
    kevt_t reg;
    kevt_t evt;
    int ref;
    int ctx;
} sentry_ev_t;


#endif


