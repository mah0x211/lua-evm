lua-evm
===

**DEPRECATED**

Development of lua-evm has been discontinued.  
Please use [lua-epoll](https://github.com/mah0x211/lua-epoll) or [lua-kqueue](https://github.com/mah0x211/lua-kqueue), which are devided by each event mechanism from lua-evm.

---

[![test](https://github.com/mah0x211/lua-evm/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-evm/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-evm/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-evm)


kqueue/epoll event module.

**NOTE: Do not use this module. this module is under heavy development.**


## Installation

```sh
luarocks install evm --server=https://luarocks.org/dev
```

## Error Handling

the functions/methods are return the error object created by https://github.com/mah0x211/lua-errno module.


## Event Monitor Object

## m, err = evm.new( [bufsize:int] );

creates an `evm` object.

**Parameters**

- `bufsize:int`: event buffer size. (`default 128`)

**Returns**

- `m:evm`: `evm` object on success, or `nil` on failure.
- `err:error`: error object.


**NOTE: bufsize will be automatically resized to larger than specified size if need more buffer allocation.**


## m, err = evm.default( [bufsize:int] );

returns the default `evm` object.

**Parameters** and **Returns** are same as evm.new function.


## ok, err = m:renew()

renew(recreate) the internal event descriptor.

**Returns**

- `ok:boolean`: true on success, or false on failure.
- `err:error`: error object.


## ev = m:newevent();

creates an [empty event object](#empty-event-object-methods).

**Returns**

- `ev:evm.event`: event object.


## evs = m:newevents( [nevt:int] );

creates the specified number of [empty event objects](#empty-event-object-methods).

**Parameters**

- `nevt:int`: number of new event. (`default 1`)

**Returns**

- `evs:evm.event[]`: list of [empty event object](#empty-event-object-methods).


## nevt, err = m:wait( [msec] )

wait until event occurring.

**Parameters**

- `msec:integer`: wait until specified timeout milliseconds. `default: -1(never-timeout)`

**Returns**

- `nevt:integer`: number of the occurred events.
- `err:error`: error object.


### ev, ctx, disabled = m:getevent()

get the event object in which the event occurred.

**Returns**

- `ev:evm.*`: event object (`evm.readable`, `evm.writable`, `evm.timer` or `evm.signal`) or `nil`.
- `ctx:any`: context object.
- `disabled:boolean`: if `true`, event object is disabled.


## Empty Event Object Methods

empty event object `evm.event` can be use as following event object;

- `evm.readable`
- `evm.writable`
- `evm.timer`
- `evm.signal`


## ok, err = ev:astimer( timeout [, ctx [, oneshot]] )

use the event object as a timer event object. (`evm.timer`)

**Parameters**

- `timeout:integer`: timeout milliseconds.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.

**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.


## ok, err = ev:assignal( signo [, ctx [, oneshot]] )

use the event object as a signal event object. (`evm.signal`)

**Parameters**

- `signo:integer`: signal number.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.

**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.


## ok, err = ev:asreadable( fd [, ctx [, oneshot [, edge]]] )

use the event object as a readable event object. (`evm.readable`)

**Parameters**

- `fd:integer`: file descriptor.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.
- `edge:boolean`: if `true`, use `edge-trigger`. `default: level-trigger`.


**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.


## ok, err = ev:aswritable( fd [, ctx [, oneshot [, edge]]] )

use the event object as a writable event object. (`evm.writable`)

**Parameters**

- `fd:integer`: file descriptor.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.
- `edge:boolean`: if `true`, use `edge-trigger`. `default: level-trigger`.

**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.


## Common Methods Of Non-Empty Event Object.


## ev = ev:revert()

revert to an empty event object.

**Returns**

- `ev:evm.event`: empty-event object.


## ok, err = ev:renew( [m] )

attach the event object `ev` to the event monitor `m`.

**Parameters**

- `m:evm`: event monitor object.

**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.


## id = ev:ident()

get an event ident.

**Returns**

- `id`
    - `timeout:integer` if `evm.timer` object.
    - `signo:integer` if `evm.signal` object.
    - `fd:integer` if `evm.readable` or `evm.writable` object.

## asa = ev:asa()

get an event type.


**Returns**

- `asa:string`: `astimer`, `assignal`, `asreadable` or `aswritable`.


## ctx = ev:context( [ctx:any] )

get the context object associated with the event object, and if argument passed then replace that object to passed argument.

**Parameters**

- `ctx:any`: context object.

**Returns**

- `ctx:any`: current context object.


## ev:unwatch()

unregister this event.


## ok, err = ev:watch()

register this event object to the attached event monitor.

**Returns**

- `ok:boolean`: `true` on success, or `false` on failure.
- `err:error`: error object.

