#include "../src/Jacdac.h"
#include "../src/JacdacTransport.h"

#include <assert.h>

struct TestSerial {
    void begin(unsigned long) {}
    int available() const { return 0; }
    int read() const { return -1; }
    template <typename Value> void print(const Value &, int = 10) {}
    template <typename Value> void println(const Value &, int = 10) {}
    void println() {}
};

static TestSerial Serial;
static constexpr int HEX = 16;

#include "../extras/HardwareValidation/HardwareValidation.ino"

static constexpr uint64_t DEVICE_ID = 0xaabbccddeeff0011ULL;

static Probe makeProbe(uint32_t serviceClass) {
    Probe probe = {};
    probe.target = {DEVICE_ID, serviceClass, 1};
    probe.registerCode = probeRegister(serviceClass);
    return probe;
}

static void deliver(Probe &probe, const uint8_t *data, uint8_t size) {
    probe.pending = true;
    const PacketView packet = {DEVICE_ID, data, static_cast<uint16_t>(CMD_GET_REGISTER | probe.registerCode), 1, size, 0};
    registerReceived(&packet, &probe);
    assert(!probe.pending);
}

static void testRecognizedServices() {
    const struct {
        uint32_t serviceClass;
        uint16_t registerCode;
        const char *name;
    } expected[] = {
        {service::BUTTON, reg::READING, "button"},
        {service::ROTARY_ENCODER, reg::READING, "rotary"},
        {service::POTENTIOMETER, reg::READING, "slider"},
        {service::MAGNETIC_FIELD_LEVEL, reg::READING, "magnet"},
        {service::POWER, reg::POWER_STATUS, "power_status"},
        {service::LED, reg::LED_NUM_PIXELS, "led_pixels"},
        {service::RELAY, reg::INTENSITY, "relay_active"},
        {service::LIGHT_LEVEL, reg::READING, "light_level"},
        {service::LED_STRIP, reg::LED_STRIP_NUM_PIXELS, "led_strip_pixels"},
        {service::ACCELEROMETER, reg::READING, "accelerometer_g"},
        {service::DISTANCE, reg::READING, "distance_m"},
        {service::SERVO, reg::VALUE, "servo"},
        {service::TEMPERATURE, reg::READING, "temperature_c"},
        {service::HUMIDITY, reg::READING, "humidity_percent"},
        {service::VIBRATION_MOTOR, reg::VIBRATION_MOTOR_MAX_VIBRATIONS, "haptic_max_steps"}
    };
    const size_t count = sizeof(expected) / sizeof(expected[0]);
    assert(Jacdac.begin());
    uint8_t services[4 + count * sizeof(uint32_t)] = {1, 1, 1, 0};
    for (size_t index = 0; index < count; ++index) {
        assert(probeRegister(expected[index].serviceClass) == expected[index].registerCode);
        assert(strcmp(serviceName(expected[index].serviceClass), expected[index].name) == 0);
        memcpy(services + 4 + index * sizeof(uint32_t), &expected[index].serviceClass, sizeof(uint32_t));
    }
    Frame frame;
    resetFrame(frame, DEVICE_ID, 0);
    assert(appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, services, sizeof(services)));
    finalizeFrame(frame);
    NrfTransport::instance().injectFrame(frame);
    Jacdac.process();
    collectProbes();
    assert(probeCount == count + 11);
    collectProbes();
    assert(probeCount == count + 11);
    for (size_t index = 0; index < count; ++index) {
        const Service target = Jacdac.service(DEVICE_ID, static_cast<uint8_t>(index + 1));
        const Probe *probe = findProbe(target, expected[index].registerCode);
        assert(probe != nullptr && probe->target.serviceClass == expected[index].serviceClass);
    }
    const Service servoService = Jacdac.findService(service::SERVO);
    assert(findProbe(servoService, reg::INTENSITY) != nullptr);
    assert(findProbe(servoService, reg::MIN_VALUE) != nullptr);
    assert(findProbe(servoService, reg::MAX_VALUE) != nullptr);
    assert(probeRegister(0x12345678) == 0xffff);
    Frame sent;
    assert(NrfTransport::instance().takeSentFrame(sent));
    NrfTransport::instance().completeTransmit();
    Jacdac.process();
    Jacdac.end();
}

