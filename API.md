# Jacdac Arduino API

This document describes the public C++ API exported by `Jacdac.h`. All declarations are in the `jacdac` namespace.

```cpp
#include <Jacdac.h>

using namespace jacdac;
```

The global `Jacdac` object is the default `Bus` instance.

## Bus lifecycle

```cpp
Bus::Bus();
Bus::~Bus();
bool Bus::begin(uint8_t pin = 12);
void Bus::end();
void Bus::process();
bool Bus::running() const;
```

- `Bus` is non-copyable. Only one `Bus` can own the singleton nRF transport at a time; use the global `Jacdac` instance unless tests or application structure require a different instance.
- `begin()` initializes the transport, resets devices, queues, pending operations, and diagnostics, and starts controller announcements. It returns `false` when the target or pin is unsupported. Calling it on a running bus succeeds without reinitializing it.
- `end()` stops the transport so another `Bus` can start. Destroying an active `Bus` also stops it.
- `process()` dispatches received packets and callbacks, manages ACK retries and asynchronous-register timeouts, expires disconnected devices, sends periodic announcements, and advances outgoing traffic. Call it frequently from `loop()`.
- `running()` reports whether initialization succeeded and the bus has not been stopped.

Callbacks registered on `Bus` run synchronously from `process()`, not from an interrupt.

### Operation errors

```cpp
enum class Error : uint8_t {
    None,
    NotRunning,
    TransportUnavailable,
    InvalidArgument,
    InvalidService,
    PacketTooLarge,
    QueueFull,
    NoCapacity,
    DuplicateRequest,
    InvalidSubscription
};

Error Bus::lastError() const;
```

Each synchronous operation that can fail sets `lastError()`. A successful operation clears it to `Error::None`; inspect it immediately after a `false` or `INVALID_SUBSCRIPTION` result. It does not report later asynchronous outcomes: ACK timeouts use `acknowledged == false`, register timeouts pass `nullptr`, and remote command errors use `CommandErrorHandler`.

```cpp
void setup() {
    Jacdac.begin(12);
}

void loop() {
    Jacdac.process();
}
```

## Devices and services

### `Device`

```cpp
struct Device {
    uint64_t deviceIdentifier;
    uint8_t serviceCount;
    uint32_t serviceClasses[MAX_SERVICES_PER_DEVICE];

    bool connected() const;
};
```

`deviceIdentifier` is the Jacdac device identifier. Service index 0 is the control service; `serviceClasses[0]` describes service index 1. Timing, announcement, report, restart, and event-sequencing state is private to `Bus`.

A `Device` reference or pointer addresses internal bus storage. Do not retain it across later calls to `process()`; copy required values instead.

### `Service`

```cpp
struct Service {
    uint64_t deviceIdentifier;
    uint32_t serviceClass;
    uint8_t serviceIndex;

    bool valid() const;
};
```

A `Service` is a lightweight snapshot. Resolve it again after disconnects or device restarts. An invalid service has a zero `deviceIdentifier`.

### Discovery

```cpp
uint8_t Bus::deviceCount() const;
const Device *Bus::device(uint8_t index) const;
Service Bus::findService(uint32_t serviceClass, uint8_t instance = 0) const;
Service Bus::service(uint64_t deviceIdentifier, uint8_t serviceIndex) const;
Service Bus::resolve(const ServiceBinding &binding) const;
```

- `deviceCount()` returns the number of currently connected devices.
- `device(index)` returns the connected device at the zero-based discovery index, or `nullptr`.
- `findService()` returns the zero-based matching instance across connected devices.
- `service()` resolves a device and service index. Index 0 resolves the control service.
- `resolve()` resolves a `ServiceBinding`.

```cpp
Service temperature = Jacdac.findService(service::TEMPERATURE);
if (temperature.valid()) {
    Jacdac.getRegister(temperature, reg::READING);
}
```

### Stable bindings

