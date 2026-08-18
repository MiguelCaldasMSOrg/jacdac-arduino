#include "Jacdac.h"
#include "JacdacTransport.h"

#include <string.h>

namespace jacdac {

Bus Jacdac;

static uint16_t readU16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

static uint32_t readU32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

Bus::Bus() : rxHead_(0), rxTail_(0), txHead_(0), txTail_(0), transmitting_(false), running_(false), packetHandler_(nullptr), deviceHandler_(nullptr), packetContext_(nullptr), deviceContext_(nullptr), ackHandler_(nullptr), ackContext_(nullptr), selfIdentifier_(0), nextAnnounce_(0), packetCount_(0) {
    memset(devices_, 0, sizeof(devices_));
    memset(ackRequests_, 0, sizeof(ackRequests_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
}

bool Bus::begin(uint8_t pin) {
    if (running_) {
        return true;
    }
    memset(devices_, 0, sizeof(devices_));
    rxHead_ = rxTail_ = txHead_ = txTail_ = 0;
    transmitting_ = false;
    memset(ackRequests_, 0, sizeof(ackRequests_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
    running_ = Nrf52Transport::instance().begin(pin, receiveFromTransport, transmitDoneFromTransport, this);
    if (running_) {
        selfIdentifier_ = Nrf52Transport::instance().deviceIdentifier();
        queueAnnounce();
        nextAnnounce_ = millis() + 500;
    }
    return running_;
}

void Bus::end() {
    Nrf52Transport::instance().end();
    running_ = false;
}

void Bus::process() {
    if (!running_) {
        return;
    }
    while (rxTail_ != rxHead_) {
        noInterrupts();
        Frame frame = rxQueue_[rxTail_];
        rxTail_ = static_cast<uint8_t>((rxTail_ + 1) % (RX_QUEUE_SIZE + 1));
        interrupts();
        handleFrame(frame);
    }
    const uint32_t now = millis();
    processAcks(now);
    if (static_cast<int32_t>(now - nextAnnounce_) >= 0) {
        queueAnnounce();
        nextAnnounce_ = now + 500;
    }
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        Device &knownDevice = devices_[index];
        if (knownDevice.connected() && static_cast<uint32_t>(now - knownDevice.lastSeen) > DEVICE_TIMEOUT_MS) {
            if (deviceHandler_ != nullptr) {
                deviceHandler_(knownDevice, false, deviceContext_);
            }
            memset(&knownDevice, 0, sizeof(knownDevice));
        }
    }
    startNextTransmission();
    diagnostics_.busErrors = Nrf52Transport::instance().busErrors();
    diagnostics_.collisions = Nrf52Transport::instance().collisions();
}

bool Bus::running() const {
    return running_;
}

uint8_t Bus::deviceCount() const {
    uint8_t count = 0;
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        if (devices_[index].connected()) {
            ++count;
        }
    }
    return count;
}

const Device *Bus::device(uint8_t requestedIndex) const {
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        if (devices_[index].connected() && requestedIndex-- == 0) {
            return &devices_[index];
        }
    }
    return nullptr;
}

Service Bus::findService(uint32_t serviceClass, uint8_t instance) const {
    for (uint8_t deviceIndex = 0; deviceIndex < MAX_DEVICES; ++deviceIndex) {
        const Device &knownDevice = devices_[deviceIndex];
        if (!knownDevice.connected()) {
            continue;
        }
        for (uint8_t serviceIndex = 1; serviceIndex <= knownDevice.serviceCount; ++serviceIndex) {
            if (knownDevice.serviceClasses[serviceIndex - 1] == serviceClass && instance-- == 0) {
                return {knownDevice.identifier, serviceClass, serviceIndex};
            }
        }
    }
    return {0, serviceClass, 0};
}

Service Bus::service(uint64_t deviceIdentifier, uint8_t serviceIndex) const {
    for (uint8_t deviceIndex = 0; deviceIndex < MAX_DEVICES; ++deviceIndex) {
        const Device &knownDevice = devices_[deviceIndex];
        if (knownDevice.identifier == deviceIdentifier && serviceIndex > 0 && serviceIndex <= knownDevice.serviceCount) {
            return {deviceIdentifier, knownDevice.serviceClasses[serviceIndex - 1], serviceIndex};
        }
    }
    return {0, 0, 0};
}

bool Bus::sendCommand(const Service &target, uint16_t command, const void *data, uint8_t size, bool requestAck) {
    if (!running_ || !target.valid()) {
        return false;
    }
    Frame frame;
    uint8_t flags = FRAME_FLAG_COMMAND;
    if (requestAck) {
        flags |= FRAME_FLAG_ACK_REQUESTED;
    }
    resetFrame(frame, target.deviceIdentifier, flags);
    if (!appendPacket(frame, target.serviceIndex, command, data, size)) {
        return false;
    }
    finalizeFrame(frame);
    int8_t ackIndex = -1;
    if (requestAck) {
        ackIndex = trackAck(frame);
        if (ackIndex < 0) {
            return false;
        }
    }
    if (!queueFrame(frame)) {
        if (ackIndex >= 0) {
            ackRequests_[ackIndex].active = false;
        }
        return false;
    }
    return true;
}

bool Bus::getRegister(const Service &target, uint16_t reg) {
    return sendCommand(target, static_cast<uint16_t>(CMD_GET_REGISTER | (reg & REGISTER_CODE_MASK)));
}

bool Bus::setRegister(const Service &target, uint16_t reg, const void *data, uint8_t size, bool requestAck) {
    return sendCommand(target, static_cast<uint16_t>(CMD_SET_REGISTER | (reg & REGISTER_CODE_MASK)), data, size, requestAck);
}

void Bus::onPacket(PacketHandler handler, void *context) {
    packetHandler_ = handler;
    packetContext_ = context;
}

void Bus::onDevice(DeviceHandler handler, void *context) {
    deviceHandler_ = handler;
    deviceContext_ = context;
}

void Bus::onAck(AckHandler handler, void *context) {
    ackHandler_ = handler;
    ackContext_ = context;
}

const Diagnostics &Bus::diagnostics() const {
    return diagnostics_;
}

void Bus::receiveFromTransport(const Frame &frame, void *context) {
    Bus *bus = static_cast<Bus *>(context);
    const uint8_t next = static_cast<uint8_t>((bus->rxHead_ + 1) % (RX_QUEUE_SIZE + 1));
    if (next == bus->rxTail_) {
        ++bus->diagnostics_.receiveOverflows;
        return;
    }
    bus->rxQueue_[bus->rxHead_] = frame;
    bus->rxHead_ = next;
}

void Bus::transmitDoneFromTransport(void *context) {
    Bus *bus = static_cast<Bus *>(context);
    bus->txTail_ = static_cast<uint8_t>((bus->txTail_ + 1) % (TX_QUEUE_SIZE + 1));
    bus->transmitting_ = false;
    ++bus->diagnostics_.framesSent;
}

void Bus::handleFrame(const Frame &frame) {
    if (!validateFrame(frame, frameSize(frame))) {
        ++diagnostics_.crcErrors;
        return;
    }
    ++diagnostics_.framesReceived;
    size_t offset = 0;
    PacketView packet;
    while (packetAt(frame, offset, packet)) {
        handlePacket(packet);
    }
}

void Bus::handlePacket(const PacketView &packet) {
    Device *knownDevice = findDevice(packet.deviceIdentifier);
    if (knownDevice != nullptr) {
        knownDevice->lastSeen = millis();
    }
    if (packet.isReport() && packet.serviceIndex == SERVICE_INDEX_CONTROL && packet.serviceCommand == CMD_ANNOUNCE) {
        handleAnnounce(packet);
    }
    handleAck(packet);
    if (packetHandler_ != nullptr) {
        packetHandler_(packet, packetContext_);
    }
}

int8_t Bus::trackAck(const Frame &frame) {
    for (uint8_t index = 0; index < MAX_ACK_REQUESTS; ++index) {
        const AckRequest &request = ackRequests_[index];
        if (request.active && request.frame.deviceIdentifier == frame.deviceIdentifier && request.crc == frame.crc) {
            return -1;
        }
    }
    for (uint8_t index = 0; index < MAX_ACK_REQUESTS; ++index) {
        AckRequest &request = ackRequests_[index];
        if (!request.active) {
            request.frame = frame;
            request.nextRetry = millis() + ACK_DELAY_MS;
            request.crc = frame.crc;
            request.attempts = 1;
            request.active = true;
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

void Bus::handleAck(const PacketView &packet) {
    if (!packet.isReport() || packet.serviceIndex != SERVICE_INDEX_CRC_ACK) {
        return;
    }
    for (uint8_t index = 0; index < MAX_ACK_REQUESTS; ++index) {
        AckRequest &request = ackRequests_[index];
        if (request.active && request.frame.deviceIdentifier == packet.deviceIdentifier && request.crc == packet.serviceCommand) {
            request.active = false;
            ++diagnostics_.acksReceived;
            if (ackHandler_ != nullptr) {
                ackHandler_(request.frame.deviceIdentifier, request.crc, true, ackContext_);
            }
            return;
        }
    }
}

void Bus::processAcks(uint32_t now) {
    for (uint8_t index = 0; index < MAX_ACK_REQUESTS; ++index) {
        AckRequest &request = ackRequests_[index];
        if (!request.active || static_cast<int32_t>(now - request.nextRetry) < 0) {
            continue;
        }
        if (request.attempts >= ACK_ATTEMPTS) {
            request.active = false;
            ++diagnostics_.ackTimeouts;
            if (ackHandler_ != nullptr) {
                ackHandler_(request.frame.deviceIdentifier, request.crc, false, ackContext_);
            }
        } else if (queueFrame(request.frame)) {
            ++request.attempts;
            ++diagnostics_.ackRetries;
            request.nextRetry = now + request.attempts * ACK_DELAY_MS;
        } else {
            request.nextRetry = now + ACK_DELAY_MS;
        }
    }
}

void Bus::handleAnnounce(const PacketView &packet) {
    if (packet.dataSize < 4) {
        return;
    }
    Device *knownDevice = findDevice(packet.deviceIdentifier);
    bool created = false;
    if (knownDevice == nullptr) {
        for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
            if (!devices_[index].connected()) {
                knownDevice = &devices_[index];
                memset(knownDevice, 0, sizeof(*knownDevice));
                knownDevice->identifier = packet.deviceIdentifier;
                created = true;
                break;
            }
        }
    }
    if (knownDevice == nullptr) {
        ++diagnostics_.receiveOverflows;
        return;
    }
    knownDevice->lastSeen = millis();
    knownDevice->announceFlags = readU16(packet.data);
    knownDevice->packetCount = packet.data[2];
    uint8_t serviceCount = static_cast<uint8_t>((packet.dataSize - 4) / 4);
    if (serviceCount > MAX_SERVICES_PER_DEVICE) {
        serviceCount = MAX_SERVICES_PER_DEVICE;
    }
    knownDevice->serviceCount = serviceCount;
    for (uint8_t index = 0; index < serviceCount; ++index) {
        knownDevice->serviceClasses[index] = readU32(packet.data + 4 + index * 4);
    }
    if (created && deviceHandler_ != nullptr) {
        deviceHandler_(*knownDevice, true, deviceContext_);
    }
}

void Bus::queueAnnounce() {
    uint8_t announce[4] = {0x00, 0x08, packetCount_++, 0x00};
    Frame frame;
    resetFrame(frame, selfIdentifier_, 0);
    if (appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, announce, sizeof(announce))) {
        finalizeFrame(frame);
        queueFrame(frame);
    }
}

Device *Bus::findDevice(uint64_t identifier) {
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        if (devices_[index].identifier == identifier) {
            return &devices_[index];
        }
    }
    return nullptr;
}

bool Bus::queueFrame(const Frame &frame) {
    noInterrupts();
    const uint8_t next = static_cast<uint8_t>((txHead_ + 1) % (TX_QUEUE_SIZE + 1));
    if (next == txTail_) {
        interrupts();
        ++diagnostics_.transmitOverflows;
        return false;
    }
    txQueue_[txHead_] = frame;
    txHead_ = next;
    interrupts();
    startNextTransmission();
    return true;
}

void Bus::startNextTransmission() {
    if (!transmitting_ && txTail_ != txHead_) {
        transmitting_ = Nrf52Transport::instance().send(txQueue_[txTail_]);
    }
}

SensorClient::SensorClient(Bus &bus, uint32_t serviceClass, uint8_t instance) : bus_(bus), serviceClass_(serviceClass), instance_(instance) {}

bool SensorClient::connected() const {
    return resolve().valid();
}

Service SensorClient::resolve() const {
    return bus_.findService(serviceClass_, instance_);
}

bool SensorClient::requestReading() const {
    return bus_.getRegister(resolve(), reg::READING);
}

bool SensorClient::setStreaming(uint8_t samples) const {
    return bus_.setRegister(resolve(), reg::IS_STREAMING, samples);
}

bool SensorClient::setStreamingInterval(uint32_t milliseconds) const {
    return bus_.setRegister(resolve(), reg::STREAMING_INTERVAL, milliseconds);
}

ButtonClient::ButtonClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::BUTTON, instance) {}

bool ButtonClient::requestPressure() const {
    return requestReading();
}

bool ButtonClient::requestPressed() const {
    return bus_.getRegister(resolve(), reg::BUTTON_PRESSED);
}

bool ButtonClient::requestAnalog() const {
    return bus_.getRegister(resolve(), reg::BUTTON_ANALOG);
}

RotaryEncoderClient::RotaryEncoderClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::ROTARY_ENCODER, instance) {}

