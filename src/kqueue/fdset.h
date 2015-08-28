/*
 *  Copyright (C) 2014 Masatoshi Teruya
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
 *  kevent/fdset.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 14/05/26.
 */

#ifndef SENTRY_KEVENT_FDSET_H
#define SENTRY_KEVENT_FDSET_H

#include "bitvec.h"

typedef bitvec_t fdset_t;

enum FDSET_MEMBER_TYPE {
    FDSET_READ  = 0x1,
    FDSET_WRITE = 0x2,
    FDEST_RDWR  = FDSET_READ|FDSET_WRITE
};

static inline int fdset_alloc( fdset_t *set, size_t nfd )
{
    return bitvec_alloc( set, nfd + 1 );
}

static inline int fdset_realloc( fdset_t *set, int fd )
{
    if( fd < 0 ){
        errno = EINVAL;
        return -1;
    }
    
    return bitvec_realloc( set, ( fd << 1 ) + 1 );
}

static inline void fdset_dealloc( fdset_t *set )
{
    bitvec_dealloc( set );
}

static inline int fdismember( fdset_t *set, int fd, int type )
{
    if( fd < 0 || type & ~FDEST_RDWR ){
        errno = EINVAL;
        return -1;
    }
    else if( type == FDSET_READ ){
        return bitvec_get( set, fd << 1 );
    }
    
    return bitvec_get( set, ( fd << 1 ) + 1 );
}

static inline int fdaddset( fdset_t *set, int fd, int type )
{
    if( fd < 0 || type & ~FDEST_RDWR ){
        errno = EINVAL;
        return -1;
    }
    else if( type == FDSET_READ ){
        return bitvec_set( set, fd << 1 );
    }
    
    return bitvec_set( set, ( fd << 1 ) + 1 );
}

static inline int fddelset( fdset_t *set, int fd, int type )
{
    if( fd < 0 || type & ~FDEST_RDWR ){
        errno = EINVAL;
        return -1;
    }
    else if( type == FDSET_READ ){
        return bitvec_unset( set, fd << 1 );
    }
    
    return bitvec_unset( set, ( fd << 1 ) + 1 );
}


#endif