```cpp
struct ServiceBinding {
    explicit ServiceBinding(uint32_t serviceClass = 0, uint8_t instance = 0);
    bool bound() const;
    void bind(const Service &service);
    void clear();
};
```

An unbound binding resolves by class and instance. After `bind()`, it resolves only the same device and service index. `clear()` returns it to class-and-instance resolution.

```cpp
ServiceBinding role(service::BUTTON, 1);
role.bind(Jacdac.resolve(role));
Service sameButton = Jacdac.resolve(role);
```

## Packets

```cpp
struct PacketView {
    uint64_t deviceIdentifier;
    const uint8_t *data;
    uint16_t serviceCommand;
    uint8_t serviceIndex;
    uint8_t dataSize;
    uint8_t flags;

    bool isCommand() const;
    bool isReport() const;
    bool isRegisterGet() const;
    bool isEvent() const;
    uint16_t registerCode() const;
    uint16_t eventCode() const;
    uint8_t eventCounter() const;
};
```

`data` remains valid only for the duration of the packet callback. Use `readValue()` for aligned native-value extraction:

```cpp
uint32_t reading;
if (packet.isRegisterGet() && packet.registerCode() == reg::READING && readValue(packet, reading)) {
    // use reading
}
```

Register reports use the same `CMD_GET_REGISTER | register` command code as register requests. Events are reports whose command carries an event code and seven-bit sequence counter.

## Commands and registers

```cpp
bool Bus::sendCommand(const Service &service, uint16_t command,
                      const void *data = nullptr, uint8_t size = 0,
                      bool requestAck = false);
bool Bus::getRegister(const Service &service, uint16_t reg);
bool Bus::setRegister(const Service &service, uint16_t reg,
                      const void *data, uint8_t size,
                      bool requestAck = false);
template <typename T>
bool Bus::setRegister(const Service &service, uint16_t reg,
                      const T &value, bool requestAck = false);
```

These methods return whether the request was validated and queued. `false` can indicate a stopped bus, invalid service, oversized packet, full transmit queue, unavailable ACK slot, or duplicate pending ACK request. A successful return does not by itself confirm remote execution.

When `requestAck` is true, the bus makes up to four transmission attempts. Completion is reported through an ACK callback.

```cpp
uint16_t value = 42;
Jacdac.setRegister(target, reg::VALUE, value, true);
```

### Asynchronous register reads

```cpp
using RegisterResponseHandler = void (*)(const PacketView *packet, void *context);

bool Bus::getRegisterAsync(const Service &service, uint16_t reg,
                           RegisterResponseHandler handler,
                           void *context = nullptr,
                           uint32_t timeoutMs = 1000);
```

The callback receives the matching report or `nullptr` on timeout. The method rejects invalid services, null handlers, a full request table, and duplicate requests for the same device, service, and register.

```cpp
void readingReceived(const PacketView *packet, void *) {
    if (packet == nullptr) {
        return;
    }
    uint16_t value;
    if (readValue(*packet, value)) {
        // use value
    }
}

Jacdac.getRegisterAsync(target, reg::READING, readingReceived);
```

### Multicast

```cpp
bool Bus::sendMulticast(uint32_t serviceClass, uint16_t command,
                        const void *data = nullptr, uint8_t size = 0);
```

Queues one broadcast command addressed to every service of the specified nonzero class. Multicast does not request ACKs.

### Command batches

```cpp
class CommandBatch {
public:
    explicit CommandBatch(uint64_t deviceIdentifier, bool requestAck = false);
    bool add(const Service &service, uint16_t command,
             const void *data = nullptr, uint8_t size = 0);
    template <typename T>
    bool add(const Service &service, uint16_t command, const T &value);
    uint8_t packetCount() const;
    Error error() const;
};

bool Bus::sendBatch(const CommandBatch &batch);
```

All services in a batch must belong to the constructor's device. `add()` returns `false` for an invalid or different-device service, invalid payload, or insufficient frame space; `error()` gives the reason. `sendBatch()` rejects an empty batch and copies its error to `Bus::lastError()` when applicable.