bool RotaryEncoderClient::requestPosition() const {
    return requestReading();
}

bool RotaryEncoderClient::requestClicksPerTurn() const {
    return bus_.getRegister(resolve(), reg::ROTARY_CLICKS_PER_TURN);
}

bool RotaryEncoderClient::requestClicker() const {
    return bus_.getRegister(resolve(), reg::ROTARY_CLICKER);
}

Service RotaryEncoderClient::buttonService() const {
    const Service rotary = resolve();
    if (!rotary.valid() || rotary.serviceIndex >= SERVICE_INDEX_MASK) {
        return {0, service::BUTTON, 0};
    }
    const Service button = bus_.service(rotary.deviceIdentifier, static_cast<uint8_t>(rotary.serviceIndex + 1));
    return button.serviceClass == service::BUTTON ? button : Service{0, service::BUTTON, 0};
}

PotentiometerClient::PotentiometerClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::POTENTIOMETER, instance) {}

bool PotentiometerClient::requestPosition() const {
    return requestReading();
}

bool PotentiometerClient::requestVariant() const {
    return bus_.getRegister(resolve(), reg::VARIANT);
}

LedStripClient::LedStripClient(Bus &bus, uint8_t instance) : bus_(bus), instance_(instance) {}

bool LedStripClient::connected() const {
    return resolve().valid();
}

