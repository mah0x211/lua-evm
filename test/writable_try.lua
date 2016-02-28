local socketpair = require('llsocket').socket.pair;
local sentry = require('sentry');
local str = 'string';
local num = 1.1;
local int = 1;
local bol = false;
local tbl = {};
local fn = function()end
local co = coroutine.create(fn);
local ctx = {};
local s = ifNil( sentry.default() );
local e = ifNil( s:newevent() );
local timer = ifNil( s:newevent() );

-- timer
ifNil( timer:astimer( 1, ctx, true ) );
ifNotTrue( timer:unwatch() );

-- socketpair
fds = ifNil( socketpair( require('llsocket').SOCK_STREAM ) );

-- invalid arguments
-- sec must be number
for _, v in ipairs({ str, bol, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:aswritable( v );
    end));
end

-- oneshot must be boolean
for _, v in ipairs({ str, num, int, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:aswritable( sec, nil, v );
    end));
end


-- edge must be boolean
for _, v in ipairs({ str, num, int, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:aswritable( sec, nil, nil, v );
    end));
end

-- oneshot
ifNil( e:aswritable( fds[1]:fd(), ctx, true ) );
-- verify type
ifNotTrue( e:typeof() == sentry.EV_WRITABLE, 'invalid implements' );

-- verify oneshot
nevt = s:wait(-1);

-- verify number of events
ifNotEqual( nevt, 1 );

-- verify return values
ev, etype, ishup = s:getevent();
ifNotEqual( e, ev );
ifNotEqual( etype, sentry.EV_WRITABLE );
ifNotEqual( ev:typeof(), etype );
ifTrue( ishup, 'invalid implements' );
ifNotEqual( ev:context(), ctx );

-- verify registration
ifNotEqual( #s, 0 );
ifNotTrue( ev:watch() );
ifNotEqual( #s, 1 );
ifNotTrue( ev:unwatch() );
ifNotEqual( #s, 0 );
-- revert
ifNotEqual( ev:revert(), e );

-- get next event
ev, etype, ishup = s:getevent();
ifNotNil( ev, 'invalid implements' );


-- repeatable
nrep = 0;
ifNil( e:aswritable( fds[1]:fd(), ctx, false ) );
-- verify repeatable
repeat
    nevt = s:wait(-1);

    -- verify number of events
    ifNotEqual( nevt, 1 );

    -- verify return values
    ev, etype, ishup = s:getevent();
    ifNotEqual( e, ev );
    ifNotEqual( etype, sentry.EV_WRITABLE );
    ifNotEqual( ev:typeof(), etype );
    ifTrue( ishup, 'invalid implements' );
    ifNotEqual( ev:context(), ctx );

    nrep = nrep + 1;
    if nrep == 1 then
        -- verify registration
        ifNotEqual( #s, 1 );
        -- get next event
        ev, etype, ishup = s:getevent();
        ifNotNil( ev, 'invalid implements' );
    elseif nrep == 2 then
        -- verify registration
        ifNotEqual( #s, 1 );
        ifNotTrue( ev:unwatch() );
        ifNotEqual( #s, 0 );
        ifNotTrue( ev:watch() );
        ifNotEqual( #s, 1 );
    elseif nrep == 4 then
        -- verify registration
        ifNotEqual( #s, 1 );
        ifNotTrue( ev:unwatch() );
        ifNotEqual( #s, 0 );

        -- revert
        ifNotEqual( ev:revert(), e );
    end
until #s == 0;


-- repeatable
nrep = 0;
ifNil( e:aswritable( fds[1]:fd(), ctx, false, true ) );
-- verify edge
repeat
    nevt = s:wait(-1);

    -- verify number of events
    ifNotEqual( nevt, 1 );

    nrep = nrep + 1;
    if nrep == 1 then
        -- verify registration
        ifNotEqual( #s, 1 );

        -- verify event
        ev, etype, ishup = s:getevent();
        ifNotEqual( e, ev );
        ifNotEqual( etype, sentry.EV_WRITABLE );
        ifNotEqual( ev:typeof(), etype );
        ifTrue( ishup, 'invalid implements' );
        ifNotEqual( ev:context(), ctx );

        -- get next event
        ev, etype, ishup = s:getevent();
        ifNotNil( ev, 'invalid implements' );

        -- set timer
        ifNotTrue( timer:watch() );
        -- verify registration
        ifNotEqual( #s, 2 );

    else
        -- verify registration
        ifNotEqual( #s, 2 );

        -- verify event
        ev, etype, ishup = s:getevent();
        ifNotEqual( timer, ev );
        ifNotEqual( etype, sentry.EV_TIMER );
        ifNotEqual( ev:typeof(), etype );
        ifTrue( ishup, 'invalid implements' );
        ifNotEqual( ev:context(), ctx );

        -- verify registration
        ifNotEqual( #s, 1 );
        ifNotTrue( e:unwatch() );
        ifNotEqual( #s, 0 );

        -- revert
        ifNotEqual( e:revert(), e );
    end
until #s == 0;


