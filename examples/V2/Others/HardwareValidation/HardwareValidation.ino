#include <Jacdac.h>

using namespace jacdac;

struct Probe {
    Service target;
    uint16_t registerCode;
    bool pending;
    bool seen;
    bool unsupported;
    double value[3];
    double minimum[3];
    double maximum[3];
    uint32_t requests;
    uint32_t responses;
    uint32_t timeouts;
    uint32_t activeEvents;
    uint32_t inactiveEvents;
    uint32_t changeEvents;
};

struct OutputCheck {
    Service target;
    uint16_t registerCode;
    uint8_t width;
    int32_t desiredValue;
    int32_t verifyingValue;
    bool writePending;
    bool readNeeded;
    bool readPending;
    bool confirmed;
    uint32_t nextAction;
    uint32_t checks;
    uint32_t mismatches;
    uint32_t timeouts;
};

enum class ServoStage : uint8_t { Idle, Disabling, Centering, Enabling, Moving };
enum class HapticAck : uint8_t { None, Pulse, Stop };

struct ServoTest {
    ServoClient client;
    OutputCheck power;
    OutputCheck angle;
    ServoStage stage;
    int32_t centerQ16;
    int32_t lowQ16;
    int32_t highQ16;
    bool nextHigh;
    uint32_t nextMove;

    explicit ServoTest(uint8_t instance) : client(Jacdac, instance), power{}, angle{}, stage(ServoStage::Idle), centerQ16(0), lowQ16(0), highQ16(0), nextHigh(false), nextMove(0) {}
};

static constexpr size_t PROBE_CAPACITY = MAX_DEVICES > 63 ? 254 : MAX_DEVICES * 4;
static Probe probes[PROBE_CAPACITY];
static uint8_t probeCount;
static uint32_t queueFailures;
static uint32_t payloadErrors;
static uint32_t disconnects;
static uint32_t probeOverflows;
static LedClient ring(Jacdac);
static ServoTest servoTests[] = {ServoTest(0), ServoTest(1)};
static LedStripClient strip(Jacdac);
static RelayClient relay(Jacdac);
static VibrationMotorClient haptic(Jacdac);
static HapticAck hapticAwaiting = HapticAck::None;
static bool hapticActive;
static bool hapticStopPending;
static uint64_t hapticIdentifier;
static uint8_t hapticPulseCount;
static uint32_t nextHapticAction;
static uint32_t hapticDeadline;
static uint32_t hapticCommands;
static uint32_t hapticAcknowledged;
static uint32_t hapticAckTimeouts;
static OutputCheck ringPower;
static OutputCheck stripPower;
static OutputCheck relayPower;
static uint32_t nextRelayToggle;
static uint32_t nextOutputAction;
static uint16_t stripPixelCount;
static uint32_t stripWrites;
static uint16_t pixelCount;
static uint8_t expectedPixels[64 * 3];
static uint8_t expectedPixelBytes;
static bool pixelReadPending;
static uint32_t pixelReadAt;
static uint32_t pixelChecks;
static uint32_t pixelMismatches;
static uint32_t pixelTimeouts;
static bool outputActive;
static bool offPending;
static uint32_t outputDeadline;
static uint32_t nextColor;
static uint8_t colorIndex;
static uint32_t queryInterval = 100;

static const char *serviceName(uint32_t serviceClass) {
    switch (serviceClass) {
    case service::BUTTON: return "button";
    case service::ROTARY_ENCODER: return "rotary";
    case service::POTENTIOMETER: return "slider";
    case service::MAGNETIC_FIELD_LEVEL: return "magnet";
    case service::POWER: return "power_status";
    case service::LED: return "led_pixels";
    case service::RELAY: return "relay_active";
    case service::LIGHT_LEVEL: return "light_level";
    case service::LED_STRIP: return "led_strip_pixels";
    case service::ACCELEROMETER: return "accelerometer_g";
    case service::DISTANCE: return "distance_m";
    case service::SERVO: return "servo";
    case service::TEMPERATURE: return "temperature_c";
    case service::HUMIDITY: return "humidity_percent";
    case service::VIBRATION_MOTOR: return "haptic_max_steps";
    default: return "other";
    }
}

