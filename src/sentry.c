/*
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
 *  sentry.c
 *  lua-sentry
 *  Created by Masatoshi Teruya on 15/08/24.
 */


#include "sentry_event.h"

static pid_t SENTRY_PID = -1;
static int DEFAULT_SENTRY = LUA_NOREF;


static int wait_lua( lua_State *L )
{
    sentry_t *s = luaL_checkudata( L, 1, SENTRY_MT );
    // default timeout: 1 sec
    int timeout = (int)luaL_optinteger( L, 2, -1 );
    sentry_ev_t *e = NULL;
    int isdel = 0;
    
    // check arguments
    // cleanup current events
    while( ( e = sentry_getev( s, &isdel ) ) )
    {
        if( isdel ){
            e->ref = lstate_unref( L, e->ref );
            s->nreg--;
        }
    }
    
    // wait event
    s->nevt = sentry_wait( s, timeout );
    // got errno
    if( s->nevt == -1 )
    {
        switch( errno ){
            // ignore error
            case ENOENT:
            case EINTR:
                s->nevt = 0;
                errno = 0;
            break;
            
            // return error
            default:
                lua_pushinteger( L, 0 );
                lua_pushstring( L, strerror( errno ) );
                return 2;
        }
    }
    
    // return number of event
    lua_pushinteger( L, s->nevt );
    
    return 1;
}


static int getevent_lua( lua_State *L )
{
    sentry_t *s = luaL_checkudata( L, 1, SENTRY_MT );
    int isdel = 0;
    sentry_ev_t *e = sentry_getev( s, &isdel );
    
    // return event, isdel and context
    if( e )
    {
        lstate_pushref( L, e->ref );
        lua_pushinteger( L, sev_type( e ) );
        lua_pushboolean( L, sev_is_hup( e ) );
        // release reference if deleted
        if( isdel ){
            e->ref = lstate_unref( L, e->ref );
            s->nreg--;
        }
        // push context if retained
        if( lstate_isref( e->ctx ) ){
            lstate_pushref( L, e->ctx );
            return 4;
        }
        
        return 3;
    }
    
    return 0;
}


static inline int newevent_lua( lua_State *L )
{
    sentry_t *s = luaL_checkudata( L, 1, SENTRY_MT );
    sentry_ev_t *e = lua_newuserdata( L, sizeof( sentry_ev_t ) );

    if( e ){
        // clear
        memset( (void*)e, 0, sizeof( sentry_ev_t ) );
        e->s = s;
        e->ctx = LUA_NOREF;
        e->ref = LUA_NOREF;
        // set metatable
        lstate_setmetatable( L, SENTRY_EVENT_MT );
        return 1;
    }

    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );

    return 2;
}


static int len_lua( lua_State *L )
{
    sentry_t *s = luaL_checkudata( L, 1, SENTRY_MT );
    
    lua_pushinteger( L, s->nreg );
    
    return 1;
}


static int tostring_lua( lua_State *L )
{
    return TOSTRING_MT( L, SENTRY_MT );
}


static int gc_lua( lua_State *L )
{
    sentry_t *s = lua_touserdata( L, 1 );

    // close if not invalid value
    if( s->fd != -1 ){
        close( s->fd );
    }
    pdealloc( s->evs );
    fdset_dealloc( &s->fds );
    
    return 0;
}


// allocate sentry data
static int new_lua( lua_State *L )
{
    sentry_t *s = NULL;
    int nbuf = 128;
    
    // check arguments
    // arg#1 number of event buffer size
    if( !lua_isnoneornil( L, 1 ) )
    {
        nbuf = (int)luaL_checkinteger( L, 1 );
        if( nbuf < 1 || nbuf > INT_MAX ){
            return luaL_error( 
                L, "event buffer value range must be 1 to %d", INT_MAX 
            );
        }
    }

    // create and init sentry_t
    if( ( s = lua_newuserdata( L, sizeof( sentry_t ) ) ) &&
        ( s->evs = pnalloc( (size_t)nbuf, kevt_t ) ) )
    {
        if( fdset_alloc( &s->fds, (size_t)nbuf ) == 0 )
        {
            // create event descriptor
            if( ( s->fd = sentry_createfd() ) != -1 ){
                lstate_setmetatable( L, SENTRY_MT );
                s->nbuf = nbuf;
                s->nreg = 0;
                s->nevt = 0;
                sigemptyset( &s->signals );
                
                return 1;
            }
            fdset_dealloc( &s->fds );
        }
        pdealloc( s->evs );
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


// create default sentry
static int default_lua( lua_State *L )
{
    if( lstate_isref( DEFAULT_SENTRY ) )
    {
        lstate_pushref( L, DEFAULT_SENTRY );
        // return current default sentry
        if( SENTRY_PID == getpid() ){
            return 1;
        }
        // create new default sentry
        else {
            sentry_t *s = luaL_checkudata( L, 1, SENTRY_MT );

            // should close event descriptor
            close( s->fd );
            // invalid value
            s->fd = -1;
            // release reference
            DEFAULT_SENTRY = lstate_unref( L, DEFAULT_SENTRY );
            lua_pop( L, 1 );
        }
    }
    
    switch( new_lua( L ) ){
        case 1:
            DEFAULT_SENTRY = lstate_refat( L, -1 );
            SENTRY_PID = getpid();
            return 1;
        
        default:
            return 2;
    }
}


LUALIB_API int luaopen_sentry( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", gc_lua },
        { "__tostring", tostring_lua },
        { "__len", len_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        { "newevent", newevent_lua },
        { "getevent", getevent_lua },
        { "wait", wait_lua },
        { NULL, NULL }
    };
    
    // register event metatables
    luaopen_sentry_event( L );
    luaopen_sentry_readable( L );
    luaopen_sentry_writable( L );
    luaopen_sentry_timer( L );
    luaopen_sentry_signal( L );

    // register sentry-metatable
    sentry_define_mt( L, SENTRY_MT, mmethod, method );
    // create table
    lua_newtable( L );
    lstate_fn2tbl( L, "new", new_lua );
    lstate_fn2tbl( L, "default", default_lua );
    // add event type
    lstate_num2tbl( L, "EV_NONE", -1 );
    lstate_num2tbl( L, "EV_READABLE", SENTRY_EV_READABLE );
    lstate_num2tbl( L, "EV_WRITABLE", SENTRY_EV_WRITABLE );
    lstate_num2tbl( L, "EV_TIMER", SENTRY_EV_TIMER );
    lstate_num2tbl( L, "EV_SIGNAL", SENTRY_EV_SIGNAL );
    
    return 1;
}

