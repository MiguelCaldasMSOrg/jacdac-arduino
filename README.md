# Jacdac for Arduino on BBC micro:bit

Use Jacdac sensors and actuators from Arduino sketches on BBC micro:bit. The library implements the 1 Mbps single-wire bus, discovers connected devices, and provides typed clients for the hardware-verified services below.

Applications can read sensor registers, subscribe to events, control actuators, batch commands, and request acknowledgements without dynamic allocation. Typed clients bind to a specific physical service after their first successful resolution, so a disconnected actuator is not silently replaced by another device.

The implementation follows the Jacdac protocol and draws on the `jacdac-c` and `pxt-jacdac` projects. It is a controller/client library: it lets the micro:bit use peripherals but does not turn the micro:bit into a new Jacdac peripheral.

See [API.md](API.md) for installation, sketch usage, helper functions, wire formats, capacity configuration, and implementation notes.

## Supported targets

- BBC micro:bit V1 (`nRF51822`)
- BBC micro:bit V2 (`nRF52833`)
- Sandeep Mistry nRF5 Arduino core 0.8.0 or compatible
- Arduino board selection: **BBC micro:bit** or **BBC micro:bit V2**
- SoftDevice: **None**
- Jacdac data pin: micro:bit `P12` by default

Default device capacity is 32 on V2 and 16 on V1. Power-provider identities count toward that total. These are memory capacities, not certified physical bus sizes. Serial examples and the bench require V2; dedicated V1 examples avoid the hardware UART conflict.

## Hardware