static void testScalarReadings() {
    const uint32_t unsigned16Classes[] = {service::BUTTON, service::POTENTIOMETER, service::LIGHT_LEVEL, service::LED, service::LED_STRIP};
    const uint8_t unsigned16[] = {0x34, 0x12};
    for (uint32_t serviceClass : unsigned16Classes) {
        Probe probe = makeProbe(serviceClass);
        deliver(probe, unsigned16, sizeof(unsigned16));
        assert(probe.seen && probe.responses == 1);
        assert(probe.value[0] == 0x1234);
        assert(probe.minimum[0] == probe.maximum[0]);
    }
    const uint8_t active[] = {1};
    const uint32_t unsigned8Classes[] = {service::POWER, service::RELAY};
    for (uint32_t serviceClass : unsigned8Classes) {
        Probe probe = makeProbe(serviceClass);
        deliver(probe, active, sizeof(active));
        assert(probe.value[0] == 1 && probe.responses == 1);
    }
    Probe rotary = makeProbe(service::ROTARY_ENCODER);
    const uint8_t position[] = {0xfe, 0xff, 0xff, 0xff};
    deliver(rotary, position, sizeof(position));
    assert(rotary.value[0] == -2);
    Probe magnet = makeProbe(service::MAGNETIC_FIELD_LEVEL);
    const uint8_t strength[] = {0x00, 0x80};
    deliver(magnet, strength, sizeof(strength));
    assert(magnet.value[0] == -32768);
}

static void testFixedPointAndAxes() {
    Probe distance = makeProbe(service::DISTANCE);
    const uint8_t metres[] = {0x00, 0x80, 0x01, 0x00};
    deliver(distance, metres, sizeof(metres));
    assert(distance.value[0] == 1.5);
    const uint8_t highUnsigned[] = {0x00, 0x00, 0x00, 0x80};
    deliver(distance, highUnsigned, sizeof(highUnsigned));
    assert(distance.minimum[0] == 1.5 && distance.maximum[0] == 32768.0);
    Probe accelerometer = makeProbe(service::ACCELEROMETER);
    const uint8_t firstForces[] = {0x00, 0x00, 0xf0, 0xff, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00};
    deliver(accelerometer, firstForces, sizeof(firstForces));
    assert(accelerometer.value[0] == -1.0 && accelerometer.value[1] == 0.5 && accelerometer.value[2] == 2.0);
    const uint8_t nextForces[] = {0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00, 0x04, 0x00};
    deliver(accelerometer, nextForces, sizeof(nextForces));
    assert(accelerometer.minimum[0] == -1.0 && accelerometer.maximum[0] == 1.0);
    assert(accelerometer.minimum[1] == -2.0 && accelerometer.maximum[1] == 0.5);
    assert(accelerometer.minimum[2] == 0.25 && accelerometer.maximum[2] == 2.0);
    const uint32_t errorsBefore = payloadErrors;
    deliver(accelerometer, nextForces, sizeof(nextForces) - 1);
    assert(accelerometer.responses == 2 && payloadErrors == errorsBefore + 1);
    assert(accelerometer.value[2] == 0.25);
    deliver(distance, metres, sizeof(metres) - 1);
    assert(distance.responses == 2 && payloadErrors == errorsBefore + 2);
    assert(distance.maximum[0] == 32768.0);
    Probe servoProbe = makeProbe(service::SERVO);
    const uint8_t angle[] = {0x00, 0x80, 0x5a, 0x00};
    deliver(servoProbe, angle, sizeof(angle));
    assert(servoProbe.value[0] == 90.5);
    const uint8_t negativeAngle[] = {0x00, 0x80, 0xff, 0xff};
    deliver(servoProbe, negativeAngle, sizeof(negativeAngle));
    assert(servoProbe.minimum[0] == -0.5 && servoProbe.maximum[0] == 90.5);
    servoProbe.registerCode = reg::INTENSITY;
    const uint8_t enabled[] = {1};
    deliver(servoProbe, enabled, sizeof(enabled));
    assert(servoProbe.value[0] == 1);
    distance.pending = true;
    registerReceived(nullptr, &distance);
    assert(!distance.pending && distance.timeouts == 1 && distance.responses == 2);
}

