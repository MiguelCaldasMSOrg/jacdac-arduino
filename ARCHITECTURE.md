# Architecture

This library is a fixed-memory Jacdac controller for BBC micro:bit V1 and V2. It has one hardware transport, one active bus owner, and a cooperative main-loop API.

## Modules

| Module | Responsibility |
| --- | --- |
| `JacdacProtocol` | Packed wire structures, CRC, frame validation, packet append, and packet iteration |
| `JacdacTransport` | nRF51/nRF52833 GPIO, UART/UARTE, timer, arbitration, and interrupt state machine |
| `Jacdac` | Queues, discovery, lifecycle, subscriptions, ACK retries, register correlation, and diagnostics |
| `JacdacClients` | Stable service binding and typed sensor/actuator convenience APIs |
| `JacdacServices` | Service classes, registers, commands, events, and public variants |
| `JacdacConfig` | Compile-time capacity and frame-size limits |

Dependencies point inward: clients depend on the bus, the bus depends on protocol and transport, and transport depends on protocol. Protocol code has no Arduino hardware dependency.

## Execution model

Transport interrupt handlers perform timing-sensitive hardware work and invoke only the private bus transport callbacks. The receive callback copies a validated frame into the fixed RX ring. The transmit callback advances the fixed TX ring.

`Bus::process()` owns protocol dispatch and all user callbacks. It drains received frames, handles device state, ACK retries, register timeouts, periodic announcements, and queued transmission. User handlers therefore run outside interrupt context, but they should return promptly to avoid filling the RX queue.

Queue indices shared with interrupts are volatile. Multi-byte frame copies and queue-index changes occur inside interrupt-disabled regions. User code must not access the transport directly.

## Ownership

`NrfTransport` is a singleton because each target has one selected UART/UARTE and timer pair. Only one `Bus` may be active. A second `begin()` fails with `Error::TransportUnavailable`; a non-owning bus cannot stop the active owner. `Bus` is non-copyable and releases the transport from `end()` or its destructor.

The global `Jacdac` instance is the normal sketch entry point. Separate instances are useful for isolated host tests and may run sequentially, not concurrently.

## Memory model

The library performs no heap allocation. Device records, frames, pending operations, and subscriptions live in compile-time-sized arrays. Each ring allocates one extra slot to distinguish full from empty.

Capacity is configured through `JACDAC_MAX_*` macros in `JacdacConfig.h`. A capacity failure is explicit through `lastError()` or a diagnostic counter; the library never allocates a fallback object.

`Device` references address bus-owned table slots and are callback-scoped. `PacketView::data` addresses an RX frame and is packet-callback-scoped. Copy values that must outlive the callback or current `process()` call.

## Service identity

Raw discovery by class and instance is dynamic. `ServiceBinding` stores a device identifier and service index when stable identity is required.

Every typed client derives from `ServiceClient`. Its first successful resolution binds to that physical service. A disconnect makes the client unresolved rather than retargeting another matching actuator or sensor. `clearBinding()` opts into selecting the current class instance again; `bind()` selects an explicit service of the configured class.

## Failure contracts

Immediate validation and capacity failures return `false` or `INVALID_SUBSCRIPTION` and set `Bus::lastError()`. A successful synchronous operation clears the error.

Outcomes that occur after queuing remain asynchronous:

- ACK completion reports `acknowledged` to `AckHandler`.
- Register completion passes a packet or `nullptr` on timeout.
- Remote unsupported-command reports use `CommandErrorHandler`.
- Long-lived physical and protocol conditions accumulate in `Diagnostics`.

Only one identical asynchronous register request may be pending for a device, service, and register. Reports have no request token, so duplicate requests cannot be correlated independently. ACK identity is likewise defined by the wire protocol's device identifier and packet CRC.

## Extension rules

- Keep interrupt handlers bounded and allocation-free.
- Dispatch user code only from `Bus::process()`.
- Preserve frame and packet layout with packed structures and static assertions.
- Add typed clients in `JacdacClients.cpp`; do not add service-specific behavior to `Bus`.
- Route immediate typed-client failures through `lastError()`.
- Add host behavior tests and compile at least one V1 and V2 example for public API changes.
- Treat transport register, timer, pin, and interrupt changes as hardware changes requiring target builds and physical validation.