local testcase = require('testcase')
local evm = require('evm')

function testcase.astimer()
    local m = assert(evm.new())
    local ev = m:newevent()
    local ctx = {
        'foo/bar',
    }

    -- test that event use as a timer event
    assert(ev:astimer(15, ctx))
    assert.match(ev, '^evm.timer: ', false)
    assert.equal(ev:ident(), 15)
    assert.equal(ev:asa(), 'astimer')
    assert.equal(ev:context(), ctx)

    -- test that no event occurs if not timed-out
    local n, err = m:wait(6)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs when timed-out
    n, err = m:wait(20)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that event occurs continuously if not one-shot event
    n, err = m:wait(20)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    ev:revert()
end

function testcase.astimer_oneshot()
    local m = assert(evm.new())
    local ev = m:newevent()
    assert(ev:astimer(15, nil, true))

    -- test that event occurs when timed-out
    local n, err = m:wait(20)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)

    -- test that no event occurs if registered as a one-shot event
    n, err = m:wait(30)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    -- test that event occurs if event is re-watched
    assert(ev:watch())
    n, err = m:wait(20)
    assert.equal(n, 1)
    assert.is_nil(err)
    assert.equal(m:getevent(), ev)
    -- confirm that no event occurs
    n, err = m:wait(20)
    assert.equal(n, 0)
    assert.is_nil(err)
    assert.is_nil(m:getevent())

    ev:revert()
end