Service LedStripClient::resolve() const {
    return bus_.findService(service::LED_STRIP, instance_);
}

bool LedStripClient::setBrightness(uint8_t brightness, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::INTENSITY, brightness, requestAck);
}

bool LedStripClient::setNumPixels(uint16_t numPixels, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::LED_STRIP_NUM_PIXELS, numPixels, requestAck);
}

bool LedStripClient::setMaxPower(uint16_t milliamps, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::MAX_POWER, milliamps, requestAck);
}

bool LedStripClient::setNumRepeats(uint16_t repeats, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::LED_STRIP_NUM_REPEATS, repeats, requestAck);
}

bool LedStripClient::requestActualBrightness() const {
    return bus_.getRegister(resolve(), reg::LED_STRIP_ACTUAL_BRIGHTNESS);
}

bool LedStripClient::requestNumPixels() const {
    return bus_.getRegister(resolve(), reg::LED_STRIP_NUM_PIXELS);
}

bool LedStripClient::requestMaxPixels() const {
    return bus_.getRegister(resolve(), reg::LED_STRIP_MAX_PIXELS);
}

bool LedStripClient::requestVariant() const {
    return bus_.getRegister(resolve(), reg::VARIANT);
}

bool LedStripClient::runProgram(const uint8_t *program, uint8_t size, bool requestAck) const {
    return bus_.sendCommand(resolve(), command::LED_STRIP_RUN, program, size, requestAck);
}

