#include "../src/Jacdac.h"
#include "../src/JacdacTransport.h"

#include <assert.h>
#include <string.h>

using namespace jacdac;

static constexpr uint64_t DEVICE_ID = 0xaabbccddeeff0011ULL;
static int packetCalls;
static int filteredCalls;
static int deviceConnects;
static int subscribedDeviceConnects;
static int lifecycleRestarts;
static int lifecycleMisses;
static int registerResponses;
static int registerTimeouts;
static int commandErrors;
static int ackSuccesses;
static int ackFailures;
static int interleavedEventCalls;

static Frame frameWithPacket(uint64_t deviceIdentifier, uint8_t flags, uint8_t serviceIndex, uint16_t command, const void *data = nullptr, uint8_t size = 0) {
    Frame frame;
    resetFrame(frame, deviceIdentifier, flags);
    assert(appendPacket(frame, serviceIndex, command, data, size));
    finalizeFrame(frame);
    return frame;
}

static Frame announce(uint8_t restartCounter, uint8_t packetCount) {
    const uint8_t data[] = {restartCounter, 0x01, packetCount, 0, 0x63, 0xa2, 0x73, 0x14};
    return frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, data, sizeof(data));
}

static Frame announceServices(const uint32_t *serviceClasses, uint8_t count) {
    uint8_t data[4 + MAX_SERVICES_PER_DEVICE * sizeof(uint32_t)] = {1, 1, 1, 0};
    assert(count <= MAX_SERVICES_PER_DEVICE);
    memcpy(data + 4, serviceClasses, count * sizeof(uint32_t));
    return frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, data, static_cast<uint8_t>(4 + count * sizeof(uint32_t)));
}

static void inject(Bus &bus, const Frame &frame) {
    NrfTransport::instance().injectFrame(frame);
    bus.process();
}

static void packetHandler(const PacketView &, void *) { ++packetCalls; }
static void filteredHandler(const PacketView &, void *) { ++filteredCalls; }
static void deviceHandler(const Device &, DeviceEvent event, void *) { if (event == DeviceEvent::Connected) ++deviceConnects; }
static void subscribedDeviceHandler(const Device &, DeviceEvent event, void *) { if (event == DeviceEvent::Connected) ++subscribedDeviceConnects; }

static void lifecycleHandler(const Device &, DeviceEvent event, void *) {
    if (event == DeviceEvent::Restarted) ++lifecycleRestarts;
    if (event == DeviceEvent::ReportsMissed) ++lifecycleMisses;
}

static void registerHandler(const PacketView *packet, void *) {
    if (packet == nullptr) ++registerTimeouts;
    else ++registerResponses;
}

static void commandErrorHandler(const Service &source, uint16_t command, uint16_t crc, void *) {
    assert(source.valid());
    assert(source.serviceClass == service::CONTROL);
    assert(source.serviceIndex == SERVICE_INDEX_CONTROL);
    assert(command == 0x88);
    assert(crc == 0x1234);
    ++commandErrors;
}

static void ackSubscription(uint64_t deviceIdentifier, uint16_t, bool acknowledged, void *) {
    assert(deviceIdentifier == DEVICE_ID);
    if (acknowledged) ++ackSuccesses;
    else ++ackFailures;
}

static void interleavedEventHandler(const PacketView &packet, void *) {
    if (packet.isEvent()) ++interleavedEventCalls;
}

static void finishPendingTransmission(Bus &bus) {
    Frame frame;
    if (NrfTransport::instance().takeSentFrame(frame)) {
        NrfTransport::instance().completeTransmit();
        bus.process();
    }
}

static PacketView takePacket(Bus &bus, Frame &frame) {
    assert(NrfTransport::instance().takeSentFrame(frame));
    size_t offset = 0;
    PacketView packet;
    assert(packetAt(frame, offset, packet));
    assert(!packetAt(frame, offset, packet));
    NrfTransport::instance().completeTransmit();
    bus.process();
    return packet;
}

static void assertPayload(const PacketView &packet, uint16_t commandCode, const uint8_t *expected, uint8_t size) {
    assert(packet.serviceCommand == commandCode);
    assert(packet.dataSize == size);
    assert(memcmp(packet.data, expected, size) == 0);
}