static PacketView takeOutputPacket(Frame &frame) {
    assert(NrfTransport::instance().takeSentFrame(frame));
    PacketView packet;
    size_t offset = 0;
    assert(packetAt(frame, offset, packet));
    assert(!packetAt(frame, offset, packet));
    NrfTransport::instance().completeTransmit();
    Jacdac.process();
    return packet;
}

static void completeOutputCycle(OutputCheck &output, uint32_t now, bool writeAlreadyQueued = false) {
    if (!writeAlreadyQueued) assert(processOutputCheck(output, now));
    Frame frame;
    PacketView packet = takeOutputPacket(frame);
    assert(packet.isCommand());
    assert(packet.deviceIdentifier == output.target.deviceIdentifier && packet.serviceIndex == output.target.serviceIndex);
    assert(packet.serviceCommand == (CMD_SET_REGISTER | output.registerCode));
    assert(packet.dataSize == output.width);
    int32_t value = 0;
    if (output.width == 1) value = packet.data[0];
    else assert(readValue(packet, value));
    assert(value == output.desiredValue);
    assert(!output.confirmed);
    assert(processOutputCheck(output, now + 150));
    packet = takeOutputPacket(frame);
    assert(packet.isCommand() && packet.serviceCommand == (CMD_GET_REGISTER | output.registerCode));
    assert(packet.dataSize == 0 && packet.serviceIndex == output.target.serviceIndex);
    Frame report;
    resetFrame(report, output.target.deviceIdentifier, 0);
    assert(appendPacket(report, output.target.serviceIndex, CMD_GET_REGISTER | output.registerCode, &value, output.width));
    finalizeFrame(report);
    NrfTransport::instance().injectFrame(report);
    Jacdac.process();
    assert(output.confirmed && !output.readPending);
}

static void testOutputReadbackRetries() {
    OutputCheck output = {};
    const Service target = {DEVICE_ID, service::SERVO, 1};
    requestOutputValue(output, target, reg::INTENSITY, 1, 1, 0);
    output.verifyingValue = 1;
    output.writePending = false;
    output.readPending = true;
    requestOutputValue(output, target, reg::INTENSITY, 1, 0, 1);
    const uint8_t enabled = 1;
    PacketView packet = {DEVICE_ID, &enabled, 0x1001, 1, 1, 0};
    outputReceived(&packet, &output);
    assert(output.desiredValue == 0 && output.writePending && !output.confirmed);
    output.verifyingValue = 0;
    output.writePending = false;
    output.readPending = true;
    outputReceived(nullptr, &output);
    assert(output.timeouts == 1 && output.writePending && !output.confirmed);
    output.writePending = false;
    output.readPending = true;
    outputReceived(&packet, &output);
    assert(output.mismatches == 1 && output.writePending && !output.confirmed);
    const uint8_t disabled = 0;
    packet.data = &disabled;
    output.writePending = false;
    output.readPending = true;
    outputReceived(&packet, &output);
    assert(outputIsOff(output));
}