bool LedStripClient::setAll(uint8_t red, uint8_t green, uint8_t blue, bool requestAck) const {
    const uint8_t program[] = {0xd0, 0xc1, red, green, blue, 0xd5, 0x00};
    return runProgram(program, sizeof(program), requestAck);
}

bool LedStripClient::setPixel(uint16_t pixel, uint8_t red, uint8_t green, uint8_t blue, bool requestAck) const {
    if (pixel > 16383) {
        return false;
    }
    uint8_t program[8] = {0xcf};
    uint8_t offset = 1;
    if (pixel < 128) {
        program[offset++] = static_cast<uint8_t>(pixel);
    } else {
        program[offset++] = static_cast<uint8_t>(0x80 | (pixel >> 8));
        program[offset++] = static_cast<uint8_t>(pixel);
    }
    program[offset++] = red;
    program[offset++] = green;
    program[offset++] = blue;
    program[offset++] = 0xd5;
    program[offset++] = 0x00;
    return runProgram(program, offset, requestAck);
}

LedClient::LedClient(Bus &bus, uint8_t instance) : bus_(bus), instance_(instance) {}

bool LedClient::connected() const {
    return bus_.findService(service::LED, instance_).valid();
}

bool LedClient::setBrightness(uint8_t brightness) const {
    return bus_.setRegister(bus_.findService(service::LED, instance_), reg::INTENSITY, brightness);
}

bool LedClient::setPixels(const uint8_t *rgb, uint8_t byteCount) const {
    return bus_.setRegister(bus_.findService(service::LED, instance_), reg::VALUE, static_cast<const void *>(rgb), byteCount);
}

ServoClient::ServoClient(Bus &bus, uint8_t instance) : bus_(bus), instance_(instance) {}

bool ServoClient::connected() const {
    return bus_.findService(service::SERVO, instance_).valid();
}

bool ServoClient::setAngle(int32_t angleDegreesQ16) const {
    return bus_.setRegister(bus_.findService(service::SERVO, instance_), reg::VALUE, angleDegreesQ16);
}

bool ServoClient::setEnabled(bool enabled) const {
    const uint8_t intensity = enabled ? 1 : 0;
    return bus_.setRegister(bus_.findService(service::SERVO, instance_), reg::INTENSITY, intensity);
}

} // namespace jacdac