static void testEventCountersArePerDevice() {
    Bus bus;
    assert(bus.begin());
    finishPendingTransmission(bus);
    assert(bus.addPacketHandler(interleavedEventHandler) != INVALID_SUBSCRIPTION);
    inject(bus, announce(1, 1));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (5 << 8) | event::BUTTON_DOWN)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 2, static_cast<uint16_t>(0x8000 | (6 << 8) | event::BUTTON_DOWN)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (6 << 8) | event::BUTTON_UP)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 2, static_cast<uint16_t>(0x8000 | (8 << 8) | event::BUTTON_UP)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (7 << 8) | event::BUTTON_UP)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 2, static_cast<uint16_t>(0x8000 | (8 << 8) | event::BUTTON_UP)));
    assert(interleavedEventCalls == 4);
    assert(bus.diagnostics().duplicateEvents == 1);
    assert(bus.diagnostics().outOfOrderEvents == 1);
    bus.end();
}

static void testDiscoveryEventsAndBindings() {
    Bus bus;
    assert(bus.begin());
    Frame sent;
    assert(NrfTransport::instance().takeSentFrame(sent));
    size_t announceOffset = 0;
    PacketView announcePacket;
    assert(packetAt(sent, announceOffset, announcePacket));
    assert(announcePacket.serviceIndex == SERVICE_INDEX_CONTROL);
    assert(announcePacket.dataSize >= 4);
    assert(announcePacket.data[2] == 1);
    NrfTransport::instance().completeTransmit();
    bus.process();
    assert(bus.addPacketHandler(packetHandler) != INVALID_SUBSCRIPTION);
    const uint8_t primaryDeviceSubscription = bus.addDeviceHandler(deviceHandler);
    const uint8_t lifecycleSubscription = bus.addDeviceHandler(lifecycleHandler);
    const uint8_t packetSubscription = bus.addPacketHandler(filteredHandler, nullptr, DEVICE_ID, 1);
    const uint8_t deviceSubscription = bus.addDeviceHandler(subscribedDeviceHandler);
    assert(primaryDeviceSubscription != INVALID_SUBSCRIPTION);
    assert(lifecycleSubscription != INVALID_SUBSCRIPTION);
    assert(packetSubscription != INVALID_SUBSCRIPTION);
    assert(deviceSubscription != INVALID_SUBSCRIPTION);

    inject(bus, announce(2, 1));
    assert(deviceConnects == 1);
    assert(subscribedDeviceConnects == 1);
    const Service button = bus.findService(service::BUTTON);
    assert(button.valid());
    ServiceBinding binding(service::BUTTON);
    binding.bind(button);
    assert(bus.resolve(binding).valid());
    const Service control = bus.service(DEVICE_ID, SERVICE_INDEX_CONTROL);
    assert(control.valid());
    assert(control.serviceClass == service::CONTROL);

    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (5 << 8) | event::BUTTON_DOWN)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (5 << 8) | event::BUTTON_DOWN)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (7 << 8) | event::BUTTON_UP)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (6 << 8) | event::BUTTON_UP)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (7 << 8) | event::BUTTON_DOWN)));
    assert(filteredCalls == 3);
    assert(bus.diagnostics().duplicateEvents == 1);
    assert(bus.diagnostics().outOfOrderEvents == 1);

    inject(bus, announce(3, 8));
    assert(lifecycleRestarts == 0);
    assert(lifecycleMisses == 1);
    assert(bus.diagnostics().missedReports == 2);

    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (20 << 8) | event::BUTTON_DOWN)));
    inject(bus, announce(1, 3));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (25 << 8) | event::BUTTON_UP)));
    assert(filteredCalls == 5);
    assert(lifecycleMisses == 2);
    assert(bus.diagnostics().missedReports == 3);

    inject(bus, announce(15, 1));
    inject(bus, announce(0, 1));
    assert(lifecycleRestarts == 2);
    assert(bus.diagnostics().deviceRestarts == 2);

    jacdacTestSetMillis(500);
    bus.process();
    assert(NrfTransport::instance().takeSentFrame(sent));
    announceOffset = 0;
    assert(packetAt(sent, announceOffset, announcePacket));
    assert(announcePacket.data[2] == 1);
    assert(packetCalls == 10);
    assert(bus.removePacketHandler(packetSubscription));
    assert(!bus.removePacketHandler(packetSubscription));
    assert(bus.removeDeviceHandler(deviceSubscription));
    assert(!bus.removeDeviceHandler(deviceSubscription));
    bus.end();
}

