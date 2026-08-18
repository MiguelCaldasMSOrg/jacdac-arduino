# Jacdac for Arduino on BBC micro:bit

This library lets an Arduino sketch act as a Jacdac controller and use Jacdac sensors and actuators. It implements the Jacdac frame format, CRC, multi-packet frames, 1 Mbps half-duplex single-wire transport, bus arbitration, device discovery, register reads/writes, commands, events, and a small set of typed clients.

The implementation is based on the protocol and transport behavior in the neighboring `jacdac`, `jacdac-c`, and `pxt-jacdac` projects. It uses fixed-size storage and does not allocate from the heap.

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

Call `Jacdac.process()` frequently. Packet and device callbacks run from `process()`, not from an interrupt.

On V1, use the LED, display, or an external software-serial implementation for diagnostics. The `MicrobitV1` example deliberately does not use `Serial`.

## Discovery

Devices announce themselves every 500 ms. Each device contains service classes at service indexes starting with 1; service index 0 is its control service.

```cpp
Service temperature = Jacdac.findService(service::TEMPERATURE);
if (temperature.valid()) {
	Jacdac.getRegister(temperature, reg::READING);
}
```

The optional second argument selects an instance across all connected devices:

```cpp
Service secondButton = Jacdac.findService(service::BUTTON, 1);
```

`Device` and `Service` values are lightweight snapshots. Resolve a service again after a disconnect or device reset. `service(deviceIdentifier, 0)` resolves the device's control service.

For multiple services of the same class, `ServiceBinding` can retain a specific device and service index instead of following discovery order:

```cpp
ServiceBinding role(service::BUTTON);
role.bind(Jacdac.findService(service::BUTTON, 1));
Service sameButton = Jacdac.resolve(role);
```

An unbound binding resolves by class and instance. Once bound, it will only resolve the same device and service index; call `clear()` to select a replacement.

## Receiving packets

Register a packet callback and filter by device identifier and service index. Packet payload memory is valid only for the duration of the callback.

```cpp
void packetReceived(const PacketView &packet, void *) {
	Service sensor = Jacdac.findService(service::DISTANCE);
	if (packet.deviceIdentifier != sensor.deviceIdentifier || packet.serviceIndex != sensor.serviceIndex) {
		return;
	}
	if (packet.isRegisterGet() && packet.registerCode() == reg::READING) {
		uint32_t distanceQ16;
		if (readValue(packet, distanceQ16)) {
			float metres = distanceQ16 / 65536.0f;
		}
	}
}
```

Jacdac register reports use the same command code as a register get: `0x1000 | register`. Events use `packet.isEvent()` and `packet.eventCode()`.

## Generic access

The generic API works with any service specification, including service classes not listed in `JacdacServices.h`:

```cpp
constexpr uint32_t MY_SERVICE_CLASS = 0x12345678;
constexpr uint16_t MY_REGISTER = 0x180;

Service target = Jacdac.findService(MY_SERVICE_CLASS);
Jacdac.getRegister(target, MY_REGISTER);

uint16_t value = 42;
Jacdac.setRegister(target, MY_REGISTER, value);

uint8_t arguments[] = {1, 2, 3};
Jacdac.sendCommand(target, 0x80, arguments, sizeof(arguments));
```

Use the generated C headers under `jacdac/dist/c` in the Jacdac specification repository as the authority for service class IDs, register codes, command codes, payload layouts, and fixed-point formats.

Set `requestAck` to `true` for commands that need delivery confirmation:

```cpp
Jacdac.onAck([](uint64_t deviceIdentifier, uint16_t packetCrc, bool acknowledged, void *) {
	// acknowledged is false after all attempts time out.
});
Jacdac.sendCommand(target, command, arguments, argumentSize, true);
```

ACK requests make four total transmission attempts. Retries use the byte-identical frame after 40 ms, then wait 80 ms and 120 ms. By default, at most four ACK requests may be pending. A second identical request to the same device is rejected while the first is pending because its ACK would have the same CRC.

## Typed clients

- `SensorClient`: discovery, reading requests, streaming samples, and streaming interval
- `ButtonClient`: pressure, pressed-state, analog-capability, and button events
- `RotaryEncoderClient`: position, clicks-per-turn, clicker configuration, and the colocated button service
- `PotentiometerClient`: position and physical variant
- `LedStripClient`: brightness, power and geometry configuration, raw programs, all-pixel color, and individual pixels
- `LedClient`: brightness and RGB pixel writes
- `ServoClient`: enabled state and Q16.16 angle writes

Typed clients bind by service-class instance, so they automatically find a replacement after reconnect:

```cpp
SensorClient temperature(Jacdac, service::TEMPERATURE);
temperature.requestReading();

ServoClient servo(Jacdac);
servo.setEnabled(true);
servo.setAngle(floatToQ16(90.0f));
```

### Peripheral modules

| Module | Jacdac service | Typed client | Reading or event payload |
| --- | --- | --- | --- |
| RGB Ring | `LED_STRIP` (`0x126f00e0`) | `LedStripClient` | brightness is `uint8_t`; pixel counts and max power are `uint16_t` |
| Rotary Button | `ROTARY_ENCODER` (`0x10fa29c9`) followed by `BUTTON` | `RotaryEncoderClient` | position is `int32_t`; clicks per turn is `uint16_t` |
| Slider | `POTENTIOMETER` (`0x1f274746`) | `PotentiometerClient` | position is a `uint16_t` `u0.16` ratio; slider variant is `1` |
| Keycap Button | `BUTTON` (`0x1473a263`) | `ButtonClient` | pressure is `uint16_t` `u0.16`; pressed and analog are `uint8_t` |