```cpp
CommandBatch batch(button.deviceIdentifier, true);
batch.add(button, CMD_GET_REGISTER | reg::BUTTON_ANALOG);
batch.add(button, CMD_GET_REGISTER | reg::READING);
Jacdac.sendBatch(batch);
```

## Control service

```cpp
bool Bus::identify(uint64_t deviceIdentifier, bool requestAck = false);
bool Bus::resetDevice(uint64_t deviceIdentifier, bool requestAck = false);
bool Bus::standby(uint64_t deviceIdentifier, uint32_t durationMs,
                  bool requestAck = false);
bool Bus::setStatusLight(uint64_t deviceIdentifier, uint8_t red,
                         uint8_t green, uint8_t blue, uint8_t speed = 0);
bool Bus::requestDeviceDescription(uint64_t deviceIdentifier);
bool Bus::requestProductIdentifier(uint64_t deviceIdentifier);
bool Bus::requestFirmwareVersion(uint64_t deviceIdentifier);
bool Bus::requestUptime(uint64_t deviceIdentifier);
```

These helpers address service index 0 on the specified device.

## Callbacks and subscriptions

```cpp
using PacketHandler = void (*)(const PacketView &, void *context);
using DeviceHandler = void (*)(const Device &, DeviceEvent, void *context);
using AckHandler = void (*)(uint64_t deviceIdentifier, uint16_t packetCrc,
                            bool acknowledged, void *context);
using CommandErrorHandler = void (*)(const Service &, uint16_t serviceCommand,
                                     uint16_t packetCrc, void *context);
uint8_t Bus::addPacketHandler(PacketHandler handler, void *context = nullptr,
                              uint64_t deviceIdentifier = 0,
                              uint8_t serviceIndex = 0xff,
                              uint16_t serviceCommand = 0xffff);
uint8_t Bus::addDeviceHandler(DeviceHandler handler, void *context = nullptr);
uint8_t Bus::addAckHandler(AckHandler handler, void *context = nullptr);

bool Bus::removePacketHandler(uint8_t subscription);
bool Bus::removeDeviceHandler(uint8_t subscription);
bool Bus::removeAckHandler(uint8_t subscription);
void Bus::setCommandErrorHandler(CommandErrorHandler handler,
                                 void *context = nullptr);
```

All observers use fixed-capacity subscriptions. `addPacketHandler()` supports optional filters: zero matches every device, `0xff` every service index, and `0xffff` every service command. Add methods return `INVALID_SUBSCRIPTION` (`0xff`) when the handler is null or no slot remains. Remove methods return `false` for an invalid or inactive handle.

`DeviceEvent` values are `Connected`, `Disconnected`, `Restarted`, and `ReportsMissed`. An ACK callback receives `acknowledged == false` after all attempts time out. `setCommandErrorHandler()` installs the single replaceable handler for remote `command_not_implemented` reports; passing `nullptr` disables it.

## Typed clients

Typed clients derive from `ServiceClient`. They resolve their zero-based service instance once and then retain a stable binding to that physical device. A disconnect makes the client unresolved instead of silently switching it to another matching device; call `clearBinding()` to select the current instance again.

### `ServiceClient`

```cpp
ServiceClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
bool connected() const;
Service resolve() const;
bool bind(const Service &service);
void clearBinding();
```

`resolve()` creates the stable binding on its first successful lookup. `bind()` accepts only a valid service of the client's configured class and reports `Error::InvalidService` otherwise. Command methods return `false` when the bound service is disconnected or the request cannot be queued.

### `SensorClient`

```cpp
SensorClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
bool requestReading() const;
bool setStreaming(uint8_t samples = 255) const;
bool setStreamingInterval(uint32_t milliseconds) const;
bool setReadingRange(uint32_t range) const;
bool setInactiveThreshold(int32_t threshold) const;
bool setActiveThreshold(int32_t threshold) const;
bool calibrate(bool requestAck = false) const;
bool requestStatus() const;
bool requestPreferredStreamingInterval() const;
bool requestReadingResolution() const;
bool requestInstanceName() const;
bool matchesReading(const PacketView &packet) const;
```