static void testRegisterRequestsAndErrors() {
    Bus bus;
    jacdacTestSetMillis(0);
    assert(bus.begin());
    finishPendingTransmission(bus);
    inject(bus, announce(1, 1));
    const Service button = bus.findService(service::BUTTON);

    assert(bus.getRegisterAsync(button, reg::BUTTON_PRESSED, registerHandler));
    assert(!bus.getRegisterAsync(button, reg::BUTTON_PRESSED, registerHandler));
    finishPendingTransmission(bus);
    const uint8_t pressed = 1;
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, CMD_GET_REGISTER | reg::BUTTON_PRESSED, &pressed, sizeof(pressed)));
    assert(registerResponses == 1);

    assert(bus.getRegisterAsync(button, reg::BUTTON_ANALOG, registerHandler, nullptr, 10));
    jacdacTestSetMillis(10);
    bus.process();
    assert(registerTimeouts == 1);
    assert(bus.diagnostics().registerTimeouts == 1);

    bus.setCommandErrorHandler(commandErrorHandler);
    const uint8_t error[] = {0x88, 0x00, 0x34, 0x12};
    inject(bus, frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CONTROL, CMD_COMMAND_NOT_IMPLEMENTED, error, sizeof(error)));
    assert(commandErrors == 1);
    bus.end();
}

static void testAckRetriesAndSubscriptions() {
    Bus bus;
    jacdacTestSetMillis(0);
    assert(bus.begin());
    finishPendingTransmission(bus);
    const uint8_t subscription = bus.addAckHandler(ackSubscription);
    assert(subscription != INVALID_SUBSCRIPTION);

    const Service button = {DEVICE_ID, service::BUTTON, 1};
    assert(bus.sendCommand(button, 0x80, nullptr, 0, true));
    Frame initial;
    assert(NrfTransport::instance().takeSentFrame(initial));
    NrfTransport::instance().completeTransmit();
    bus.process();

    const uint32_t retryTimes[] = {40, 120, 240};
    for (uint8_t index = 0; index < 3; ++index) {
        jacdacTestSetMillis(retryTimes[index]);
        bus.process();
        Frame retry;
        assert(NrfTransport::instance().takeSentFrame(retry));
        assert(frameSize(retry) == frameSize(initial));
        assert(memcmp(&retry, &initial, frameSize(initial)) == 0);
        NrfTransport::instance().completeTransmit();
        bus.process();
    }
    jacdacTestSetMillis(400);
    bus.process();
    assert(ackFailures == 1);
    assert(bus.diagnostics().ackRetries == 3);
    assert(bus.diagnostics().ackTimeouts == 1);

    const uint8_t value = 1;
    assert(bus.sendCommand(button, 0x81, &value, sizeof(value), true));
    Frame acknowledged;
    assert(NrfTransport::instance().takeSentFrame(acknowledged));
    NrfTransport::instance().completeTransmit();
    bus.process();
    inject(bus, frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CRC_ACK, acknowledged.crc));
    assert(ackSuccesses == 1);
    assert(bus.diagnostics().acksReceived == 1);

    assert(bus.removeAckHandler(subscription));
    assert(!bus.removeAckHandler(subscription));
    assert(!bus.removeAckHandler(INVALID_SUBSCRIPTION));
    bus.end();
}