static void printIdentifier(uint64_t identifier) {
    for (int8_t shift = 60; shift >= 0; shift -= 4) {
        Serial.print(static_cast<uint8_t>((identifier >> shift) & 15), HEX);
    }
}

static uint16_t probeRegister(uint32_t serviceClass) {
    switch (serviceClass) {
    case service::BUTTON:
    case service::ROTARY_ENCODER:
    case service::POTENTIOMETER:
    case service::MAGNETIC_FIELD_LEVEL:
    case service::LIGHT_LEVEL:
    case service::ACCELEROMETER:
    case service::DISTANCE:
    case service::TEMPERATURE:
    case service::HUMIDITY:
        return reg::READING;
    case service::POWER: return reg::POWER_STATUS;
    case service::LED: return reg::LED_NUM_PIXELS;
    case service::RELAY: return reg::INTENSITY;
    case service::LED_STRIP: return reg::LED_STRIP_NUM_PIXELS;
    case service::SERVO: return reg::VALUE;
    case service::VIBRATION_MOTOR: return reg::VIBRATION_MOTOR_MAX_VIBRATIONS;
    default: return 0xffff;
    }
}

static Probe *findProbe(const Service &target, uint16_t registerCode) {
    for (uint8_t index = 0; index < probeCount; ++index) {
        Probe &probe = probes[index];
        if (probe.target.deviceIdentifier == target.deviceIdentifier && probe.target.serviceIndex == target.serviceIndex && probe.registerCode == registerCode) return &probe;
    }
    return nullptr;
}

static void addProbe(const Service &target, uint16_t registerCode) {
    if (findProbe(target, registerCode) != nullptr) return;
    if (probeCount >= sizeof(probes) / sizeof(probes[0])) {
        ++probeOverflows;
        return;
    }
    Probe &probe = probes[probeCount++];
    probe.target = target;
    probe.registerCode = registerCode;
}

static void collectProbes() {
    for (uint8_t deviceIndex = 0; deviceIndex < Jacdac.deviceCount(); ++deviceIndex) {
        const Device *device = Jacdac.device(deviceIndex);
        for (uint8_t serviceIndex = 1; device != nullptr && serviceIndex <= device->serviceCount; ++serviceIndex) {
            const Service target = Jacdac.service(device->deviceIdentifier, serviceIndex);
            const uint16_t registerCode = probeRegister(target.serviceClass);
            if (registerCode == 0xffff) continue;
            addProbe(target, registerCode);
            if (target.serviceClass == service::SERVO) {
                addProbe(target, reg::INTENSITY);
                addProbe(target, reg::MIN_VALUE);
                addProbe(target, reg::MAX_VALUE);
            } else if (target.serviceClass == service::POWER) {
                const uint16_t registers[] = {reg::INTENSITY, reg::MAX_POWER, reg::READING, reg::POWER_BATTERY_VOLTAGE, reg::POWER_BATTERY_CHARGE, reg::POWER_BATTERY_CAPACITY, reg::POWER_KEEP_ON_PULSE_DURATION, reg::POWER_KEEP_ON_PULSE_PERIOD};
                for (uint16_t code : registers) addProbe(target, code);
            }
        }
    }
}