The Rotary Button advertises its push button immediately after its encoder service. Use `RotaryEncoderClient::buttonService()` to address that button. A separate Keycap Button is another `BUTTON` service and may have a different instance number depending on discovery order; filter packets by both device identifier and service index.

`LedStripClient::setAll()` and `setPixel()` build and run standard LED-strip bytecode programs. Use `runProgram()` when an animation needs commands beyond these helpers. RGB operands are ordered red, green, blue at this API boundary.

Button down carries no payload. Button up and hold events can carry a `uint32_t` duration in milliseconds. Devices may also report changes without being polled, so packet handlers should accept both requested reports and spontaneous events.

The bus suppresses repeated event copies and delivers events in their seven-bit sequence order. If an announcement proves that reports were missed, event tracking is resynchronized so one completely lost event cannot stall later delivery. The related counters are available in `diagnostics()`.

## Asynchronous reads and subscriptions

`getRegisterAsync()` correlates a register report with a callback and reports timeout by passing `nullptr`. A duplicate pending request for the same device, service, and register is rejected because its response would be ambiguous.

```cpp
void pressureReceived(const PacketView *packet, void *) {
	if (packet == nullptr) {
		return;
	}
	uint16_t pressure;
	if (readValue(*packet, pressure)) {
		// Use pressure.
	}
}

Jacdac.getRegisterAsync(button, reg::READING, pressureReceived, nullptr, 1000);
```

The original `onPacket()`, `onDevice()`, and `onAck()` setters remain available. Independent components can additionally use `addPacketHandler()`, `addDeviceHandler()`, and `addAckHandler()`; remove the returned subscription ID when it is no longer needed. Packet subscriptions can filter by device identifier, service index, and service command so callbacks are not invoked for unrelated traffic.

`onDeviceEvent()` reports connections, disconnections, detected restarts, and missed reports. `onCommandError()` reports Jacdac `command_not_implemented` responses; an ACK by itself only confirms receipt.

## Batching, multicast, and control

`CommandBatch` combines commands for services on one device into one frame. It is stack-owned and does not reserve permanent bus memory:

```cpp
CommandBatch batch(deviceIdentifier, true);
batch.add(button, CMD_GET_REGISTER | reg::BUTTON_PRESSED);
batch.add(slider, CMD_GET_REGISTER | reg::READING);
Jacdac.sendBatch(batch);
```

`sendMulticast()` sends one command to every service of a class. Control-service helpers cover identify, reset, standby, status-light control, device description, product identifier, firmware version, and uptime. `ActuatorClient` exposes common intensity/value/status operations for service classes without a dedicated typed client, while `SensorClient` now includes calibration, range, thresholds, status, resolution, preferred interval, and instance-name helpers.

Capacities can be overridden before including the library by defining `JACDAC_MAX_DEVICES`, `JACDAC_MAX_SERVICES_PER_DEVICE`, `JACDAC_RX_QUEUE_SIZE`, `JACDAC_TX_QUEUE_SIZE`, `JACDAC_MAX_ACK_REQUESTS`, `JACDAC_MAX_REGISTER_REQUESTS`, `JACDAC_MAX_SUBSCRIBERS`, or `JACDAC_FRAME_DATA_SIZE`. Device, queue, request, and subscriber capacities must be between 1 and 254; services per device must be between 1 and 60. Frame data size must be a multiple of four between 8 and 240.

## Streaming

Sensors can stream readings instead of being polled. The sample count needs refreshing before it reaches zero; `255` is the conventional request.

```cpp
SensorClient sensor(Jacdac, service::LIGHT_LEVEL);
sensor.setStreamingInterval(100);
sensor.setStreaming(255);
```

For continuous streaming, write `255` again about once per second from `loop()`.

## Fixed-point values

Jacdac specifications use explicit integer and fixed-point formats. Common examples:

| Format | Conversion |
| --- | --- |
| signed `i22.10` | divide `int32_t` by 1024 |
| signed `i16.16` | divide `int32_t` by 65536 |
| unsigned `u16.16` | divide `uint32_t` by 65536 |
| ratio `u0.16` | divide `uint16_t` by 65535 |
| ratio `u0.8` | divide `uint8_t` by 255 |

`readValue()` copies an aligned native value safely from a packet. Do not cast packet payload pointers to multi-byte integer pointers.

## Capacity and diagnostics

Defaults are intentionally bounded for embedded use:

- 8 connected devices
- 16 services per device
- 4 received frames queued between calls to `process()`
- 4 outgoing frames and 4 pending ACK requests
- 4 correlated register requests and 6 additional packet, device, and ACK handlers of each type
- 236 bytes maximum payload per frame

`Jacdac.diagnostics()` reports received/sent frames, CRC errors, queue overflows, bus errors, arbitration collisions, ACKs, retries and timeouts, duplicate and out-of-order events, device restarts, missed reports, register timeouts, and command errors.

## Examples

- `Discover`: prints connected devices and their service classes
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
- stable service bindings, multicast commands, multi-packet command batches, and control-service helpers

Not yet implemented:

- persistent role-manager storage and user-facing role assignment
- pipe/reliable-command transport
- Jacdac server/peripheral authoring
- BLE or USB Jacdac bridge transport
- non-micro:bit Arduino boards

The code compiles for both specified micro:bit targets. Physical bus timing and interoperability still need validation on real hardware with a Jacdac module and preferably a logic analyzer before treating this as production-ready. The V1 backend is more timing-sensitive because UART0 has no EasyDMA: an interrupt runs for every byte, long interrupt-disabled sections can lose data, and other high-priority interrupt workloads can increase collision or frame-error rates. ACK retries reduce packet-loss failures but cannot guarantee exactly-once execution; a device may execute a non-idempotent command and lose its ACK, causing the controller to execute it again.

## License

MIT, matching the source Jacdac projects. See `LICENSE`.