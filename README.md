# Jacdac for Arduino on BBC micro:bit

This library lets an Arduino sketch act as a Jacdac controller and use Jacdac sensors and actuators. It implements the Jacdac frame format, CRC, multi-packet frames, 1 Mbps half-duplex single-wire transport, bus arbitration, device discovery, register reads/writes, commands, events, the complete service-class catalog, and allocation-free typed clients for common peripherals.

The implementation is based on the protocol and transport behavior in the neighboring `jacdac`, `jacdac-c`, and `pxt-jacdac` projects. It uses fixed-size storage and does not allocate from the heap.

Use the global `Jacdac` bus in sketches. The hardware transport supports one active `Bus`; typed clients bind to the first matching physical service and do not silently switch devices after a disconnect. Call `clearBinding()` when deliberate failover is required.

## Supported targets

- BBC micro:bit V1 (`nRF51822`)
- BBC micro:bit V2 (`nRF52833`)
- Sandeep Mistry nRF5 Arduino core 0.8.0 or compatible
- Arduino board selection: **BBC micro:bit** or **BBC micro:bit V2**
- SoftDevice: **None**
- Jacdac data pin: micro:bit `P12` by default

The V1 transport takes exclusive ownership of UART0 and TIMER2. Do not reference `Serial` in a V1 sketch; UART0 is the only hardware UART, and doing so will produce a duplicate interrupt-handler link error. The V2 transport owns UARTE1 and TIMER3, leaving `Serial` available.

## Hardware

Use a micro:bit-to-Jacdac adapter when possible. The library expects:

| Jacdac signal | micro:bit connection |
| --- | --- |
| `JD_DATA` | `P12` (default) |
| `GND` | `GND` |
| `JD_PWR` | A suitable Jacdac bus power source |

Do not assume that every Jacdac chain can be powered directly from the micro:bit 3 V pin. Check the peripheral current and voltage requirements, use a compliant adapter or external bus supply, and always share ground.

The transport owns `P12` on both boards. It additionally owns UART0 and TIMER2 on V1, or UARTE1 and TIMER3 on V2. Do not use those resources elsewhere in the sketch.

## Installation

Install the nRF5 core if it is not already present:

```powershell
arduino-cli core install sandeepmistry:nRF5
```

Install this repository as an Arduino library by placing or linking it in the Arduino libraries directory. For command-line builds from this checkout:

```powershell
arduino-cli compile --fqbn sandeepmistry:nRF5:BBCmicrobitV2:softdevice=none --library . examples\Discover
arduino-cli compile --fqbn sandeepmistry:nRF5:BBCmicrobit:softdevice=none --library . examples\MicrobitV1
```

GitHub releases provide a minimal `Jacdac-MicroBit-Arduino-X.Y.Z.zip` containing only the Arduino library metadata, source, examples, license, README, and API reference. Download that named asset, not GitHub's automatic **Source code** archives, and install it with **Sketch > Include Library > Add .ZIP Library**. The Arduino ZIP excludes tests, local builds, Git metadata, and editor workspace files.

Maintainers create a release by updating `version` in `library.properties`, committing and pushing that change, then pushing a matching semantic-version tag:

```powershell
git tag -a v0.2.1 -m "Jacdac Arduino 0.2.1"
git push origin v0.2.1
```

The release workflow rejects a tag whose version does not match `library.properties`.

## First sketch

```cpp
#include <Jacdac.h>

using namespace jacdac;

void setup() {
	Serial.begin(115200);
	Jacdac.begin();
}

void loop() {
	Jacdac.process();
}
```

Call `Jacdac.process()` frequently. Packet, device, and ACK subscription handlers run from `process()`, not from an interrupt.

On V1, use the LED, display, or an external software-serial implementation for diagnostics. The `MicrobitV1` example deliberately does not use `Serial`.

## API and usage

See [API.md](API.md) for discovery, packets, commands, registers, callbacks, typed clients, diagnostics, protocol constants, fixed-point conversions, and compile-time configuration.

See [ARCHITECTURE.md](ARCHITECTURE.md) for module boundaries, interrupt/main-loop ownership, fixed-memory design, and extension rules.

## Examples

- `Discover`: prints connected devices and their service classes
- `DeviceCountMatrixV2`: scans the bus and shows the number of responding devices on the micro:bit V2 LED matrix
- `ButtonEvents`: receives button events and polls the pressed register
- `Temperature`: reads an `i22.10` temperature sensor
- `LedAndServo`: controls common actuators
- `MicrobitV1`: V1-safe discovery indicator using the built-in LED and no `Serial`
- `RgbRing`: cycles the entire RGB Ring through test colors
- `RotaryButton`: prints rotary position and push-button events
- `Slider`: prints slider position as a percentage and checks its variant
- `KeycapButton`: prints keycap events and pressure
- `PeripheralKitV1`: V1-safe integrated test with no `Serial`; the slider controls ring brightness, rotation selects a color, the rotary press turns the ring off, and the keycap press turns it white
- `ProtocolFeatures`: V2 serial validation of subscriptions, stable binding, correlated reads, lifecycle/error callbacks, batching, multicast, and control helpers
- `ProtocolFeaturesV1`: V1-safe LED validation of discovery, subscriptions, stable binding, and correlated reads

## Scope and limitations

Implemented:

- Jacdac v1 serial frame encoding, validation, and packed packet iteration
- Controller/client device announce
- Discovery and disconnect expiry
- Generic commands and register operations
- Event/report delivery
- Single-wire receive/transmit and randomized arbitration on nRF51822 and nRF52833
- ACK matching, automatic retries, completion callbacks, and timeout diagnostics
- duplicate-event suppression and sequence ordering
- device restart and report-loss detection
- fixed-capacity callback subscriptions and correlated register reads
- stable service and typed-client bindings, multicast commands, multi-packet command batches, and control-service helpers

Not yet implemented:

- persistent role-manager storage and user-facing role assignment
- pipe/reliable-command transport
- Jacdac server/peripheral authoring
- BLE or USB Jacdac bridge transport
- non-micro:bit Arduino boards

The code compiles for both specified micro:bit targets. Physical bus timing and interoperability still need validation on real hardware with a Jacdac module and preferably a logic analyzer before treating this as production-ready. The V1 backend is more timing-sensitive because UART0 has no EasyDMA: an interrupt runs for every byte, long interrupt-disabled sections can lose data, and other high-priority interrupt workloads can increase collision or frame-error rates. ACK retries reduce packet-loss failures but cannot guarantee exactly-once execution; a device may execute a non-idempotent command and lose its ACK, causing the controller to execute it again.

## License

MIT, matching the source Jacdac projects. See `LICENSE`.