static void registerReceived(const PacketView *packet, void *context) {
    Probe &probe = *static_cast<Probe *>(context);
    probe.pending = false;
    if (packet == nullptr) {
        if (!probe.unsupported) ++probe.timeouts;
        return;
    }
    double value[3] = {};
    bool valid;
    if (probe.target.serviceClass == service::ACCELEROMETER) {
        int32_t forces[3] = {};
        valid = readValue(*packet, forces);
        for (uint8_t axis = 0; axis < 3; ++axis) value[axis] = forces[axis] / 1048576.0;
    } else if (probe.target.serviceClass == service::DISTANCE || probe.target.serviceClass == service::HUMIDITY) {
        uint32_t reading = 0;
        valid = readValue(*packet, reading);
        value[0] = reading / (probe.target.serviceClass == service::DISTANCE ? 65536.0 : 1024.0);
    } else if (probe.target.serviceClass == service::TEMPERATURE) {
        int32_t temperature = 0;
        valid = readValue(*packet, temperature);
        value[0] = temperature / 1024.0;
    } else if (probe.target.serviceClass == service::SERVO && probe.registerCode != reg::INTENSITY) {
        int32_t angle = 0;
        valid = readValue(*packet, angle);
        value[0] = angle / 65536.0;
    } else if (probe.target.serviceClass == service::ROTARY_ENCODER) {
        int32_t position = 0;
        valid = readValue(*packet, position);
        value[0] = position;
    } else if (probe.target.serviceClass == service::MAGNETIC_FIELD_LEVEL) {
        int16_t strength = 0;
        valid = readValue(*packet, strength);
        value[0] = strength;
    } else if (probe.target.serviceClass == service::POWER) {
        if (probe.registerCode == reg::POWER_STATUS || probe.registerCode == reg::INTENSITY) {
            uint8_t state = 0;
            valid = readValue(*packet, state);
            value[0] = state;
        } else if (probe.registerCode == reg::POWER_BATTERY_CAPACITY) {
            uint32_t capacity = 0;
            valid = readValue(*packet, capacity);
            value[0] = capacity;
        } else {
            uint16_t reading = 0;
            valid = readValue(*packet, reading);
            value[0] = reading;
        }
    } else if (probe.target.serviceClass == service::RELAY || probe.target.serviceClass == service::SERVO || probe.target.serviceClass == service::VIBRATION_MOTOR) {
        uint8_t status = 0;
        valid = readValue(*packet, status);
        value[0] = status;
    } else {
        uint16_t reading = 0;
        valid = readValue(*packet, reading);
        value[0] = reading;
    }
    if (!valid) {
        ++payloadErrors;
        return;
    }
    ++probe.responses;
    const uint8_t components = probe.target.serviceClass == service::ACCELEROMETER ? 3 : 1;
    for (uint8_t component = 0; component < components; ++component) {
        probe.value[component] = value[component];
        if (!probe.seen || value[component] < probe.minimum[component]) probe.minimum[component] = value[component];
        if (!probe.seen || value[component] > probe.maximum[component]) probe.maximum[component] = value[component];
    }
    probe.seen = true;
    const Service currentRing = ring.resolve();
    const Service currentStrip = strip.resolve();
    if (probe.target.serviceClass == service::LED && probe.target.deviceIdentifier == currentRing.deviceIdentifier && probe.target.serviceIndex == currentRing.serviceIndex) {
        pixelCount = static_cast<uint16_t>(value[0]);
    } else if (probe.target.serviceClass == service::LED_STRIP && probe.target.deviceIdentifier == currentStrip.deviceIdentifier && probe.target.serviceIndex == currentStrip.serviceIndex) {
        stripPixelCount = static_cast<uint16_t>(value[0]);
    }
}

static void packetReceived(const PacketView &packet, void *) {
    if (packet.isReport() && packet.serviceCommand == CMD_COMMAND_NOT_IMPLEMENTED && packet.dataSize >= 4) {
        const uint16_t rejected = static_cast<uint16_t>(packet.data[0] | (static_cast<uint16_t>(packet.data[1]) << 8));
        Serial.print("not_implemented "); printIdentifier(packet.deviceIdentifier); Serial.print(':'); Serial.print(packet.serviceIndex);
        Serial.print(" command=0x"); Serial.println(rejected, HEX);
        for (uint8_t index = 0; index < probeCount; ++index) {
            Probe &probe = probes[index];
            if (packet.deviceIdentifier == probe.target.deviceIdentifier && packet.serviceIndex == probe.target.serviceIndex && rejected == (CMD_GET_REGISTER | probe.registerCode)) {
                probe.unsupported = true;
            }
        }
    }
    if (!packet.isEvent()) return;
    for (uint8_t index = 0; index < probeCount; ++index) {
        Probe &probe = probes[index];
        if (packet.deviceIdentifier != probe.target.deviceIdentifier || packet.serviceIndex != probe.target.serviceIndex) continue;
        if (packet.eventCode() == event::ACTIVE) ++probe.activeEvents;
        if (packet.eventCode() == event::INACTIVE) ++probe.inactiveEvents;
        if (packet.eventCode() == event::VALUE_CHANGED) ++probe.changeEvents;
    }
}

