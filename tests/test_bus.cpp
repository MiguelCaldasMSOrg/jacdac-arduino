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

static void inject(Bus &bus, const Frame &frame) {
    Nrf52Transport::instance().injectFrame(frame);
    bus.process();
}

static void packetHandler(const PacketView &, void *) { ++packetCalls; }
static void filteredHandler(const PacketView &, void *) { ++filteredCalls; }
static void deviceHandler(const Device &, bool connected, void *) { if (connected) ++deviceConnects; }
static void subscribedDeviceHandler(const Device &, bool connected, void *) { if (connected) ++subscribedDeviceConnects; }

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

static void finishPendingTransmission(Bus &bus) {
    Frame frame;
    if (Nrf52Transport::instance().takeSentFrame(frame)) {
        Nrf52Transport::instance().completeTransmit();
        bus.process();
    }
}

static void testDiscoveryEventsAndBindings() {
    Bus bus;
    assert(bus.begin());
    Frame sent;
    assert(Nrf52Transport::instance().takeSentFrame(sent));
    size_t announceOffset = 0;
    PacketView announcePacket;
    assert(packetAt(sent, announceOffset, announcePacket));
    assert(announcePacket.serviceIndex == SERVICE_INDEX_CONTROL);
    assert(announcePacket.dataSize >= 4);
    assert(announcePacket.data[2] == 1);
    Nrf52Transport::instance().completeTransmit();
    bus.process();
    bus.onPacket(packetHandler);
    bus.onDevice(deviceHandler);
    bus.onDeviceEvent(lifecycleHandler);
    const uint8_t packetSubscription = bus.addPacketHandler(filteredHandler, nullptr, DEVICE_ID, 1);
    const uint8_t deviceSubscription = bus.addDeviceHandler(subscribedDeviceHandler);
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

    inject(bus, announce(1, 8));
    assert(lifecycleRestarts == 1);
    assert(lifecycleMisses == 1);
    assert(bus.diagnostics().missedReports == 2);

    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (20 << 8) | event::BUTTON_DOWN)));
    inject(bus, announce(1, 3));
    inject(bus, frameWithPacket(DEVICE_ID, 0, 1, static_cast<uint16_t>(0x8000 | (25 << 8) | event::BUTTON_UP)));
    assert(filteredCalls == 5);
    assert(lifecycleMisses == 2);
    assert(bus.diagnostics().missedReports == 3);

    jacdacTestSetMillis(500);
    bus.process();
    assert(Nrf52Transport::instance().takeSentFrame(sent));
    announceOffset = 0;
    assert(packetAt(sent, announceOffset, announcePacket));
    assert(announcePacket.data[2] == 1);
    assert(packetCalls == 8);
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

    bus.onCommandError(commandErrorHandler);
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
    assert(Nrf52Transport::instance().takeSentFrame(initial));
    Nrf52Transport::instance().completeTransmit();
    bus.process();

    const uint32_t retryTimes[] = {40, 120, 240};
    for (uint8_t index = 0; index < 3; ++index) {
        jacdacTestSetMillis(retryTimes[index]);
        bus.process();
        Frame retry;
        assert(Nrf52Transport::instance().takeSentFrame(retry));
        assert(frameSize(retry) == frameSize(initial));
        assert(memcmp(&retry, &initial, frameSize(initial)) == 0);
        Nrf52Transport::instance().completeTransmit();
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
    assert(Nrf52Transport::instance().takeSentFrame(acknowledged));
    Nrf52Transport::instance().completeTransmit();
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
    assert(Nrf52Transport::instance().takeSentFrame(sent));
    assert((sent.flags & FRAME_FLAG_IDENTIFIER_IS_SERVICE_CLASS) != 0);
    assert(static_cast<uint32_t>(sent.deviceIdentifier) == service::LED);
    Nrf52Transport::instance().completeTransmit();
    bus.process();

    const Service first = {DEVICE_ID, service::BUTTON, 1};
    const Service second = {DEVICE_ID, service::POTENTIOMETER, 2};
    CommandBatch batch(DEVICE_ID);
    assert(batch.add(first, CMD_GET_REGISTER | reg::BUTTON_PRESSED));
    assert(batch.add(second, CMD_GET_REGISTER | reg::READING));
    assert(batch.packetCount() == 2);
    assert(bus.sendBatch(batch));
    assert(Nrf52Transport::instance().takeSentFrame(sent));
    size_t offset = 0;
    PacketView packet;
    assert(packetAt(sent, offset, packet));
    assert(packet.serviceIndex == 1);
    assert(packetAt(sent, offset, packet));
    assert(packet.serviceIndex == 2);
    assert(!packetAt(sent, offset, packet));
    bus.end();
}

int main() {
    testDiscoveryEventsAndBindings();
    testRegisterRequestsAndErrors();
    testAckRetriesAndSubscriptions();
    testMulticastAndBatching();
    return 0;
}