static void testEnvironmentalReadings() {
    assert(probeRegister(service::TEMPERATURE) == reg::READING);
    assert(probeRegister(service::HUMIDITY) == reg::READING);
    assert(probeRegister(service::VIBRATION_MOTOR) == reg::VIBRATION_MOTOR_MAX_VIBRATIONS);
    assert(strcmp(serviceName(service::TEMPERATURE), "temperature_c") == 0);
    assert(strcmp(serviceName(service::HUMIDITY), "humidity_percent") == 0);
    assert(strcmp(serviceName(service::VIBRATION_MOTOR), "haptic_max_steps") == 0);
    Probe temperature = makeProbe(service::TEMPERATURE);
    const uint8_t belowZero[] = {0, 0xce, 0xff, 0xff};
    deliver(temperature, belowZero, sizeof(belowZero));
    assert(temperature.value[0] == -12.5);
    const uint8_t roomTemperature[] = {0, 0x65, 0, 0};
    deliver(temperature, roomTemperature, sizeof(roomTemperature));
    assert(temperature.value[0] == 25.25 && temperature.minimum[0] == -12.5);
    Probe humidity = makeProbe(service::HUMIDITY);
    const uint8_t relativeHumidity[] = {0, 0xb5, 0, 0};
    deliver(humidity, relativeHumidity, sizeof(relativeHumidity));
    assert(humidity.value[0] == 45.25);
    const uint8_t highUnsigned[] = {0, 0, 0, 0x80};
    deliver(humidity, highUnsigned, sizeof(highUnsigned));
    assert(humidity.value[0] == 2097152.0);
    const uint32_t errorsBefore = payloadErrors;
    deliver(temperature, roomTemperature, 3);
    deliver(humidity, relativeHumidity, 3);
    assert(payloadErrors == errorsBefore + 2 && temperature.responses == 2 && humidity.responses == 2);
    Probe hapticProbe = makeProbe(service::VIBRATION_MOTOR);
    const uint8_t maxSteps[] = {8};
    deliver(hapticProbe, maxSteps, sizeof(maxSteps));
    assert(hapticProbe.value[0] == 8 && hapticProbe.responses == 1);
}

