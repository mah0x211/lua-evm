local testcase = require('testcase')
local llsocket = require('llsocket')
local evm = require('evm')

-- socketpair
local SOCK1
local SOCK2

function testcase.before_all()
    local pair = assert(llsocket.socket.pair(llsocket.SOCK_STREAM))
    SOCK1, SOCK2 = pair[1], pair[2]
end

function testcase.asreadable()
    local m = assert(evm.new())
    local ev = m:newevent()
    local ctx = {
        'foo/bar',
    }

    -- test that event use as a readable event
    assert(ev:asreadable(SOCK1:fd(), ctx))
    assert.match(ev, '^evm.readable: ', false)
    assert.equal(ev:ident(), SOCK1:fd())
    assert.equal(ev:asa(), 'asreadable')
    assert.equal(ev:context(), ctx)

    -- test that no event occurs if fd is not readable
    local n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when fd is readable
    assert(SOCK2:send('hello'))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that event occurs continuously if not edge-trigger
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)
    assert.equal(SOCK1:recv(), 'hello')

    -- test that no event occurs after read message
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    ev:revert()
end

function testcase.asreadable_oneshot()
    local m = assert(evm.new())
    local ev = m:newevent()
    assert(ev:asreadable(SOCK1:fd(), nil, true))

    -- test that event occurs when fd is readable
    assert(SOCK2:send('hello'))
    local n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)
    assert.equal(SOCK1:recv(), 'hello')

    -- test that no event occurs if registered as a one-shot event
    assert(SOCK2:send('world'))
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

    assert.equal(SOCK1:recv(), 'world')
    ev:revert()
end

function testcase.asreadable_edge_trigger()
    local m = assert(evm.new())
    local ev = m:newevent()
    assert(ev:asreadable(SOCK1:fd(), nil, nil, true))

    -- test that event occurs when fd is readable
    assert(SOCK2:send('hello'))
    local n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if registered as a edge-trigger
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when fd is re-readable
    assert(SOCK2:send('world'))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    assert.equal(SOCK1:recv(), 'helloworld')
    ev:revert()
end