static void testMulticastAndBatching() {
    Bus bus;
    jacdacTestSetMillis(0);
    assert(bus.begin());
    finishPendingTransmission(bus);

    const uint8_t value = 1;
    assert(bus.sendMulticast(service::LED, CMD_SET_REGISTER | reg::INTENSITY, &value, sizeof(value)));
    Frame sent;
    assert(NrfTransport::instance().takeSentFrame(sent));
    assert((sent.flags & FRAME_FLAG_IDENTIFIER_IS_SERVICE_CLASS) != 0);
    assert(static_cast<uint32_t>(sent.deviceIdentifier) == service::LED);
    NrfTransport::instance().completeTransmit();
    bus.process();

    const Service first = {DEVICE_ID, service::BUTTON, 1};
    const Service second = {DEVICE_ID, service::POTENTIOMETER, 2};
    CommandBatch batch(DEVICE_ID);
    assert(batch.add(first, CMD_GET_REGISTER | reg::BUTTON_PRESSED));
    assert(batch.add(second, CMD_GET_REGISTER | reg::READING));
    assert(batch.packetCount() == 2);
    assert(bus.sendBatch(batch));
    assert(NrfTransport::instance().takeSentFrame(sent));
    size_t offset = 0;
    PacketView packet;
    assert(packetAt(sent, offset, packet));
    assert(packet.serviceIndex == 1);
    assert(packetAt(sent, offset, packet));
    assert(packet.serviceIndex == 2);
    assert(!packetAt(sent, offset, packet));
    bus.end();
}

static void testOperationErrors() {
    Bus bus;
    const Service button = {DEVICE_ID, service::BUTTON, 1};
    assert(!bus.sendCommand(button, 0x80));
    assert(bus.lastError() == Error::NotRunning);
    assert(bus.begin());
    assert(bus.lastError() == Error::None);
    finishPendingTransmission(bus);

    assert(!bus.sendCommand({0, service::BUTTON, 1}, 0x80));
    assert(bus.lastError() == Error::InvalidService);
    assert(!bus.sendCommand(button, 0x80, nullptr, 1));
    assert(bus.lastError() == Error::InvalidArgument);
    uint8_t largePayload[FRAME_DATA_SIZE] = {};
    assert(!bus.sendCommand(button, 0x80, largePayload, sizeof(largePayload)));
    assert(bus.lastError() == Error::PacketTooLarge);
    assert(bus.sendCommand({DEVICE_ID, service::BUTTON, SERVICE_INDEX_MAX_REGULAR}, 0x80));
    finishPendingTransmission(bus);
    assert(!bus.sendCommand({DEVICE_ID, service::BUTTON, static_cast<uint8_t>(SERVICE_INDEX_MAX_REGULAR + 1)}, 0x80));
    assert(bus.lastError() == Error::InvalidService);

    assert(bus.getRegisterAsync(button, reg::READING, registerHandler));
    assert(!bus.getRegisterAsync(button, reg::READING, registerHandler));
    assert(bus.lastError() == Error::DuplicateRequest);
    finishPendingTransmission(bus);

    uint8_t subscriptions[MAX_SUBSCRIBERS];
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        subscriptions[index] = bus.addPacketHandler(packetHandler);
        assert(subscriptions[index] != INVALID_SUBSCRIPTION);
    }
    assert(bus.addPacketHandler(packetHandler) == INVALID_SUBSCRIPTION);
    assert(bus.lastError() == Error::NoCapacity);
    assert(!bus.removePacketHandler(INVALID_SUBSCRIPTION));
    assert(bus.lastError() == Error::InvalidSubscription);
    assert(bus.removePacketHandler(subscriptions[0]));
    assert(bus.lastError() == Error::None);

    CommandBatch batch(DEVICE_ID);
    assert(!batch.add(button, 0x80, nullptr, 1));
    assert(batch.error() == Error::InvalidArgument);
    assert(!bus.sendBatch(batch));
    assert(bus.lastError() == Error::InvalidArgument);

    LedStripClient strip(bus);
    assert(!strip.setPixel(16384, 1, 2, 3));
    assert(bus.lastError() == Error::InvalidArgument);
    bus.end();
}

static void testExclusiveBusOwnership() {
    Bus first;
    Bus second;
    assert(first.begin());
    assert(!second.begin());
    assert(second.lastError() == Error::TransportUnavailable);
    second.end();
    assert(first.sendMulticast(service::BUTTON, CMD_GET_REGISTER | reg::BUTTON_PRESSED));
    first.end();
    assert(second.begin());
    second.end();
}