static void testRelayAndServoSequence() {
    ring.clearBinding();
    strip.clearBinding();
    relay.clearBinding();
    for (ServoTest &test : servoTests) test.client.clearBinding();
    ServoTest &firstServo = servoTests[0];
    ServoTest &secondServo = servoTests[1];
    probeCount = 0;
    memset(probes, 0, sizeof(probes));
    jacdacTestSetMillis(0);
    assert(Jacdac.begin());
    Frame frame;
    takeOutputPacket(frame);
    const uint32_t serviceClasses[] = {service::LED, service::LED_STRIP, service::RELAY, service::SERVO, service::SERVO};
    uint8_t services[4 + sizeof(serviceClasses)] = {1, 1, 1, 0};
    memcpy(services + 4, serviceClasses, sizeof(serviceClasses));
    resetFrame(frame, DEVICE_ID, 0);
    assert(appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, services, sizeof(services)));
    finalizeFrame(frame);
    NrfTransport::instance().injectFrame(frame);
    Jacdac.process();
    collectProbes();
    assert(!NrfTransport::instance().takeSentFrame(frame));
    assert(!startOutputTest(0));
    assert(!outputActive);
    const uint8_t pixels[] = {8, 0};
    deliver(*findProbe(ring.resolve(), reg::LED_NUM_PIXELS), pixels, sizeof(pixels));
    deliver(*findProbe(strip.resolve(), reg::LED_STRIP_NUM_PIXELS), pixels, sizeof(pixels));
    const uint8_t minimum[] = {0, 0, 0, 0};
    const uint8_t maximum[] = {0, 0, 180, 0};
    deliver(*findProbe(firstServo.client.resolve(), reg::MIN_VALUE), minimum, sizeof(minimum));
    deliver(*findProbe(firstServo.client.resolve(), reg::MAX_VALUE), maximum, sizeof(maximum));
    assert(!startOutputTest(0));
    const uint8_t secondMinimum[] = {0, 0, 0xe2, 0xff};
    const uint8_t secondMaximum[] = {0, 0, 30, 0};
    deliver(*findProbe(secondServo.client.resolve(), reg::MIN_VALUE), secondMinimum, sizeof(secondMinimum));
    deliver(*findProbe(secondServo.client.resolve(), reg::MAX_VALUE), secondMaximum, sizeof(secondMaximum));
    assert(startOutputTest(0));
    assert(firstServo.centerQ16 == 90 * 65536 && firstServo.lowQ16 == 80 * 65536 && firstServo.highQ16 == 100 * 65536);
    assert(secondServo.centerQ16 == 0 && secondServo.lowQ16 == -10 * 65536 && secondServo.highQ16 == 10 * 65536);
    assert(firstServo.power.target.serviceIndex == 4 && secondServo.power.target.serviceIndex == 5);
    for (ServoTest &test : servoTests) {
        assert(test.stage == ServoStage::Disabling && test.power.desiredValue == 0);
        processServo(test, 0);
        assert(test.stage == ServoStage::Disabling);
        completeOutputCycle(test.power, 0);
    }
    completeOutputCycle(relayPower, 200);
    assert(relayPower.desiredValue == 1);
    completeOutputCycle(ringPower, 400);
    completeOutputCycle(stripPower, 600);
    for (ServoTest &test : servoTests) {
        processServo(test, 800);
        assert(test.stage == ServoStage::Centering && test.power.desiredValue == 0);
        completeOutputCycle(test.angle, 800);
        processServo(test, 1100);
        assert(test.stage == ServoStage::Enabling && test.power.desiredValue == 1);
        completeOutputCycle(test.power, 1100);
        processServo(test, 1400);
        assert(test.stage == ServoStage::Moving);
    }
    nextColor = 60000;
    processOutput(5000);
    assert(relayPower.desiredValue == 0);
    completeOutputCycle(relayPower, 5000, true);
    for (ServoTest &test : servoTests) {
        processServo(test, 6400);
        assert(test.angle.desiredValue == test.lowQ16);
        completeOutputCycle(test.angle, 6400);
        processServo(test, 11400);
        assert(test.angle.desiredValue == test.highQ16);
        completeOutputCycle(test.angle, 11400);
    }
    processOutput(45000);
    assert(!outputActive && offPending && firstServo.stage == ServoStage::Idle && secondServo.stage == ServoStage::Idle);
    completeOutputCycle(firstServo.power, 45000, true);
    completeOutputCycle(secondServo.power, 45000);
    completeOutputCycle(relayPower, 45200);
    completeOutputCycle(ringPower, 45400);
    completeOutputCycle(stripPower, 45600);
    processOutput(45800);
    assert(!offPending && outputIsOff(firstServo.power) && outputIsOff(secondServo.power) && outputIsOff(relayPower));
    assert(!NrfTransport::instance().takeSentFrame(frame));

    assert(startOutputTest(46000));
    stopOutput();
    for (ServoTest &test : servoTests) {
        processServo(test, 46000);
        assert(test.stage == ServoStage::Idle && !outputActive && test.power.desiredValue == 0);
        completeOutputCycle(test.power, 46000);
    }
    completeOutputCycle(relayPower, 46200);
    completeOutputCycle(ringPower, 46400);
    completeOutputCycle(stripPower, 46600);
    processOutput(47000);
    assert(!offPending && outputIsOff(firstServo.power) && outputIsOff(secondServo.power) && outputIsOff(relayPower));
    assert(!NrfTransport::instance().takeSentFrame(frame));
    Jacdac.end();
}

static void acknowledgeFrame(const Frame &frame) {
    Frame reply;
    resetFrame(reply, frame.deviceIdentifier, 0);
    assert(appendPacket(reply, SERVICE_INDEX_CRC_ACK, frame.crc));
    finalizeFrame(reply);
    NrfTransport::instance().injectFrame(reply);
    Jacdac.process();
}