static void deviceChanged(const Device &, DeviceEvent event, void *) {
    if (event == DeviceEvent::Disconnected) ++disconnects;
}

static void pixelsReceived(const PacketView *packet, void *) {
    pixelReadPending = false;
    if (packet == nullptr) {
        ++pixelTimeouts;
    } else if (packet->dataSize != expectedPixelBytes || memcmp(packet->data, expectedPixels, expectedPixelBytes) != 0) {
        ++pixelMismatches;
    } else {
        ++pixelChecks;
    }
}

static void requestOutputValue(OutputCheck &output, const Service &target, uint16_t registerCode, uint8_t width, int32_t value, uint32_t now) {
    if (!target.valid()) return;
    output.target = target;
    output.registerCode = registerCode;
    output.width = width;
    output.desiredValue = value;
    output.writePending = true;
    output.confirmed = false;
    output.nextAction = now;
}

static void outputReceived(const PacketView *packet, void *context) {
    OutputCheck &output = *static_cast<OutputCheck *>(context);
    output.readPending = false;
    int32_t value = 0;
    bool matches = false;
    if (packet == nullptr) {
        ++output.timeouts;
    } else {
        if (packet->dataSize == output.width) {
            if (output.width == 1) value = packet->data[0];
            else readValue(*packet, value);
            matches = value == output.verifyingValue;
        }
        if (matches) ++output.checks;
        else ++output.mismatches;
    }
    output.confirmed = matches && output.desiredValue == output.verifyingValue && !output.writePending;
    if (!output.confirmed) output.writePending = true;
    output.nextAction = millis() + 200;
}

static bool processOutputCheck(OutputCheck &output, uint32_t now) {
    if (!output.target.valid() || output.readPending || static_cast<int32_t>(now - output.nextAction) < 0) return false;
    if (!Jacdac.service(output.target.deviceIdentifier, output.target.serviceIndex).valid()) return false;
    if (output.writePending) {
        bool queued;
        if (output.width == 1) {
            const uint8_t value = static_cast<uint8_t>(output.desiredValue);
            queued = Jacdac.setRegister(output.target, output.registerCode, value);
        } else {
            queued = Jacdac.setRegister(output.target, output.registerCode, output.desiredValue);
        }
        if (queued) {
            output.verifyingValue = output.desiredValue;
            output.writePending = false;
            output.readNeeded = true;
            output.nextAction = now + 150;
        } else {
            ++queueFailures;
            output.nextAction = now + 100;
        }
        return true;
    }
    if (output.readNeeded) {
        if (Jacdac.getRegisterAsync(output.target, output.registerCode, outputReceived, &output, 600)) {
            output.readPending = true;
            output.readNeeded = false;
        } else {
            output.nextAction = now + 50;
        }
        return true;
    }
    return false;
}

static bool outputIsOff(const OutputCheck &output) {
    return !output.target.valid() || (output.desiredValue == 0 && output.confirmed);
}

static bool servoRangeReady(ServoTest &test) {
    const Service target = test.client.resolve();
    if (!target.valid()) return false;
    const Probe *minimum = findProbe(target, reg::MIN_VALUE);
    const Probe *maximum = findProbe(target, reg::MAX_VALUE);
    if (minimum == nullptr || maximum == nullptr || !minimum->seen || !maximum->seen || minimum->value[0] >= maximum->value[0]) return false;
    const double center = (minimum->value[0] + maximum->value[0]) / 2.0;
    double excursion = (maximum->value[0] - minimum->value[0]) / 4.0;
    if (excursion > 10.0) excursion = 10.0;
    test.centerQ16 = static_cast<int32_t>(center * 65536.0);
    test.lowQ16 = static_cast<int32_t>((center - excursion) * 65536.0);
    test.highQ16 = static_cast<int32_t>((center + excursion) * 65536.0);
    return true;
}

