local evm = require('evm')
local fork = require('fork')
local assert = require('assert')
local llsocket = require('llsocket')
local NCONN = 0

local function close(req)
    -- print('delete request', req.evr, req.evw)
    if req.evs then
        req.evs[1]:revert()
        req.evs[2]:revert()
    end

    req.sock:close()
    NCONN = NCONN - 1
    -- print('connection ', NCONN)
end

local function echo_sendq(req)
    if req.qtail >= req.qhead then
        local sock = req.sock
        local msgq, qhead, qtail = req.msgq, req.qhead, req.qtail

        for i = req.qhead, req.qtail do
            local len, err, again = sock:send(msgq[i])
            if err then
                print('failed to send', err)
                close(req)
                return
            elseif again then
                msgq[i] = msgq[i]:sub(len + 1)
                break
            end

            qhead = i + 1
            msgq[i] = nil
        end

        if qhead > qtail then
            print('rewind msgq')
            req.qhead, req.qtail = 1, 0
        end
    end
end

local function push_sendq(req, msg)
    req.qtail = req.qtail + 1
    req.msgq[req.qtail] = msg
    -- print('push to msgq', msg)
end

local function echo_recv(req)
    local msg, err, again = req.sock:recv()
    if again then
        return
    elseif not msg then
        if err then
            print('failed to recv', err)
        end
        close(req)
        return
    end

    -- message must be sent after queued messages have been sent
    if req.qtail > 0 then
        push_sendq(req, msg)
        return
    end

    local len
    len, err, again = req.sock:send(msg)
    if err then
        print('failed to send', err)
        close(req)
    elseif again then
        push_sendq(req, msg:sub(len + 1))
    end
end

local function accept(server, m)
    local sock, err, again = server:accept()

    if again then
        -- print('accept again')
        return
    elseif err then
        print('failed to accept', err)
        return
    end

    -- create read and write events
    local req = {
        m = m,
        sock = sock,
        msgq = {},
        qhead = 1,
        qtail = 0,
    }
    req.evs, err = m:newevents(2)
    if err then
        print('failed to create read and write events:', err)
        sock:close()
        return
    end

    -- register read event
    local evs = req.evs
    err = evs[1]:asreadable(sock:fd(), req)
    if err then
        print('failed to register read event:', err)
        sock:close()
    end
    -- register write event with edge triger
    err = evs[2]:aswritable(sock:fd(), req, false, true)
    if err then
        evs[1]:revert()
        print('failed to register write event:', err)
        sock:close()
    end

    NCONN = NCONN + 1
    -- print('connection', NCONN)
end

local function run_loop(server)
    -- create loop
    local m = assert(evm.default())
    -- register server fd
    local sev = assert(m:newevent())
    local err = sev:asreadable(server:fd())
    if err then
        error(err)
    end

    -- event-loop
    while #m > 0 do
        local nevt, werr = m:wait(-1)
        if err then
            print(werr)
            return
        elseif nevt == 0 then
            print('no event')
        else
            local ev, ctx = m:getevent()
            while ev do
                if ev == sev then
                    -- server event
                    accept(server, m)
                elseif ctx.evs[1] == ev then
                    echo_recv(ctx)
                else
                    echo_sendq(ctx)
                end
                ev, ctx = m:getevent()
            end
        end
    end
end

local function echo_server()
    local addrinfo = llsocket.addrinfo.inet('127.0.0.1', 5000,
                                            llsocket.SOCK_STREAM,
                                            llsocket.IPPROTO_TCP,
                                            llsocket.AI_PASSIVE)
    -- create a socket with O_NONBLOCK option
    local server, err = llsocket.socket.new(addrinfo:family(),
                                            addrinfo:socktype(),
                                            addrinfo:protocol(), true)
    if err then
        error(err)
    end
    server:reuseaddr(true)
    assert(server:bind(addrinfo))
    assert(server:listen())

    print('start server: ', addrinfo:addr() .. ':' .. addrinfo:port())

    local worker = {}
    for _ = 1, 8 do
        local p = assert(fork())
        if p:is_child() then
            print('worker', p:pid())

            local ok
            ok, err = pcall(run_loop, server)
            if not ok then
                print(err)
            end
            os.exit()
        end
        worker[#worker + 1] = p
    end
    server:close()

    while #worker > 0 do
        local list = {}
        for _, p in ipairs(worker) do
            p:wait()
            if p:pid() > 0 then
                list[#list + 1] = p
            end
        end
        worker = list
    end

    print('end server')
end

echo_server()