static void testTypedClientBindingIsStable() {
    jacdacTestSetMillis(0);
    Bus bus;
    assert(bus.begin());
    finishPendingTransmission(bus);

    Frame first = announce(1, 1);
    inject(bus, first);
    ButtonClient button(bus);
    assert(button.resolve().deviceIdentifier == DEVICE_ID);
    assert(!button.bind({DEVICE_ID, service::LED, 1}));
    assert(bus.lastError() == Error::InvalidService);

    Frame second = announce(1, 1);
    second.deviceIdentifier = DEVICE_ID + 1;
    finalizeFrame(second);
    inject(bus, second);
    jacdacTestSetMillis(1000);
    inject(bus, second);
    jacdacTestSetMillis(2100);
    bus.process();
    assert(!button.resolve().valid());
    button.clearBinding();
    assert(button.resolve().deviceIdentifier == DEVICE_ID + 1);
    bus.end();
}

static void testDiagnosticCategories() {
    Bus bus;
    assert(bus.begin());
    finishPendingTransmission(bus);

    Frame malformed = frameWithPacket(DEVICE_ID, 0, 1, 0x80);
    malformed.data[0] = 0xff;
    finalizeFrame(malformed);
    inject(bus, malformed);
    assert(bus.diagnostics().malformedPackets == 1);
    assert(bus.diagnostics().crcErrors == 0);

    const uint8_t shortPayload = 0;
    inject(bus, frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, &shortPayload, sizeof(shortPayload)));
    inject(bus, frameWithPacket(DEVICE_ID, 0, SERVICE_INDEX_CONTROL, CMD_COMMAND_NOT_IMPLEMENTED, &shortPayload, sizeof(shortPayload)));
    assert(bus.diagnostics().malformedPackets == 3);
    assert(bus.diagnostics().commandErrors == 0);

    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        Frame deviceAnnouncement = announce(1, 1);
        deviceAnnouncement.deviceIdentifier += index;
        finalizeFrame(deviceAnnouncement);
        inject(bus, deviceAnnouncement);
    }
    Frame excessDevice = announce(1, 1);
    excessDevice.deviceIdentifier += MAX_DEVICES;
    finalizeFrame(excessDevice);
    inject(bus, excessDevice);
    assert(bus.diagnostics().deviceOverflows == 1);
    assert(bus.diagnostics().receiveOverflows == 0);
    bus.end();
}