static void stopHapticTest(uint32_t now) {
    const Service target = haptic.resolve();
    if (target.valid()) hapticIdentifier = target.deviceIdentifier;
    hapticActive = false;
    if (hapticIdentifier != 0) {
        hapticStopPending = true;
        nextHapticAction = now;
    }
}

static void hapticAckReceived(uint64_t identifier, uint16_t, bool acknowledged, void *) {
    if (identifier != hapticIdentifier || hapticAwaiting == HapticAck::None) return;
    const bool stopping = hapticAwaiting == HapticAck::Stop;
    hapticAwaiting = HapticAck::None;
    if (acknowledged) ++hapticAcknowledged;
    else ++hapticAckTimeouts;
    if (stopping) {
        hapticStopPending = !acknowledged;
        nextHapticAction = millis() + 200;
        if (acknowledged) Serial.println("haptic stop acknowledged");
    }
}

static bool startHapticTest(uint32_t now) {
    if (outputActive || offPending || hapticActive || hapticStopPending || hapticAwaiting != HapticAck::None) return false;
    const Service target = haptic.resolve();
    if (!target.valid()) {
        Serial.println("haptic not connected");
        return false;
    }
    hapticIdentifier = target.deviceIdentifier;
    hapticActive = true;
    hapticPulseCount = 0;
    nextHapticAction = now;
    hapticDeadline = now + 5000;
    Serial.println("haptic test started: three 200 ms pulses, intensity 128/255");
    return true;
}

static void processHaptic(uint32_t now) {
    if (hapticActive && static_cast<int32_t>(now - hapticDeadline) >= 0) stopHapticTest(now);
    if (hapticAwaiting != HapticAck::None || static_cast<int32_t>(now - nextHapticAction) < 0) return;
    if (hapticActive && hapticPulseCount == 3) stopHapticTest(now);
    if (hapticStopPending) {
        if (haptic.connected() && haptic.stop(true)) {
            hapticAwaiting = HapticAck::Stop;
            ++hapticCommands;
        } else {
            nextHapticAction = now + 200;
        }
    } else if (hapticActive) {
        const VibrationStep pulse[] = {{25, 128}};
        if (haptic.vibrate(pulse, 1, true)) {
            ++hapticPulseCount;
            ++hapticCommands;
            hapticAwaiting = HapticAck::Pulse;
            nextHapticAction = now + 1000;
        } else {
            ++queueFailures;
            nextHapticAction = now + 100;
        }
    }
}

static void stopOutput() {
    outputActive = false;
    offPending = true;
    queryInterval = 100;
    const uint32_t now = millis();
    stopHapticTest(now);
    for (ServoTest &test : servoTests) {
        test.stage = ServoStage::Idle;
        test.angle.writePending = false;
        test.angle.readNeeded = false;
        requestOutputValue(test.power, test.client.resolve(), reg::INTENSITY, 1, 0, now);
    }
    requestOutputValue(relayPower, relay.resolve(), reg::INTENSITY, 1, 0, now);
    requestOutputValue(ringPower, ring.resolve(), reg::INTENSITY, 1, 0, now);
    requestOutputValue(stripPower, strip.resolve(), reg::INTENSITY, 1, 0, now);
}

static bool startOutputTest(uint32_t now) {
    if (outputActive || offPending || hapticActive || hapticStopPending || hapticAwaiting != HapticAck::None) return false;
    const bool ringReady = ring.connected() && pixelCount > 0 && pixelCount <= 64;
    const bool stripReady = strip.connected() && stripPixelCount > 0;
    const bool relayReady = relay.connected();
    bool servoReady = false;
    for (ServoTest &test : servoTests) {
        if (!test.client.connected()) continue;
        if (!servoRangeReady(test)) {
            Serial.println("servo angle limits unavailable; output test not started");
            return false;
        }
        servoReady = true;
    }
    if (!ringReady && !stripReady && !relayReady && !servoReady) return false;
    if (ringReady) requestOutputValue(ringPower, ring.resolve(), reg::INTENSITY, 1, 8, now);
    if (stripReady) requestOutputValue(stripPower, strip.resolve(), reg::INTENSITY, 1, 8, now);
    if (relayReady) requestOutputValue(relayPower, relay.resolve(), reg::INTENSITY, 1, 1, now);
    for (ServoTest &test : servoTests) {
        if (!test.client.connected()) continue;
        requestOutputValue(test.power, test.client.resolve(), reg::INTENSITY, 1, 0, now);
        test.stage = ServoStage::Disabling;
        test.nextHigh = false;
    }
    outputActive = true;
    outputDeadline = now + 45000;
    nextRelayToggle = now + 5000;
    nextColor = now;
    colorIndex = 0;
    queryInterval = 25;
    Serial.println("output test started: 45 seconds; low-brightness RGB, relay cycling, limited servo motion");
    return true;
}

