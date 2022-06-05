local testcase = require('testcase')
local evm = require('evm')
local signal = require('signal')

function testcase.assignal()
    local m = assert(evm.new())
    local ev = m:newevent()
    local ctx = {
        'foo/bar',
    }

    -- test that event use as a signal event
    assert(signal.block(signal.SIGUSR1))
    assert(ev:assignal(signal.SIGUSR1, ctx))
    assert.match(ev, '^evm.signal: ', false)
    assert.equal(ev:ident(), signal.SIGUSR1)
    assert.equal(ev:asa(), 'assignal')
    assert.equal(ev:context(), ctx)

    -- test that no event occurs if no signal occurs
    local n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when got SIGUSR1 signal
    assert(signal.kill(signal.SIGUSR1))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that event occurs continuously if not one-shot event
    assert(signal.kill(signal.SIGUSR1))
    n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    ev:revert()
end

function testcase.assignal_oneshot()
    local m = assert(evm.new())
    local ev = m:newevent()
    assert(ev:assignal(signal.SIGUSR1, nil, true))
    assert(signal.block(signal.SIGUSR1))

    -- test that event occurs when got SIGUSR1 signal
    assert(signal.kill(signal.SIGUSR1))
    local n, err = m:wait(5)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if registered as a one-shot event
    assert(signal.kill(signal.SIGUSR1))
    n, err = m:wait(5)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs if event is re-watched
    assert(ev:watch())
    assert(signal.kill(signal.SIGUSR1))
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

