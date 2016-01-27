/*
 *  Copyright (C) 2014 Masatoshi Teruya
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
 *  epoll/fdset.h
 *  lua-sentry
 *  Created by Masatoshi Teruya on 14/05/27.
 */

#ifndef SENTRY_EPOLL_FDSET_H
#define SENTRY_EPOLL_FDSET_H


typedef struct {
    int nevs;
    void **evs;
} fdset_t;

#define FV_SIZE sizeof(void*)


enum FDSET_MEMBER_TYPE {
    FDSET_READ  = EPOLLIN,
    FDSET_WRITE = EPOLLOUT
};


static inline int fdset_alloc( fdset_t *set, size_t nfd )
{
    set->evs = calloc( (size_t)nfd, sizeof( void* ) );
    
    if( set->evs ){
        set->nevs = nfd;
        return 0;
    }
    
    return -1;
}


static inline int fdset_realloc( fdset_t *set, int fd )
{
    if( fd < 0 ){
        errno = EINVAL;
        return -1;
    }
    else if( fd >= set->nevs )
    {
        // realloc event container
        void **evs = realloc( set->evs, ( fd + 1 ) * sizeof( void* ) );
        
        if( !evs ){
            return -1;
        }
        memset( evs + FV_SIZE * set->nevs, 0, ( fd - set->nevs ) * FV_SIZE );
        set->nevs = fd;
        set->evs = evs;
    }
    
    return 0;
}


static inline void fdset_dealloc( fdset_t *set )
{
    free( (void*)set->evs );
}


static inline void *fdismember( fdset_t *set, int fd )
{
    if( fd < 0 || fd > set->nevs ){
        errno = EINVAL;
        return NULL;
    }
    
    return set->evs[fd];
}


static inline int fdaddset( fdset_t *set, int fd, void *evt )
{
    if( fd < 0 || fd > set->nevs ){
        errno = EINVAL;
        return -1;
    }
    
    set->evs[fd] = evt;
    
    return 0;
}

static inline int fddelset( fdset_t *set, int fd )
{
    if( fd < 0 || fd > set->nevs ){
        errno = EINVAL;
        return -1;
    }
    
    set->evs[fd] = NULL;
    
    return 0;
}


#endif