static void processServo(ServoTest &test, uint32_t now) {
    if (!outputActive) return;
    if (test.stage == ServoStage::Disabling && test.power.confirmed) {
        requestOutputValue(test.angle, test.client.resolve(), reg::VALUE, 4, test.centerQ16, now);
        test.stage = ServoStage::Centering;
    } else if (test.stage == ServoStage::Centering && test.angle.confirmed) {
        requestOutputValue(test.power, test.client.resolve(), reg::INTENSITY, 1, 1, now);
        test.stage = ServoStage::Enabling;
    } else if (test.stage == ServoStage::Enabling && test.power.confirmed) {
        test.stage = ServoStage::Moving;
        test.nextMove = now + 5000;
    } else if (test.stage == ServoStage::Moving && test.angle.confirmed && static_cast<int32_t>(now - test.nextMove) >= 0) {
        requestOutputValue(test.angle, test.client.resolve(), reg::VALUE, 4, test.nextHigh ? test.highQ16 : test.lowQ16, now);
        test.nextHigh = !test.nextHigh;
        test.nextMove = now + 5000;
    }
}

static bool processOutputActions(uint32_t now) {
    for (ServoTest &test : servoTests) {
        if (processOutputCheck(test.power, now)) return true;
    }
    OutputCheck *outputs[] = {&relayPower, &ringPower, &stripPower};
    for (OutputCheck *output : outputs) {
        if (processOutputCheck(*output, now)) return true;
    }
    if (outputActive) {
        for (ServoTest &test : servoTests) {
            if (processOutputCheck(test.angle, now)) return true;
        }
    }
    return false;
}

static void processOutput(uint32_t now) {
    if (outputActive && static_cast<int32_t>(now - outputDeadline) >= 0) stopOutput();
    if (outputActive && relayPower.confirmed && static_cast<int32_t>(now - nextRelayToggle) >= 0) {
        requestOutputValue(relayPower, relayPower.target, reg::INTENSITY, 1, relayPower.desiredValue == 0 ? 1 : 0, now);
        nextRelayToggle = now + 5000;
    }
    bool servosOff = true;
    for (ServoTest &test : servoTests) {
        processServo(test, now);
        if (!outputIsOff(test.power)) servosOff = false;
    }
    if (static_cast<int32_t>(now - nextOutputAction) >= 0 && processOutputActions(now)) nextOutputAction = now + 10;
    if (offPending && servosOff && outputIsOff(relayPower) && outputIsOff(ringPower) && outputIsOff(stripPower) && !hapticStopPending && hapticAwaiting == HapticAck::None) {
        offPending = false;
        Serial.println("test outputs off (readback confirmed)");
    }
    if (pixelReadAt != 0 && !pixelReadPending && static_cast<int32_t>(now - pixelReadAt) >= 0) {
        if (Jacdac.getRegisterAsync(ring.resolve(), reg::VALUE, pixelsReceived, nullptr, 600)) {
            pixelReadPending = true;
            pixelReadAt = 0;
        }
    }
    if (!outputActive || offPending || pixelReadPending || pixelReadAt != 0 || static_cast<int32_t>(now - nextColor) < 0) return;
    static const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    const uint8_t *color = colors[colorIndex];
    if (ringPower.confirmed && ringPower.desiredValue != 0 && ring.connected() && pixelCount > 0 && pixelCount <= 64) {
        expectedPixelBytes = static_cast<uint8_t>(pixelCount * 3);
        for (uint16_t pixel = 0; pixel < pixelCount; ++pixel) memcpy(expectedPixels + pixel * 3, color, 3);
        if (ring.setPixels(expectedPixels, expectedPixelBytes)) pixelReadAt = now + 150;
        else ++queueFailures;
    }
    if (stripPower.confirmed && stripPower.desiredValue != 0 && strip.connected()) {
        if (strip.setAll(color[0], color[1], color[2])) ++stripWrites;
        else ++queueFailures;
    }
    colorIndex = static_cast<uint8_t>((colorIndex + 1) % 3);
    nextColor = now + 2000;
}