`setStreaming()` writes the requested sample count. For continuous streaming, conventionally refresh `255` before it reaches zero.

### `ActuatorClient`

```cpp
ActuatorClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
bool requestStatus() const;
bool requestInstanceName() const;
```

`ActuatorClient` contains only operations whose wire format is uniform across actuator services. Use a service-specific client or `Bus::setRegister()` with the exact specification type for intensity and value registers.

### `ButtonClient`

```cpp
explicit ButtonClient(Bus &bus, uint8_t instance = 0);
bool requestPressure() const;
bool requestPressed() const;
bool requestAnalog() const;
```

Pressure is the standard `READING` register. Pressed and analog capability are `uint8_t` values. Button-down events carry no payload; button-up and hold events may carry a `uint32_t` duration in milliseconds.

### `RotaryEncoderClient`

```cpp
explicit RotaryEncoderClient(Bus &bus, uint8_t instance = 0);
bool requestPosition() const;
bool requestClicksPerTurn() const;
bool requestClicker() const;
Service buttonService() const;
```

Position is `int32_t`; clicks per turn is `uint16_t`. `buttonService()` returns the immediately following button service when the encoder advertises one.

### `PotentiometerClient`

```cpp
explicit PotentiometerClient(Bus &bus, uint8_t instance = 0);
bool requestPosition() const;
bool requestVariant() const;
```

Position is a `uint16_t` `u0.16` ratio. Variants are defined by `PotentiometerVariant`.

### `LedStripClient`

```cpp
explicit LedStripClient(Bus &bus, uint8_t instance = 0);
bool setBrightness(uint8_t brightness, bool requestAck = false) const;
bool setNumPixels(uint16_t numPixels, bool requestAck = false) const;
bool setMaxPower(uint16_t milliamps, bool requestAck = false) const;
bool setNumRepeats(uint16_t repeats, bool requestAck = false) const;
bool requestActualBrightness() const;
bool requestNumPixels() const;
bool requestMaxPixels() const;
bool requestVariant() const;
bool runProgram(const uint8_t *program, uint8_t size,
                bool requestAck = false) const;
bool setAll(uint8_t red, uint8_t green, uint8_t blue,
            bool requestAck = false) const;
bool setPixel(uint16_t pixel, uint8_t red, uint8_t green, uint8_t blue,
              bool requestAck = false) const;
```

Brightness uses `uint8_t`; RGB operands are red, green, blue. `setPixel()` accepts indexes through 16383. Variants and light types are defined by `LedStripVariant` and `LedStripLightType`.

### `LedClient`

```cpp
explicit LedClient(Bus &bus, uint8_t instance = 0);
bool setBrightness(uint8_t brightness, bool requestAck = false) const;
bool setPixels(const uint8_t *rgb, uint8_t byteCount,
               bool requestAck = false) const;
```

`setPixels()` sends packed RGB bytes to the `VALUE` register.

### `ServoClient`

```cpp
explicit ServoClient(Bus &bus, uint8_t instance = 0);
bool setAngle(float angleDegrees, bool requestAck = false) const;
bool setAngleQ16(int32_t angleDegreesQ16, bool requestAck = false) const;
bool setEnabled(bool enabled, bool requestAck = false) const;
```

`setAngle()` accepts degrees directly. `setAngleQ16()` exposes the signed Q16.16 wire representation for code that already uses fixed-point values. `setEnabled()` writes actuator intensity 1 or 0.

### Peripheral actuator clients

The following clients encode service-specific payload widths:

