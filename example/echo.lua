local inspect = require('util').inspect;
local process = require('process');
local sentry = require('sentry');
local lls = require('llsocket');
local bind = lls.inet.bind;
local listen = lls.listen;
local close = lls.close;
local recv = lls.recv;
local send = lls.send;
local accept = lls.acceptInherits;
local SOCK_STREAM = lls.SOCK_STREAM;
local HOST = '127.0.0.1';
local PORT = '5000';
local NONBLOCK = true;
local REUSEADDR = true;
local RecvTotal = 0;
local SendTotal = 0;

local function dispose( req )
    print( 'delete request', req.evr, req.evw );
    if req.evr then
        req.evr:unwatch();
    end
    
    if req.evw then
        erq.evw:unwatch();
    end
    
    close( req.fd );
end

local function echo( req, isdel )
    if isdel then
        dispose( req );
    elseif req.waitw then
        local len, err, again = send( req.fd, req.msg );
        
        if not again then
            if err then
                print( 'failed to send', err );
                dispose( req );
            else
                assert( req.evw:unwatch() );
                assert( req.evr:watch() );
                req.waitw = false;
                if len then
                    SendTotal = SendTotal + len;
                end
            end
        end
    else
        local msg, err, again = recv( req.fd );
        
        if not again then
            -- close by peer
            if not msg then
                if err then
                    print( 'failed to recv', err );
                end
                dispose( req );
            else
                local len;
                
                RecvTotal = RecvTotal + #msg;
                len, err, again = send( req.fd, msg );
                if again then
                    assert( req.evr:unwatch() );
                    req.evw = assert( req.s:writable( req.fd, req ) );
                    req.waitw = true;
                    req.msg = msg;
                elseif err then
                    print('failed to send', err );
                    dispose( req );
                elseif len then
                    SendTotal = SendTotal + len;
                end
            end
        end
    end
end


local function acceptClient( s, sfd )
    local fd, err, again = accept( sfd );
    
    if not again then
        if err then
            print( 'failed to accept', err );
        else
            local req = {
                s = s,
                fd = fd
            };
            
            req.evr, err = s:readable( fd, req );
            if err then
                print( 'failed to register readable', fd, err );
                close( fd );
            end
        end
    else
        print( 'accept again' );
    end
end


local function createServer()
    -- create loop
    local s = assert( sentry.new() );
    local clients = {};
    local sfd, sev, ev, nevt, ctx;
    
    -- create bind socket
    sfd = assert( bind( HOST, PORT, SOCK_STREAM, NONBLOCK, REUSEADDR ) );
    err = listen( sfd );
    if err then
        error( err );
    end
        
    sev = assert( s:readable( sfd ) );
    -- run
    print( 'start server: ', HOST, PORT );
    
    -- event-loop
    repeat
        nevt = s:wait(1);
        if nevt == 0 then
            print(
                ('rt: %d, st: %d'):format( RecvTotal, SendTotal )
            );
        else
            ev, isdel, ctx = s:getevent();
            while ev do
                --print('got event', nevt, isdel, ev, ctx );
                if ev == sev then
                    acceptClient( s, sfd );
                else
                    echo( ctx, isdel );
                end
                ev, isdel, ctx = s:getevent();
            end
        end
    until #s == 0;
    
    print( 'end server' );
end

createServer();

