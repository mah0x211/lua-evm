local signal = require('signal');
local sentry = require('sentry');
local unpack = unpack or table.unpack;
local SIGINT = signal.SIGINT;
local str = 'string';
local num = 1.1;
local int = 1;
local bol = false;
local tbl = {};
local fn = function()end
local co = coroutine.create(fn);
local ctx = {};
local s = ifNil( sentry.default() );
local e, timer = unpack( ifNil( s:newevents( 2 ) ) );

-- block SIGINT
ifNotTrue( signal.block( SIGINT ) );

-- timer
ifNotNil( timer:astimer( 1, ctx, true ) );
ifNotTrue( timer:unwatch() );


-- invalid arguments
-- sec must be number
for _, v in ipairs({ str, bol, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:asreadable( v );
    end));
end

-- oneshot must be boolean
for _, v in ipairs({ str, num, int, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:asreadable( sec, nil, v );
    end));
end


-- edge must be boolean
for _, v in ipairs({ str, num, int, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:asreadable( sec, nil, nil, v );
    end));
end

-- oneshot
ifNotNil( e:assignal( SIGINT, ctx, true ) );
-- verify type
ifNotTrue( e:asa() == 'assignal', 'invalid implements' );

-- verify oneshot
signal.raise( SIGINT );
nevt = s:wait(-1);

-- verify number of events
ifNotEqual( nevt, 1 );

-- verify return values
ev, ectx = s:getevent();
ifNotEqual( e, ev );
ifNotEqual( ectx, ctx );
ifNotEqual( ev:context(), ectx );

-- verify registration
ifNotEqual( #s, 0 );
ifNotTrue( ev:watch() );
ifNotEqual( #s, 1 );
ifNotTrue( ev:unwatch() );
ifNotEqual( #s, 0 );
-- revert
ifNotEqual( ev:revert(), e );

-- get next event
ev = s:getevent();
ifNotNil( ev, 'invalid implements' );


-- repeatable
nrep = 0;
ifNotNil( e:assignal( SIGINT, ctx, false ) );
-- verify repeatable
signal.raise( SIGINT );
repeat
    nevt = s:wait(-1);

    -- verify number of events
    ifNotEqual( nevt, 1 );

    -- verify return values
    ev, ectx = s:getevent();
    ifNotEqual( e, ev );
    ifNotEqual( ectx, ctx );
    ifNotEqual( ev:context(), ectx );
    nrep = nrep + 1;
    if nrep < 3 then
        -- verify registration
        ifNotEqual( #s, 1 );
        -- get next event
        ev = s:getevent();
        ifNotNil( ev, 'invalid implements' );
        signal.raise( SIGINT );
    else
        -- verify registration
        ifNotEqual( #s, 1 );
        ifNotTrue( ev:unwatch() );
        ifNotEqual( #s, 0 );

        -- revert
        ifNotEqual( ev:revert(), e );
    end
until #s == 0;