| Client | Main operations and wire units |
| --- | --- |
| `RelayClient` | `setActive(bool)`, variant and maximum-current requests |
| `LightBulbClient` | `setBrightness(uint16_t)` as `u0.16`, dimmable request |
| `MotorClient` | `setSpeed(int16_t)` as `i1.15`, `setEnabled(bool)` |
| `DualMotorsClient` | `setSpeeds(int16_t, int16_t)` as two `i1.15` values, `setEnabled(bool)` |
| `BuzzerClient` | `setVolume(uint8_t)`, `playTone(periodUs, dutyUs, durationMs)`, `playNote(frequency, volume, durationMs)` |
| `VibrationMotorClient` | `vibrate(const VibrationStep *, uint8_t)`, `stop()`, maximum-sequence request |

`VibrationStep::duration8Milliseconds` is measured in 8 ms units; `intensity` is `u0.8`.

### HID clients

`HidKeyboardClient` provides `key(selector, modifiers, action)` and `clear()`. `HidMouseClient` provides `setButton(buttons, event)`, `move(deltaX, deltaY, timeMilliseconds)`, and `wheel(deltaY, timeMilliseconds)`. `HidJoystickClient` provides repeated-byte button pressures, repeated `i1.15` axes, and capability-register requests. HID selector, modifier, action, button, and event values follow the Jacdac service specifications.

### Display and power clients

`CharacterScreenClient` writes counted message bytes and `u0.16` brightness and requests rows, columns, and variant. `CursorCharacterScreenClient` provides enable, home, clear, cursor-position, counted-message, row, and column operations.

`PowerClient` controls the one-byte `allowed` register and `uint16_t` maximum power in mA. It can request current draw, battery voltage, power status, battery charge, and battery capacity. Power-provider shutdown negotiation is intentionally not exposed as a normal directed client command.

## Value conversion

```cpp
template <typename T>
bool readValue(const PacketView &packet, T &value);

float q10ToFloat(int32_t value);
float uq16ToFloat(uint16_t value);
int32_t floatToQ16(float value);
```

Common wire conversions:

| Format | Conversion |
| --- | --- |
| signed `i22.10` | divide `int32_t` by 1024 |
| signed `i16.16` | divide `int32_t` by 65536 |
| unsigned `u16.16` | divide `uint32_t` by 65536 |
| ratio `u0.16` | divide `uint16_t` by 65535 |
| ratio `u0.8` | divide `uint8_t` by 255 |

`readValue()` copies bytes with `memcpy` and returns `false` when the payload is too short.

## Diagnostics

```cpp
const Diagnostics &Bus::diagnostics() const;
```

Counters reset on `begin()`.

| Field | Meaning |
| --- | --- |
| `framesReceived`, `framesSent` | Valid frames processed and frames transmitted |
| `crcErrors` | Frames rejected by protocol validation |
| `receiveOverflows`, `transmitOverflows` | Full receive or transmit queues |
| `malformedPackets` | Structurally or semantically incomplete packets inside valid frames |
| `deviceOverflows` | Announced devices rejected because the device table is full |
| `busErrors` | Aggregate physical receive errors |
| `collisions` | Arbitration attempts that found the line occupied |
| `acksReceived`, `ackRetries`, `ackTimeouts` | ACK lifecycle counters |
| `duplicateEvents`, `outOfOrderEvents` | Events suppressed by the device-global sequence counter |
| `deviceRestarts`, `missedReports` | Conditions detected from announcements |
| `registerTimeouts` | Asynchronous register requests that expired |
| `commandErrors` | Remote `command_not_implemented` reports |
| `fallingEdges`, `receiveStarts`, `receiveCompletions` | Physical receive progression |
| `receiveBytes` | Total bytes finalized by the receiver |
| `receiveTimeouts`, `receiveShortFrames` | Timed-out or truncated receives |
| `receiveInvalidFrames` | Complete-looking frames rejected by validation |
| `receiveHardwareErrors` | UART/UARTE hardware error events |

Transport counters are synchronized into `Diagnostics` by `process()`.

## Service, register, command, and event constants