Use a compliant micro:bit-to-Jacdac adapter and a suitably rated power source with common ground. Check the adapter switch position. Do not assume the micro:bit 3 V pin can power an actuator chain. Wiring and exclusive hardware-resource requirements are in [API.md](API.md#installation-and-target-setup).

## Installation

Install the named `Jacdac-X.Y.Z.zip` release asset through Arduino's **Sketch > Include Library > Add .ZIP Library**. After installation, the supported sketches appear under **File > Examples > Jacdac**. Start with `Discover` to inspect the bus, then use the example for a specific device or `HardwareValidation` for a combined bench test.

The Sandeep Mistry nRF5 core and SoftDevice **None** are required. Follow [API.md](API.md#installation-and-target-setup) for board setup, command-line builds, wiring, the first sketch, and resource ownership.

## Services and examples

The library includes only the 15 peripheral service types physically verified on the V2 bench, plus the required Control service. Every peripheral client has an individual example and bench coverage. **Live** means the service was exercised on connected hardware, not that every product variant or optional operation was tested. Power verification was read-only.

| Service | Client | Individual example | Bench | Evidence |
| --- | --- | --- | --- | --- |
| Button/keycap | `ButtonClient` | [KeycapButton](examples/KeycapButton/KeycapButton.ino) | pressure, events | Live |
| Rotary encoder/button | `RotaryEncoderClient` | [RotaryButton](examples/RotaryButton/RotaryButton.ino) | position, clicker events | Live |
| Slider | `PotentiometerClient` | [Slider](examples/Slider/Slider.ino) | position | Live |
| Magnetic field | `MagneticFieldLevelClient` | [MagneticField](examples/MagneticField/MagneticField.ino) | strength, events | Live |
| Light level | `LightLevelClient` | [LightLevel](examples/LightLevel/LightLevel.ino) | light reading | Live |
| Accelerometer | `AccelerometerClient` | [Accelerometer](examples/Accelerometer/Accelerometer.ino) | XYZ in g | Live |
| Ultrasonic/distance | `DistanceClient` | [Distance](examples/Distance/Distance.ino) | metres | Live |
| Temperature | `TemperatureClient` | [Temperature](examples/Temperature/Temperature.ino) | Celsius | Live |
| Humidity | `HumidityClient` | [Humidity](examples/Humidity/Humidity.ino) | %RH | Live |
| LED ring/short display | `LedClient` | [RgbRing](examples/RgbRing/RgbRing.ino) | `t`: RGB + pixel readback | Live |
| LED-strip controller | `LedStripClient` | [LedStrip](examples/LedStrip/LedStrip.ino) | `t`: RGB program | Live |
| Servo | `ServoClient` | [Servo](examples/Servo/Servo.ino) | `t`: two channels, limits and readback | Live |
| Relay | `RelayClient` | [Relay](examples/Relay/Relay.ino) | `t`: on/off + readback | Live |
| Haptic/vibration | `VibrationMotorClient` | [Haptic](examples/Haptic/Haptic.ino) | `h`: finite pulses + ACK | Live |
| Power provider | `PowerClient` | [Power](examples/Power/Power.ino) | read-only capabilities and status | Live, reads only |

Also available: [Discover](examples/Discover/Discover.ino), [ButtonEvents](examples/ButtonEvents/ButtonEvents.ino), [DeviceCountMatrixV2](examples/DeviceCountMatrixV2/DeviceCountMatrixV2.ino), [ProtocolFeatures](examples/ProtocolFeatures/ProtocolFeatures.ino), and the packaged [HardwareValidation](examples/HardwareValidation/HardwareValidation.ino) bench. V1 entry points are [MicrobitV1](examples/MicrobitV1/MicrobitV1.ino), [PeripheralKitV1](examples/PeripheralKitV1/PeripheralKitV1.ino), and [ProtocolFeaturesV1](examples/ProtocolFeaturesV1/ProtocolFeaturesV1.ino). PeripheralKitV1 is a combined demonstration that operates its LED-strip output automatically; use the individual opt-in actuator examples for initial checks.

## Hardware bench (V2)

[HardwareValidation](examples/HardwareValidation/HardwareValidation.ino) is installed with the library and appears in the Arduino Examples menu. It polls every recognized service in the table, records input ranges and events, and tracks timeouts, unsupported registers, queue overflows, and disconnects. The sketch starts in read-only mode. Output tests require a serial command and never change power-provider settings. Register formats and probe limits are documented in [API.md](API.md).

```powershell
arduino-cli compile --fqbn sandeepmistry:nRF5:BBCmicrobitV2:softdevice=none --library . --output-dir build\bench-v2 examples\HardwareValidation
arduino-cli upload --port COM12 --fqbn sandeepmistry:nRF5:BBCmicrobitV2:softdevice=none --input-file build\bench-v2\HardwareValidation.ino.hex examples\HardwareValidation
```

Open the serial monitor at 115200 baud. The periodic summaries show current readings, observed ranges, request/reply totals, optional-register support, and bus diagnostics. The default mode only reads registers. Serial commands:

| Command | Action |
| --- | --- |
| `s` | Print current measurements and cumulative counters |
| `d` | List currently connected device identifiers and all their advertised service classes |
| `h` | Isolated haptic test: three 200 ms pulses at intensity 128/255, one second apart, followed by an acknowledged stop. Other outputs are not activated. |
| `t` | Run a 45-second output test: first LED and LED-strip instances cycle RGB at brightness 8/255, first relay alternates on/off every five seconds, and up to two servo instances move within their reported ranges. Nominal polling rate is 40 requests/second. |
| `x` | Stop the test and haptic sequence, request LED brightness zero, relay off, and servo power disabled; retry shutdown until readbacks/ACK confirm it, and return to normal polling |

**Before sending `t`, leave relay contacts unloaded and both servo mechanisms unloaded and clear to move.** Use a suitably powered Jacdac adapter. The sketch never changes power-provider settings or servo pulse calibration.

The two output modes (`t` and `h`) are mutually exclusive. Haptic pulse/stop commands request ACKs; feel or observe the motor to confirm physical output. Unsupported register queries are marked and skipped until reset, rather than treated as measured zeros. This includes the optional haptic capacity register and optional power telemetry. Disconnected probe histories are labeled; `d` shows only the live device table.

The LED ring test requires 1-64 reported pixels; the separate strip test requires a nonzero configured pixel count. Observe strip colors visually; ring pixel buffers can also be read back. Brightness, relay state, and servo enable/target values are checked with readback. Servos are disabled first, positioned at the midpoint of their reported limits, then enabled; excursions are capped at 10 degrees either side (or one quarter of a narrower range). Missing or invalid limits prevent startup. Target readback is not physical-position feedback.

For an attached continuous-rotation servo, these controller values request rotation near the motor's neutral pulse rather than physical angles. A positional servo makes small position changes. The controller cannot infer which motor type is attached; keep either type unloaded during the test.

The timer triggers the same shutdown path as `x`. Disconnected outputs remain unconfirmed, with shutdown retried if they reconnect. Loss of the controller or bus can prevent shutdown, so physical power removal must remain available. Counters accumulate until reset; compare before/after values for each run. ACKs confirm receipt, not execution. Standalone actuator examples have finite tests but do not provide the bench's complete readback/retry workflow.

## Validation summary

V2 checks on COM12 on 2026-09-15/16 covered the Live rows above in several configurations of up to 14 device identities:

- Inputs: independent keycaps and rotary clicker, rotary movement, full slider travel, both magnetic polarities, light changes, XYZ acceleration, ultrasonic distance, temperature and humidity.
- Outputs: ring and strip RGB, relay on/off, two servo channels (one positional motor and one continuous-rotation motor), and three short haptic pulses. Physical observations were paired with register readback or ACKs as applicable, followed by shutdown.
- Eight-device load run: 1,895 replies from 1,898 requests and 23 matching ring readbacks. Fourteen-device run: both servo channels matched all nine angle targets, ten relay checks matched, and 22 ring readbacks matched. No queue overflows or disconnects in those timed runs; occasional request loss and background bus errors remain.
- Power verification was read-only: both supply channels allow power, report Powering, and report a 900 mA limit. They do not implement current draw, voltage, battery information, or keep-alive telemetry. Writable behavior of the limit was not tested. See [API.md](API.md#powerclient).

The final audit adds host regressions for ACK classification, capacity, every typed client, fixed-point decoding, limits, output sequencing, retry/stop behavior, and bench coverage. Strict library lint and target compilation are required before publishing. This is interoperability evidence, not full waveform or electrical certification.

## Scope and limitations

V1 is build-tested but not physically validated. Untested product variants may omit optional registers. Increasing device slots does not increase available power or bus bandwidth. CRC-invalid traffic is rejected, not silently accepted; power-provider negotiation can produce visible transient errors. ACK retries do not guarantee exactly-once execution.

Server authoring, persistent role storage, pipes/reliable-command transport, BLE/USB bridging, and non-micro:bit boards are outside the implemented scope. Implementation constraints and delivery semantics are detailed in [API.md](API.md#implementation).

## License

MIT, matching the source Jacdac projects. See [LICENSE](LICENSE).