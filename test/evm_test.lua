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

function testcase.new()
    -- test that create a new event monitor object
    local m = assert(evm.new())
    assert.match(m, '^evm: ', false)
    assert.not_equal(evm.new(), m)
end

function testcase.default()
    -- test that return default event monitor object
    local m = assert(evm.default())
    assert.equal(evm.default(), m)
end

function testcase.newevent()
    local m = assert(evm.new())

    -- test that create new event object
    local ev = m:newevent()
    assert.match(ev, '^evm.event: ', false)
end

function testcase.newevents()
    local m = assert(evm.new())

    -- test that create a specified number of new event objects
    local evs = m:newevents(10)
    assert.is_table(evs)
    assert.equal(#evs, 10)
    local uniqueness = {}
    for _, ev in pairs(evs) do
        assert.match(ev, '^evm.event: ', false)
        assert.is_nil(uniqueness[ev])
        uniqueness[ev] = ev
    end

    -- test that throws an error if n is less than 1
    local err = assert.throws(m.newevents, m, -10)
    assert.match(err, 'out of range value')
end

function testcase.renew()
    local m = assert(evm.new())
    local ev = m:newevent()
    assert(ev:asreadable(SOCK1:fd()))
    assert(SOCK2:send('hello'))

    -- test that no event occurs if event monitor is renewed
    assert(m:renew())
    local n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs after event object is renewed
    assert(ev:renew(m))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)
    assert.equal(SOCK1:read(), 'hello')
end

function testcase.wait()
    local m = assert(evm.new())

    -- test that no event occurs if registered event does not exist
    assert.equal(#m, 0)
    local n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when fd is readable
    local ev = m:newevent()
    assert(ev:asreadable(SOCK1:fd()))
    assert(SOCK2:send('hello'))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)

    -- test that no event occurs if event is disabled
    ev:unwatch()
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    ev:revert()
end