Service classes are defined under `jacdac::service`. `JacdacServices.h` includes all 113 service identifiers in the upstream Jacdac catalog at the time of this release, using uppercase underscore names such as `HID_KEYBOARD`, `CHARACTER_SCREEN`, `POWER`, and `DC_CURRENT_MEASUREMENT`. These constants consume no target RAM or linked flash when unused.

Core protocol constants:

| Constant | Value |
| --- | ---: |
| `CMD_ANNOUNCE` | `0x0000` |
| `CMD_EVENT` | `0x0001` |
| `CMD_CALIBRATE` | `0x0002` |
| `CMD_COMMAND_NOT_IMPLEMENTED` | `0x0003` |
| `CMD_GET_REGISTER` | `0x1000` |
| `CMD_SET_REGISTER` | `0x2000` |
| `SERVICE_INDEX_CONTROL` | `0x00` |
| `SERVICE_INDEX_MAX_REGULAR` | `0x3a` |
| `SERVICE_INDEX_BROADCAST` | `0x3d` |
| `SERVICE_INDEX_PIPE` | `0x3e` |
| `SERVICE_INDEX_CRC_ACK` | `0x3f` |

Common events under `jacdac::event` are `ACTIVE`/`BUTTON_DOWN` (`0x01`), `INACTIVE`/`BUTTON_UP` (`0x02`), `VALUE_CHANGED` (`0x03`), and `BUTTON_HOLD` (`0x81`).

Public variants:

| Type | Values |
| --- | --- |
| `LedStripLightType` | `Ws2812bGrb = 0x00`, `Apa102 = 0x10`, `Sk9822 = 0x11` |
| `LedStripVariant` | `Strip = 0x01`, `Ring = 0x02`, `Stick = 0x03`, `Jewel = 0x04`, `Matrix = 0x05` |
| `PotentiometerVariant` | `Slider = 0x01`, `Rotary = 0x02`, `Hall = 0x03` |

Register constants are under `jacdac::reg`; service command constants are under `jacdac::command`. Use `JacdacServices.h` for the included register and command values and the generated Jacdac specification headers as the authority for complete service-specific payload layouts.

## Low-level frame API

Most applications should use `Bus`. The following API is available for frame tooling and protocol tests:

```cpp
uint16_t crc16(const void *data, size_t size);
size_t frameSize(const Frame &frame);
bool validateFrame(const Frame &frame, size_t receivedSize);
void resetFrame(Frame &frame, uint64_t deviceIdentifier, uint8_t flags);
bool appendPacket(Frame &frame, uint8_t serviceIndex, uint16_t serviceCommand,
                  const void *data = nullptr, uint8_t dataSize = 0);
void finalizeFrame(Frame &frame);
bool packetAt(const Frame &frame, size_t &offset, PacketView &packet);
```

`Frame` and `PacketHeader` are packed wire structures. `appendPacket()` returns `false` when the packet does not fit. `packetAt()` advances `offset` while iterating packets and returns `false` when no valid packet remains.

## Compile-time configuration

Define overrides before including `Jacdac.h`.

| Macro | Default | Valid range |
| --- | ---: | --- |
| `JACDAC_MAX_DEVICES` | 8 | 1-254 |
| `JACDAC_MAX_SERVICES_PER_DEVICE` | 16 | 1-58 |
| `JACDAC_RX_QUEUE_SIZE` | 4 | 1-254 |
| `JACDAC_TX_QUEUE_SIZE` | 4 | 1-254 |
| `JACDAC_MAX_ACK_REQUESTS` | 4 | 1-254 |
| `JACDAC_MAX_REGISTER_REQUESTS` | 4 | 1-254 |
| `JACDAC_MAX_SUBSCRIBERS` | 6 | 1-254 |
| `JACDAC_FRAME_DATA_SIZE` | 240 | Multiple of 4 from 8 through 240 |

The default maximum frame data is 240 bytes, of which 236 bytes are available as serial payload after frame overhead. Increasing capacities increases static RAM use.