static void printReading(uint32_t serviceClass, const double *value) {
    if (serviceClass == service::ACCELEROMETER) {
        Serial.print('[');
        for (uint8_t axis = 0; axis < 3; ++axis) {
            if (axis != 0) Serial.print(',');
            Serial.print(value[axis], 4);
        }
        Serial.print(']');
    } else if (serviceClass == service::DISTANCE || serviceClass == service::SERVO || serviceClass == service::TEMPERATURE || serviceClass == service::HUMIDITY) {
        Serial.print(value[0], 4);
    } else {
        Serial.print(value[0], 0);
    }
}

static void printOutputCheck(const char *name, const OutputCheck &output) {
    if (!output.target.valid()) return;
    Serial.print(name); Serial.print(' '); printIdentifier(output.target.deviceIdentifier); Serial.print(':'); Serial.print(output.target.serviceIndex);
    Serial.print(" target=");
    if (output.width == 4) Serial.print(output.desiredValue / 65536.0, 4);
    else Serial.print(output.desiredValue);
    Serial.print(" confirmed="); Serial.print(output.confirmed ? 1 : 0);
    Serial.print(" checks/mismatch/timeout="); Serial.print(output.checks); Serial.print('/'); Serial.print(output.mismatches); Serial.print('/'); Serial.println(output.timeouts);
    Jacdac.process();
}

static void printSnapshot() {
    const Diagnostics &diagnostics = Jacdac.diagnostics();
    Serial.print("bench ms="); Serial.print(millis());
    Serial.print(" devices="); Serial.print(Jacdac.deviceCount());
    Serial.print(" rx/tx="); Serial.print(diagnostics.framesReceived); Serial.print('/'); Serial.print(diagnostics.framesSent);
    Serial.print(" buserr="); Serial.print(diagnostics.busErrors);
    Serial.print(" overflows(device/rx/tx)="); Serial.print(diagnostics.deviceOverflows); Serial.print('/'); Serial.print(diagnostics.receiveOverflows); Serial.print('/'); Serial.print(diagnostics.transmitOverflows);
    Serial.print(" command_errors="); Serial.print(diagnostics.commandErrors);
    Serial.print(" disconnects="); Serial.print(disconnects);
    Serial.print(" queue_failures="); Serial.print(queueFailures);
    Serial.print(" payload_errors="); Serial.print(payloadErrors);
    Serial.print(" probe_overflows="); Serial.println(probeOverflows);
    Serial.print("bus ACK/retry/timeout="); Serial.print(diagnostics.acksReceived); Serial.print('/'); Serial.print(diagnostics.ackRetries); Serial.print('/'); Serial.println(diagnostics.ackTimeouts);
    Jacdac.process();
    for (uint8_t index = 0; index < probeCount; ++index) {
        const Probe &probe = probes[index];
        Serial.print(serviceName(probe.target.serviceClass)); Serial.print(' ');
        printIdentifier(probe.target.deviceIdentifier); Serial.print(':'); Serial.print(probe.target.serviceIndex);
        Serial.print(" reg=0x"); Serial.print(probe.registerCode, HEX);
        Serial.print(" connected="); Serial.print(Jacdac.service(probe.target.deviceIdentifier, probe.target.serviceIndex).valid() ? 1 : 0);
        Serial.print(" value="); printReading(probe.target.serviceClass, probe.value);
        Serial.print(" min/max="); printReading(probe.target.serviceClass, probe.minimum); Serial.print('/'); printReading(probe.target.serviceClass, probe.maximum);
        Serial.print(" replies/requests="); Serial.print(probe.responses); Serial.print('/'); Serial.print(probe.requests);
        Serial.print(" timeout="); Serial.print(probe.timeouts);
        Serial.print(" unsupported="); Serial.print(probe.unsupported ? 1 : 0);
        Serial.print(" active/inactive/change="); Serial.print(probe.activeEvents); Serial.print('/'); Serial.print(probe.inactiveEvents); Serial.print('/'); Serial.println(probe.changeEvents);
        Jacdac.process();
    }
    Serial.print("ring readback ok/mismatch/timeout="); Serial.print(pixelChecks); Serial.print('/'); Serial.print(pixelMismatches); Serial.print('/'); Serial.println(pixelTimeouts);
    Serial.print("strip program writes="); Serial.println(stripWrites);
    Serial.print("haptic commands/ack/timeouts="); Serial.print(hapticCommands); Serial.print('/'); Serial.print(hapticAcknowledged); Serial.print('/'); Serial.print(hapticAckTimeouts);
    Serial.print(" active="); Serial.print(hapticActive ? 1 : 0); Serial.print(" stop_pending="); Serial.println(hapticStopPending ? 1 : 0);
    printOutputCheck("ring brightness", ringPower);
    printOutputCheck("strip brightness", stripPower);
    printOutputCheck("relay active", relayPower);
    for (const ServoTest &test : servoTests) {
        printOutputCheck("servo enabled", test.power);
        printOutputCheck("servo requested angle", test.angle);
    }
}

