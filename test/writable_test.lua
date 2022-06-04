local testcase = require('testcase')
local llsocket = require('llsocket')
local evm = require('evm')

-- socketpair
local SOCK1
local SOCK2

function testcase.before_all()
    local pair = assert(llsocket.socket.pair(llsocket.SOCK_STREAM, nil, true))
    SOCK1, SOCK2 = pair[1], pair[2]
end

function testcase.aswritable()
    local m = assert(evm.default())
    local ev = m:newevent()
    local ctx = {
        'foo/bar',
    }
    assert(SOCK1:sndbuf(5))
    local bufsiz = assert(SOCK1:sndbuf())

    -- test that event use as a writable event
    local err = ev:aswritable(SOCK1:fd(), ctx)
    assert.is_nil(err)
    assert.equal(ev:ident(), SOCK1:fd())
    assert.equal(ev:asa(), 'aswritable')
    assert.equal(ev:context(), ctx)

    -- test that event occurs when fd is writable
    local n
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that event occurs continuously if not edge-trigger
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if fd is not writable
    repeat
        local _, _, again = assert(SOCK1:send('hello'))
    until again
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs after the send buffer is empty
    assert(SOCK2:recv(bufsiz))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    ev:revert()
end

function testcase.aswritable_oneshot()
    local m = assert(evm.default())
    local ev = m:newevent()
    local err = ev:aswritable(SOCK1:fd(), nil, true)
    assert.is_nil(err)

    -- test that event occurs when fd is writable
    local n
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if registered as a one-shot event
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs if event is re-watched
    assert(ev:watch())
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- confirm that no event occurs
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    ev:revert()
end

function testcase.aswritable_edge_trigger()
    local m = assert(evm.default())
    local ev = m:newevent()
    local err = ev:aswritable(SOCK1:fd(), nil, nil, true)
    assert.is_nil(err)

    -- test that event occurs when fd is writable
    local n
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if registered as a edge-trigger
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when fd is re-writable
    assert(SOCK1:send('hello'))
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())
    assert(SOCK2:recv())
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    ev:revert()
end

