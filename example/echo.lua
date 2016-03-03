local unpack = unpack or table.unpack;
local inspect = require('util').inspect;
local sentry = require('sentry');
local lls = require('llsocket');
local getaddrinfoInet = lls.inet.getaddrinfo;
local socket = lls.socket;
local SOCK_STREAM = lls.SOCK_STREAM;
local HOST = '127.0.0.1';
local PORT = '5000';
local NOONESHOT = false;
local EDGE = true;
local NONBLOCK = true;
local REUSEADDR = true;
local NCONN = 0;


local function echoClose( req )
    --print( 'delete request', req.evr, req.evw );
    if req.evs then
        req.evs[1]:revert();
        req.evs[2]:revert();
    end
    
    req.sock:close();
    NCONN = NCONN - 1;
    print( 'connection ', NCONN );
end


local function echoSendQ( req )
    --print( 'echo send ', req.qtail, req.qhead );
    if req.qtail >= req.qhead then
        local sock = req.sock;
        local msgq, qhead, qtail = req.msgq, req.qhead, req.qtail;
        local len, err, again;

        for i = req.qhead, req.qtail do
            len, err, again = sock:send( msgq[i] );

            if not len then
                if err then
                    print( 'failed to send', err );
                end
                return echoClose( req );
            elseif again then
                msgq[i] = msgq[i]:sub( len + 1 );
                break;
            end

            qhead = i + 1;
            msgq[i] = nil;
        end

        if qhead > qtail then
            print( 'rewind msgq' );
            req.qhead, req.qtail = 1, 0;
        end
    end
end


local function pushSendQ( req, msg )
    req.qtail = req.qtail + 1;
    req.msgq[req.qtail] = msg;
    print( 'push to msgq', msg );
end


local function echoRecv( req )
    local msg, err, again = req.sock:recv();

    if again then
        print( 'recv again' );
    -- close by peer
    elseif not msg then
        if err then
            print( 'failed to recv', err );
        end
        echoClose( req );
    elseif req.qtail > 0 then
        pushSendQ( req, msg );
    else
        local len;
        
        len, err, again = req.sock:send( msg );
        if not len then
            if err then
                print( 'failed to send', err );
            end
            echoClose( req );
        elseif again then
            pushSendQ( req, msg:sub( len + 1 ) );
        end
    end
end


local function acceptClient( s, server, ishup )
    if ishup then
        error( 'server socket has been closed' );
    else
        local sock, err, again = server:accept();
        
        if again then
            print( 'accept again' );
        elseif err then
            print( 'failed to accept', err );
        else
            local req = {
                s = s,
                sock = sock,
                msgq = {},
                qhead = 1,
                qtail = 0;
            };

            NCONN = NCONN + 1;
            --sock:sndbuf( 5 );
            -- create read/write event
            req.evs, err = s:newevent( 2 );
            if not err then
                err = req.evs[1]:asreadable( sock:fd(), req ) or
                      req.evs[2]:aswritable( sock:fd(), req, NOONESHOT, EDGE );
            end

            -- got error
            if err then
                print( 'failed to register read/write event', sock, err );
                echoClose( req );
            else
                print( 'connection', NCONN );
            end
        end
    end
end


local function createServer()
    -- create loop
    local s = assert( sentry.default() );
    -- create bind socket
    local addrs = assert( getaddrinfoInet( HOST, PORT, SOCK_STREAM ) );
    local server, sev, ev, nevt, ishup, ctx, err;

    for _, addr in ipairs( addrs ) do
        server, err = socket.new( addr, NONBLOCK );
        if server then
            assert( server:reuseaddr( true ) );
            err = server:bind();
            if not err then
                err = server:listen();
            end
            break;
        end
    end

    if err then
        error( err );
    end

    sev = unpack( assert( s:newevent() ) );
    err = sev:asreadable( server:fd() );
    if err then
        error( err );
    end

    -- run
    print( 'start server: ', HOST, PORT );
    
    -- event-loop
    repeat
        nevt, err = s:wait(-1);
        if err then
            print( err );
            break;
        elseif nevt == 0 then
            print( 'no event' );
        else
            --print('got event', nevt );
            ev, etype, ishup, ctx = s:getevent();
            while ev do
                --print('got event', nevt, ishup, ev, ctx );
                if ev == sev then
                    acceptClient( s, server, ishup );
                elseif ishup then
                    echoClose( ctx );
                elseif ctx.evs[1] == ev then
                    echoRecv( ctx );
                else
                    echoSendQ( ctx );
                end
                ev, etype, ishup, ctx = s:getevent();
            end
        end
    until #s == 0;
    
    print( 'end server' );
end

createServer();
