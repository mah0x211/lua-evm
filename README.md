lua-sentry
===

kqueue/epoll event sentry module.

**NOTE: Do not use this module. this module is under heavy development.**


## Installation

```sh
luarocks install sentry --from=http://mah0x211.github.io/rocks/
```

## Constants

**Event Types**

- `sentry.EV_READABLE` readable event.
- `sentry.EV_WRITABLE` writable event.
- `sentry.EV_TIMER` timer event.
- `sentry.EV_SIGNAL` signal event.


## Creating a Sentry Object

### s, err = sentry.new( [bufsize:int] );

returns the sentry object.


**Parameters**

- `bufsize:int`: event buffer size. (`default 128`)


**Returns**

- `s:userdata`: sentry object on success, or nil on failure.
- `err:string`: error string.


**NOTE: bufsize will be automatically resized to larger than specified size if need more buffer allocation.**


### s, err = sentry.default( [bufsize:int] );

returns the default sentry object.


**Parameters** and **Returns** are same as sentry.new function.


## Sentry Methods


### ev, err = s:newevent();

returns the new [empty event object](#empty-event-object-methods).

**Returns**

- `ev:userdata`: event object on success, or nil on failure.
- `err:string`: error string.


### nevt, err = s:wait( [timeout:int] )

wait until event occurring.


**Parameters**

- `timeout:int`: wait until specified timeout seconds. `default: -1(never-timeout)`


**Returns**

- `nevt:int`: number of the occurred events.
- `err:string`: error string.


### ev, evtype, ishup, ctx = s:getevent()

returns an event object.


**Returns**

- `ev:userdata`: event object or nil.
- `evtype:int`: event type.
- `ishup:boolean`: true on hang-up events.
- `ctx:any`: context object.


## Empty Event Object Methods

empty event object can be use as following event object;

### err = ev:astimer( timeout:number [, ctx:any [, oneshot:boolean]] )

to use the event object as a timer event.

**Parameters**

- `timeout:number`: timeout seconds.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.


**Returns**

- `err:string`: nil on success, or error string on failure.


### err = ev:assignal( signo:int [, ctx:any [, oneshot:boolean]] )

to use as a signal event.

**Parameters**

- `signo:int`: signal number.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.


**Returns**

- `err:string`: nil on success, or error string on failure.


### err = ev:asreadable( fd:int [, ctx:any [, oneshot:boolean [, edge:boolean]]] )

to use as a readable event.

**Parameters**

- `fd:int`: descriptor.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.
- `edge:boolean`: using an edge-trigger if specified a true. `default: level-trigger`.


**Returns**

- `err:string`: nil on success, or error string on failure.


### err = ev:aswritable( fd:int [, ctx:any [, oneshot:boolean [, edge:boolean]]] )

to use as a writable event.

**Parameters**

- `fd:int`: descriptor.
- `ctx:any`: context object.
- `oneshot:boolean`: automatically unregister this event when event occurred.
- `edge:boolean`: using an edge-trigger if specified a true. `default: level-trigger`.


**Returns**

- `err:string`: nil on success, or error string on failure.


## Common Methods Of Non-Empty Event Object.


### ev = ev:revert()

revert to an empty event object.

**Returns**

- `ev:userdata`: empty event object.


### id = ev:ident()

returns an event ident.


**Returns**

- `id`
    - `timeout:number` if timer event.
    - `signo:int` if signal event.
    - `fd:int` if readable or writable event.


### evtype = ev:typeof()

returns an event type.


**Returns**

- `evtype:int`: following type.
    - `EV_READABLE` readable event.
    - `EV_WRITABLE` writable event.
    - `EV_TIMER` timer event.
    - `EV_SIGNAL` signal event.


### ctx = ev:context()

returns a context object.


### ev:unwatch()

unregister this event.


### ok, err = ev:watch()

register this event.


**Returns**

- `ok:boolean`: true on success, or false on failure.
- `err:string`: error string.

