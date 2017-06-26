local sentry = require('sentry');
local unpack = unpack or table.unpack;
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

-- invalid arguments
-- sec must be number
for _, v in ipairs({ str, bol, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:astimer( v );
    end));
end

-- oneshot must be boolean
for _, v in ipairs({ str, num, int, tbl, fn, co }) do
    ifTrue(isolate(function()
        e:astimer( sec, nil, v );
    end));
end

-- oneshot
ifNotNil( e:astimer( 1000, ctx, true ) );
-- verify type
ifNotTrue( e:asa() == 'astimer', 'invalid implements' );

-- verify oneshot
elapsed = os.time();
nevt = s:wait(-1);
elapsed = os.time() - elapsed;
-- verify elapsed
ifNotEqual( elapsed, 1 );

-- verify number of events
ifNotEqual( nevt, 1 );

-- verify event
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
ifNotNil( e:astimer( 1000, ctx, false ) );

-- verify repeatable
nrep = 0;
repeat
    elapsed = os.time();
    nevt = s:wait(-1);
    elapsed = os.time() - elapsed;
    -- verify elapsed
    ifNotEqual( elapsed, 1 );

    -- verify number of events
    ifNotEqual( nevt, 1 );

    -- verify event
    ev, ectx = s:getevent();
    ifNotEqual( e, ev );
    ifNotEqual( ectx, ctx );
    ifNotEqual( ev:context(), ectx );

    nrep = nrep + 1;
    if nrep == 1 then
        -- verify registration
        ifNotEqual( #s, 1 );
        -- get next event
        ev = s:getevent();
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