static void printDevices() {
    for (uint8_t index = 0; index < Jacdac.deviceCount(); ++index) {
        const Device *device = Jacdac.device(index);
        if (device == nullptr) continue;
        Serial.print("device "); printIdentifier(device->deviceIdentifier);
        Serial.print(" classes=");
        for (uint8_t serviceIndex = 0; serviceIndex < device->serviceCount; ++serviceIndex) {
            if (serviceIndex != 0) Serial.print(',');
            Serial.print(device->serviceClasses[serviceIndex], HEX);
        }
        Serial.println();
        Jacdac.process();
    }
}

void setup() {
    Serial.begin(115200);
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.addDeviceHandler(deviceChanged);
    Jacdac.addAckHandler(hapticAckReceived);
    if (!Jacdac.begin()) Serial.println("Jacdac begin failed");
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    static uint32_t nextScan;
    static uint32_t nextQuery;
    static uint32_t nextSnapshot;
    static uint8_t queryIndex;
    if (static_cast<int32_t>(now - nextScan) >= 0) {
        collectProbes();
        nextScan = now + 1000;
    }
    while (Serial.available()) {
        const int command = Serial.read();
        if (command == 'd') {
            printDevices();
        } else if (command == 'h') {
            startHapticTest(now);
        } else if (command == 't') {
            startOutputTest(now);
        } else if (command == 'x') {
            stopOutput();
        } else if (command == 's') {
            printSnapshot();
        }
    }
    processHaptic(now);
    processOutput(now);
    if (probeCount != 0 && static_cast<int32_t>(now - nextQuery) >= 0) {
        Probe &probe = probes[queryIndex];
        if (!probe.pending && !probe.unsupported && Jacdac.service(probe.target.deviceIdentifier, probe.target.serviceIndex).valid()) {
            if (Jacdac.getRegisterAsync(probe.target, probe.registerCode, registerReceived, &probe, 600)) {
                probe.pending = true;
                ++probe.requests;
            } else if (Jacdac.lastError() != Error::NoCapacity && Jacdac.lastError() != Error::DuplicateRequest) {
                ++queueFailures;
            }
        }
        queryIndex = static_cast<uint8_t>((queryIndex + 1) % probeCount);
        nextQuery = now + queryInterval;
    }
    if (static_cast<int32_t>(now - nextSnapshot) >= 0) {
        printSnapshot();
        nextSnapshot = millis() + 5000;
    }
}