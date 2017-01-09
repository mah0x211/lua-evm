--[[
  
  Copyright (C) 2015 Masatoshi Teruya

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  example/readable_ev.lua
  lua-sentry

  Created by Masatoshi Teruya on 15/08/25.
  
--]]

local sentry = require('sentry');
local waitsec = 2000;
local nrep = 0;
local oneshot = false;
local edgeTrigger = true;
local s = assert( sentry.default() );
-- create io watcher: 0 = stdin
local e = assert( s:newevent() );
local err = e:asreadable( 0, oneshot, edgeTrigger );
local nevt, ev, etype, ishup;

if err then
    error( err );
end

print( 'event type', e:typeof(), sentry.EV_READABLE );

repeat
    print( 'wait #' .. #s );
    nevt = assert( s:wait( waitsec ) );
    print( 'got number of event', nevt );
    ev, etype, ishup = s:getevent();
    while ev do
        nrep = nrep + 1;
        assert( e == ev, 'invalid implements' );
        print('unwatch', assert( ev:unwatch() ) );
        if nrep == 1 then
            print('rewatch', assert( ev:watch() ) );
        end
        ev, etype, ishup = s:getevent();
    end
until #s == 0;

print( 'done' );