static void testPeripheralClientPayloads() {
    Bus bus;
    assert(bus.begin());
    finishPendingTransmission(bus);
    Frame frame;
    const uint32_t serviceClasses[] = {service::RELAY, service::LIGHT_BULB, service::MOTOR, service::DUAL_MOTORS, service::BUZZER, service::VIBRATION_MOTOR, service::HID_KEYBOARD, service::HID_MOUSE, service::HID_JOYSTICK, service::CHARACTER_SCREEN, service::CURSOR_CHARACTER_SCREEN, service::POWER};
    inject(bus, announceServices(serviceClasses, sizeof(serviceClasses) / sizeof(serviceClasses[0])));

    RelayClient relay(bus);
    assert(relay.bind({DEVICE_ID, service::RELAY, 1}));
    assert(relay.setActive(true));
    PacketView packet = takePacket(bus, frame);
    const uint8_t relayData[] = {1};
    assertPayload(packet, CMD_SET_REGISTER | reg::INTENSITY, relayData, sizeof(relayData));

        LightBulbClient bulb(bus);
        assert(bulb.bind({DEVICE_ID, service::LIGHT_BULB, 2}));
        assert(bulb.setBrightness(0x1234));
        packet = takePacket(bus, frame);
        const uint8_t bulbData[] = {0x34, 0x12};
        assertPayload(packet, CMD_SET_REGISTER | reg::INTENSITY, bulbData, sizeof(bulbData));

        MotorClient motor(bus);
        assert(motor.bind({DEVICE_ID, service::MOTOR, 3}));
        assert(motor.setSpeed(-2));
        packet = takePacket(bus, frame);
        const uint8_t motorData[] = {0xfe, 0xff};
        assertPayload(packet, CMD_SET_REGISTER | reg::VALUE, motorData, sizeof(motorData));

        DualMotorsClient motors(bus);
        assert(motors.bind({DEVICE_ID, service::DUAL_MOTORS, 4}));
        assert(motors.setSpeeds(0x1234, -2));
        packet = takePacket(bus, frame);
        const uint8_t motorsData[] = {0x34, 0x12, 0xfe, 0xff};
        assertPayload(packet, CMD_SET_REGISTER | reg::VALUE, motorsData, sizeof(motorsData));

        BuzzerClient buzzer(bus);
        assert(buzzer.bind({DEVICE_ID, service::BUZZER, 5}));
        assert(buzzer.playNote(440, 0x8000, 250));
        packet = takePacket(bus, frame);
        const uint8_t buzzerData[] = {0xb8, 0x01, 0x00, 0x80, 0xfa, 0x00};
        assertPayload(packet, command::BUZZER_PLAY_NOTE, buzzerData, sizeof(buzzerData));

        VibrationMotorClient vibration(bus);
        assert(vibration.bind({DEVICE_ID, service::VIBRATION_MOTOR, 6}));
        const VibrationStep steps[] = {{2, 255}, {3, 0}};
        assert(vibration.vibrate(steps, 2));
        packet = takePacket(bus, frame);
        const uint8_t vibrationData[] = {2, 255, 3, 0};
        assertPayload(packet, command::VIBRATION_MOTOR_VIBRATE, vibrationData, sizeof(vibrationData));

        HidKeyboardClient keyboard(bus);
        assert(keyboard.bind({DEVICE_ID, service::HID_KEYBOARD, 7}));
        assert(keyboard.key(0x1234, 0x05, 2));
        packet = takePacket(bus, frame);
        const uint8_t keyboardData[] = {0x34, 0x12, 0x05, 0x02};
        assertPayload(packet, command::HID_KEYBOARD_KEY, keyboardData, sizeof(keyboardData));

        HidMouseClient mouse(bus);
        assert(mouse.bind({DEVICE_ID, service::HID_MOUSE, 8}));
        assert(mouse.move(-2, 0x1234, 0x5678));
        packet = takePacket(bus, frame);
        const uint8_t mouseData[] = {0xfe, 0xff, 0x34, 0x12, 0x78, 0x56};
        assertPayload(packet, command::HID_MOUSE_MOVE, mouseData, sizeof(mouseData));

        HidJoystickClient joystick(bus);
        assert(joystick.bind({DEVICE_ID, service::HID_JOYSTICK, 9}));
        const int16_t axes[] = {-2, 0x1234};
        assert(joystick.setAxes(axes, 2));
        packet = takePacket(bus, frame);
        const uint8_t joystickData[] = {0xfe, 0xff, 0x34, 0x12};
        assertPayload(packet, command::HID_JOYSTICK_SET_AXIS, joystickData, sizeof(joystickData));

        CharacterScreenClient screen(bus);
        assert(screen.bind({DEVICE_ID, service::CHARACTER_SCREEN, 10}));
        assert(screen.setMessage("abc", 3));
        packet = takePacket(bus, frame);
        const uint8_t messageData[] = {'a', 'b', 'c'};
        assertPayload(packet, CMD_SET_REGISTER | reg::VALUE, messageData, sizeof(messageData));

        CursorCharacterScreenClient cursorScreen(bus);
        assert(cursorScreen.bind({DEVICE_ID, service::CURSOR_CHARACTER_SCREEN, 11}));
        assert(cursorScreen.setCursor(12, 3));
        packet = takePacket(bus, frame);
        const uint8_t cursorData[] = {12, 3};
        assertPayload(packet, command::CURSOR_SCREEN_SET_CURSOR, cursorData, sizeof(cursorData));

        PowerClient power(bus);
        assert(power.bind({DEVICE_ID, service::POWER, 12}));
        assert(power.setMaxPower(900));
        packet = takePacket(bus, frame);
        const uint8_t powerData[] = {0x84, 0x03};
        assertPayload(packet, CMD_SET_REGISTER | reg::MAX_POWER, powerData, sizeof(powerData));

    VibrationStep tooMany[1];
    assert(!vibration.vibrate(tooMany, static_cast<uint8_t>(SERIAL_PAYLOAD_SIZE / sizeof(VibrationStep) + 1)));
    assert(bus.lastError() == Error::PacketTooLarge);
    bus.end();
}

int main() {
    testEventCountersArePerDevice();
    testDiscoveryEventsAndBindings();
    testRegisterRequestsAndErrors();
    testAckRetriesAndSubscriptions();
    testMulticastAndBatching();
    testOperationErrors();
    testPeripheralClientPayloads();
    testExclusiveBusOwnership();
    testTypedClientBindingIsStable();
    testDiagnosticCategories();
    return 0;
}