static void testHapticSequence() {
    probeCount = 0;
    memset(probes, 0, sizeof(probes));
    jacdacTestSetMillis(0);
    assert(Jacdac.begin());
    Frame frame;
    takeOutputPacket(frame);
    const uint8_t services[] = {1, 1, 1, 0, 0xa2, 0xc4, 0x3f, 0x18};
    resetFrame(frame, DEVICE_ID, 0);
    assert(appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, services, sizeof(services)));
    finalizeFrame(frame);
    NrfTransport::instance().injectFrame(frame);
    Jacdac.process();
    collectProbes();
    assert(probeCount == 1);
    const uint8_t rejectedRegister[] = {0x80, 0x11, 0, 0};
    const PacketView rejected = {DEVICE_ID, rejectedRegister, CMD_COMMAND_NOT_IMPLEMENTED, 1, sizeof(rejectedRegister), 0};
    packetReceived(rejected, nullptr);
    assert(probes[0].unsupported);
    probes[0].pending = true;
    registerReceived(nullptr, &probes[0]);
    assert(!probes[0].pending && probes[0].timeouts == 0);
    const uint8_t subscription = Jacdac.addAckHandler(hapticAckReceived);
    assert(subscription != INVALID_SUBSCRIPTION);
    assert(startHapticTest(0));
    assert(!startOutputTest(0));
    for (uint8_t pulse = 0; pulse < 3; ++pulse) {
        processHaptic(pulse * 1000);
        const PacketView packet = takeOutputPacket(frame);
        assert(packet.isCommand() && packet.serviceIndex == 1 && packet.serviceCommand == 0x80);
        assert(packet.dataSize == 2 && packet.data[0] == 25 && packet.data[1] == 128);
        assert((packet.flags & FRAME_FLAG_ACK_REQUESTED) != 0);
        acknowledgeFrame(frame);
    }
    processHaptic(3000);
    PacketView packet = takeOutputPacket(frame);
    assert(packet.serviceCommand == 0x80 && packet.dataSize == 0);
    acknowledgeFrame(frame);
    assert(!hapticActive && !hapticStopPending && hapticAwaiting == HapticAck::None);
    assert(hapticCommands == 4 && hapticAcknowledged == 4 && hapticAckTimeouts == 0);

    assert(startHapticTest(4000));
    processHaptic(4000);
    takeOutputPacket(frame);
    stopHapticTest(4050);
    acknowledgeFrame(frame);
    processHaptic(4200);
    packet = takeOutputPacket(frame);
    assert(packet.serviceCommand == 0x80 && packet.dataSize == 0);
    acknowledgeFrame(frame);
    processHaptic(6000);
    assert(!NrfTransport::instance().takeSentFrame(frame));
    assert(hapticPulseCount == 1 && !hapticActive && !hapticStopPending);
    assert(Jacdac.removeAckHandler(subscription));
    Jacdac.end();
}

static void testPowerReadings() {
    Probe power = makeProbe(service::POWER);
    const uint16_t registers[] = {reg::MAX_POWER, reg::READING, reg::POWER_BATTERY_VOLTAGE, reg::POWER_BATTERY_CHARGE, reg::POWER_KEEP_ON_PULSE_DURATION, reg::POWER_KEEP_ON_PULSE_PERIOD};
    const uint8_t data[] = {0x88, 0x13};
    for (uint16_t code : registers) {
        power.registerCode = code;
        deliver(power, data, sizeof(data));
        assert(power.value[0] == 5000);
    }
    power.registerCode = reg::POWER_BATTERY_CAPACITY;
    const uint8_t capacity[] = {0x00, 0x00, 0x00, 0x80};
    deliver(power, capacity, sizeof(capacity));
    assert(power.value[0] == 2147483648.0);
    const uint32_t errorsBefore = payloadErrors;
    deliver(power, capacity, 3);
    assert(payloadErrors == errorsBefore + 1);
    const uint8_t state[] = {3};
    power.registerCode = reg::POWER_STATUS;
    deliver(power, state, sizeof(state));
    assert(power.value[0] == static_cast<uint8_t>(PowerStatus::Overprovision));
}

int main() {
    testRecognizedServices();
    testScalarReadings();
    testFixedPointAndAxes();
    testOutputReadbackRetries();
    testEnvironmentalReadings();
    testPowerReadings();
    testRelayAndServoSequence();
    testHapticSequence();
    return 0;
}