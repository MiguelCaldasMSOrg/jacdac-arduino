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

static bool payloadFits(uint8_t size) {
    return ((static_cast<size_t>(size) + 7) & ~static_cast<size_t>(3)) <= FRAME_DATA_SIZE;
}

static bool validService(const Service &service) {
    return service.valid() && service.serviceIndex <= SERVICE_INDEX_MAX_REGULAR;
}

Bus::Bus() : rxHead_(0), rxTail_(0), txHead_(0), txTail_(0), transmitting_(false), running_(false), commandErrorHandler_(nullptr), commandErrorContext_(nullptr), selfIdentifier_(0), nextAnnounce_(0), lastError_(Error::None) {
    memset(devices_, 0, sizeof(devices_));
    memset(ackRequests_, 0, sizeof(ackRequests_));
    memset(registerRequests_, 0, sizeof(registerRequests_));
    memset(packetSubscriptions_, 0, sizeof(packetSubscriptions_));
    memset(deviceSubscriptions_, 0, sizeof(deviceSubscriptions_));
    memset(ackSubscriptions_, 0, sizeof(ackSubscriptions_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
}

Bus::~Bus() {
    if (running_) {
        end();
    }
}

bool Bus::begin(uint8_t pin) {
    if (running_) {
        lastError_ = Error::None;
        return true;
    }
    memset(devices_, 0, sizeof(devices_));
    rxHead_ = rxTail_ = txHead_ = txTail_ = 0;
    transmitting_ = false;
    memset(ackRequests_, 0, sizeof(ackRequests_));
    memset(registerRequests_, 0, sizeof(registerRequests_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
    running_ = NrfTransport::instance().begin(pin, receiveFromTransport, transmitDoneFromTransport, this);
    if (running_) {
        lastError_ = Error::None;
        selfIdentifier_ = NrfTransport::instance().deviceIdentifier();
        queueAnnounce();
        nextAnnounce_ = millis() + 500;
    } else {
        lastError_ = Error::TransportUnavailable;
    }
    return running_;
}

void Bus::end() {
    if (!running_) {
        return;
    }
    NrfTransport::instance().end();
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
    processRegisterRequests(now);
    if (static_cast<int32_t>(now - nextAnnounce_) >= 0) {
        queueAnnounce();
        nextAnnounce_ = now + 500;
    }
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        Device &knownDevice = devices_[index];
        if (knownDevice.connected() && static_cast<uint32_t>(now - knownDevice.lastSeen) > DEVICE_TIMEOUT_MS) {
            dispatchDevice(knownDevice, DeviceEvent::Disconnected);
            memset(&knownDevice, 0, sizeof(knownDevice));
        }
    }
    startNextTransmission();
    diagnostics_.busErrors = NrfTransport::instance().busErrors();
    diagnostics_.collisions = NrfTransport::instance().collisions();
    const TransportDiagnostics &transportDiagnostics = NrfTransport::instance().diagnostics();
    diagnostics_.fallingEdges = transportDiagnostics.fallingEdges;
    diagnostics_.receiveStarts = transportDiagnostics.receiveStarts;
    diagnostics_.receiveCompletions = transportDiagnostics.receiveCompletions;
    diagnostics_.receiveBytes = transportDiagnostics.receiveBytes;
    diagnostics_.receiveTimeouts = transportDiagnostics.receiveTimeouts;
    diagnostics_.receiveShortFrames = transportDiagnostics.receiveShortFrames;
    diagnostics_.receiveInvalidFrames = transportDiagnostics.receiveInvalidFrames;
    diagnostics_.receiveHardwareErrors = transportDiagnostics.receiveHardwareErrors;
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
                return {knownDevice.deviceIdentifier, serviceClass, serviceIndex};
            }
        }
    }
    return {0, serviceClass, 0};
}

Service Bus::service(uint64_t deviceIdentifier, uint8_t serviceIndex) const {
    for (uint8_t deviceIndex = 0; deviceIndex < MAX_DEVICES; ++deviceIndex) {
        const Device &knownDevice = devices_[deviceIndex];
        if (knownDevice.deviceIdentifier == deviceIdentifier) {
            if (serviceIndex == SERVICE_INDEX_CONTROL) {
                return {deviceIdentifier, service::CONTROL, SERVICE_INDEX_CONTROL};
            }
            if (serviceIndex <= knownDevice.serviceCount) {
                return {deviceIdentifier, knownDevice.serviceClasses[serviceIndex - 1], serviceIndex};
            }
        }
    }
    return {0, 0, 0};
}

Service Bus::resolve(const ServiceBinding &binding) const {
    if (binding.bound()) {
        const Service result = service(binding.deviceIdentifier, binding.serviceIndex);
        return result.serviceClass == binding.serviceClass ? result : Service{0, binding.serviceClass, 0};
    }
    return findService(binding.serviceClass, binding.instance);
}

CommandBatch::CommandBatch(uint64_t deviceIdentifier, bool requestAck) : packetCount_(0), error_(Error::None) {
    resetFrame(frame_, deviceIdentifier, static_cast<uint8_t>(FRAME_FLAG_COMMAND | (requestAck ? FRAME_FLAG_ACK_REQUESTED : 0)));
}

bool CommandBatch::add(const Service &target, uint16_t command, const void *data, uint8_t size) {
    if (!validService(target) || target.deviceIdentifier != frame_.deviceIdentifier) {
        error_ = Error::InvalidService;
        return false;
    }
    if (size != 0 && data == nullptr) {
        error_ = Error::InvalidArgument;
        return false;
    }
    if (!payloadFits(size) || !appendPacket(frame_, target.serviceIndex, command, data, size)) {
        error_ = Error::PacketTooLarge;
        return false;
    }
    ++packetCount_;
    error_ = Error::None;
    return true;
}

bool Bus::sendCommand(const Service &target, uint16_t command, const void *data, uint8_t size, bool requestAck) {
    if (!running_) {
        lastError_ = Error::NotRunning;
        return false;
    }
    if (!validService(target)) {
        lastError_ = Error::InvalidService;
        return false;
    }
    if (size != 0 && data == nullptr) {
        lastError_ = Error::InvalidArgument;
        return false;
    }
    if (!payloadFits(size)) {
        lastError_ = Error::PacketTooLarge;
        return false;
    }
    Frame frame;
    uint8_t flags = FRAME_FLAG_COMMAND;
    if (requestAck) {
        flags |= FRAME_FLAG_ACK_REQUESTED;
    }
    resetFrame(frame, target.deviceIdentifier, flags);
    if (!appendPacket(frame, target.serviceIndex, command, data, size)) {
        lastError_ = Error::PacketTooLarge;
        return false;
    }
    finalizeFrame(frame);
    int8_t ackIndex = -1;
    if (requestAck) {
        ackIndex = trackAck(frame);
        if (ackIndex < 0) {
            lastError_ = ackIndex == -1 ? Error::DuplicateRequest : Error::NoCapacity;
            return false;
        }
    }
    if (!queueFrame(frame)) {
        if (ackIndex >= 0) {
            ackRequests_[ackIndex].active = false;
        }
        lastError_ = Error::QueueFull;
        return false;
    }
    lastError_ = Error::None;
    return true;
}

bool Bus::getRegister(const Service &target, uint16_t reg) {
    return sendCommand(target, static_cast<uint16_t>(CMD_GET_REGISTER | (reg & REGISTER_CODE_MASK)));
}

bool Bus::setRegister(const Service &target, uint16_t reg, const void *data, uint8_t size, bool requestAck) {
    return sendCommand(target, static_cast<uint16_t>(CMD_SET_REGISTER | (reg & REGISTER_CODE_MASK)), data, size, requestAck);
}

bool Bus::getRegisterAsync(const Service &target, uint16_t reg, RegisterResponseHandler handler, void *context, uint32_t timeoutMs) {
    if (!running_) {
        lastError_ = Error::NotRunning;
        return false;
    }
    if (!validService(target)) {
        lastError_ = Error::InvalidService;
        return false;
    }
    if (handler == nullptr) {
        lastError_ = Error::InvalidArgument;
        return false;
    }
    const uint16_t serviceCommand = static_cast<uint16_t>(CMD_GET_REGISTER | (reg & REGISTER_CODE_MASK));
    for (uint8_t index = 0; index < MAX_REGISTER_REQUESTS; ++index) {
        const RegisterRequest &request = registerRequests_[index];
        if (request.active && request.deviceIdentifier == target.deviceIdentifier && request.serviceIndex == target.serviceIndex && request.serviceCommand == serviceCommand) {
            lastError_ = Error::DuplicateRequest;
            return false;
        }
    }
    for (uint8_t index = 0; index < MAX_REGISTER_REQUESTS; ++index) {
        RegisterRequest &request = registerRequests_[index];
        if (!request.active) {
            request = {target.deviceIdentifier, millis() + timeoutMs, handler, context, serviceCommand, target.serviceIndex, true};
            if (!sendCommand(target, serviceCommand)) {
                request.active = false;
                return false;
            }
            return true;
        }
    }
    lastError_ = Error::NoCapacity;
    return false;
}

bool Bus::sendMulticast(uint32_t serviceClass, uint16_t serviceCommand, const void *data, uint8_t size) {
    if (!running_) {
        lastError_ = Error::NotRunning;
        return false;
    }
    if (serviceClass == 0 || (size != 0 && data == nullptr)) {
        lastError_ = Error::InvalidArgument;
        return false;
    }
    if (!payloadFits(size)) {
        lastError_ = Error::PacketTooLarge;
        return false;
    }
    Frame frame;
    resetFrame(frame, serviceClass, FRAME_FLAG_COMMAND | FRAME_FLAG_IDENTIFIER_IS_SERVICE_CLASS);
    if (!appendPacket(frame, SERVICE_INDEX_BROADCAST, serviceCommand, data, size)) {
        lastError_ = Error::PacketTooLarge;
        return false;
    }
    finalizeFrame(frame);
    if (!queueFrame(frame)) {
        lastError_ = Error::QueueFull;
        return false;
    }
    lastError_ = Error::None;
    return true;
}

bool Bus::sendBatch(const CommandBatch &batch) {
    if (!running_) {
        lastError_ = Error::NotRunning;
        return false;
    }
    if (batch.packetCount_ == 0) {
        lastError_ = batch.error_ == Error::None ? Error::InvalidArgument : batch.error_;
        return false;
    }
    Frame frame = batch.frame_;
    finalizeFrame(frame);
    int8_t ackIndex = -1;
    if ((frame.flags & FRAME_FLAG_ACK_REQUESTED) != 0) {
        ackIndex = trackAck(frame);
        if (ackIndex < 0) {
            lastError_ = ackIndex == -1 ? Error::DuplicateRequest : Error::NoCapacity;
            return false;
        }
    }
    if (!queueFrame(frame)) {
        if (ackIndex >= 0) {
            ackRequests_[ackIndex].active = false;
        }
        lastError_ = Error::QueueFull;
        return false;
    }
    lastError_ = Error::None;
    return true;
}

static Service controlService(uint64_t deviceIdentifier) {
    return {deviceIdentifier, service::CONTROL, SERVICE_INDEX_CONTROL};
}

bool Bus::identify(uint64_t deviceIdentifier, bool requestAck) { return sendCommand(controlService(deviceIdentifier), command::CONTROL_IDENTIFY, nullptr, 0, requestAck); }
bool Bus::resetDevice(uint64_t deviceIdentifier, bool requestAck) { return sendCommand(controlService(deviceIdentifier), command::CONTROL_RESET, nullptr, 0, requestAck); }
bool Bus::standby(uint64_t deviceIdentifier, uint32_t durationMs, bool requestAck) { return sendCommand(controlService(deviceIdentifier), command::CONTROL_STANDBY, &durationMs, sizeof(durationMs), requestAck); }
bool Bus::requestDeviceDescription(uint64_t deviceIdentifier) { return getRegister(controlService(deviceIdentifier), reg::DEVICE_DESCRIPTION); }
bool Bus::requestProductIdentifier(uint64_t deviceIdentifier) { return getRegister(controlService(deviceIdentifier), reg::PRODUCT_IDENTIFIER); }
bool Bus::requestFirmwareVersion(uint64_t deviceIdentifier) { return getRegister(controlService(deviceIdentifier), reg::FIRMWARE_VERSION); }
bool Bus::requestUptime(uint64_t deviceIdentifier) { return getRegister(controlService(deviceIdentifier), reg::UPTIME); }

bool Bus::setStatusLight(uint64_t deviceIdentifier, uint8_t red, uint8_t green, uint8_t blue, uint8_t speed) {
    const uint8_t data[] = {red, green, blue, speed};
    return sendCommand(controlService(deviceIdentifier), command::CONTROL_SET_STATUS_LIGHT, data, sizeof(data));
}

void Bus::setCommandErrorHandler(CommandErrorHandler handler, void *context) {
    commandErrorHandler_ = handler;
    commandErrorContext_ = context;
}

uint8_t Bus::addPacketHandler(PacketHandler handler, void *context, uint64_t deviceIdentifier, uint8_t serviceIndex, uint16_t serviceCommand) {
    if (handler == nullptr) {
        lastError_ = Error::InvalidArgument;
        return INVALID_SUBSCRIPTION;
    }
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        if (packetSubscriptions_[index].handler == nullptr) {
            packetSubscriptions_[index] = {handler, context, deviceIdentifier, serviceCommand, serviceIndex};
            lastError_ = Error::None;
            return index;
        }
    }
    lastError_ = Error::NoCapacity;
    return INVALID_SUBSCRIPTION;
}

uint8_t Bus::addDeviceHandler(DeviceHandler handler, void *context) {
    if (handler == nullptr) {
        lastError_ = Error::InvalidArgument;
        return INVALID_SUBSCRIPTION;
    }
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        if (deviceSubscriptions_[index].handler == nullptr) {
            deviceSubscriptions_[index] = {handler, context};
            lastError_ = Error::None;
            return index;
        }
    }
    lastError_ = Error::NoCapacity;
    return INVALID_SUBSCRIPTION;
}

uint8_t Bus::addAckHandler(AckHandler handler, void *context) {
    if (handler == nullptr) {
        lastError_ = Error::InvalidArgument;
        return INVALID_SUBSCRIPTION;
    }
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        if (ackSubscriptions_[index].handler == nullptr) {
            ackSubscriptions_[index] = {handler, context};
            lastError_ = Error::None;
            return index;
        }
    }
    lastError_ = Error::NoCapacity;
    return INVALID_SUBSCRIPTION;
}

bool Bus::removePacketHandler(uint8_t subscription) {
    if (subscription >= MAX_SUBSCRIBERS || packetSubscriptions_[subscription].handler == nullptr) {
        lastError_ = Error::InvalidSubscription;
        return false;
    }
    memset(&packetSubscriptions_[subscription], 0, sizeof(packetSubscriptions_[subscription]));
    lastError_ = Error::None;
    return true;
}

bool Bus::removeDeviceHandler(uint8_t subscription) {
    if (subscription >= MAX_SUBSCRIBERS || deviceSubscriptions_[subscription].handler == nullptr) {
        lastError_ = Error::InvalidSubscription;
        return false;
    }
    memset(&deviceSubscriptions_[subscription], 0, sizeof(deviceSubscriptions_[subscription]));
    lastError_ = Error::None;
    return true;
}

bool Bus::removeAckHandler(uint8_t subscription) {
    if (subscription >= MAX_SUBSCRIBERS || ackSubscriptions_[subscription].handler == nullptr) {
        lastError_ = Error::InvalidSubscription;
        return false;
    }
    memset(&ackSubscriptions_[subscription], 0, sizeof(ackSubscriptions_[subscription]));
    lastError_ = Error::None;
    return true;
}

Error Bus::lastError() const {
    return lastError_;
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
    while (offset < frame.size) {
        if (!packetAt(frame, offset, packet)) {
            ++diagnostics_.malformedPackets;
            break;
        }
        handlePacket(packet);
    }
}

void Bus::handlePacket(const PacketView &packet) {
    Device *knownDevice = findDevice(packet.deviceIdentifier);
    if (knownDevice != nullptr) {
        knownDevice->lastSeen = millis();
        if (packet.isReport() && knownDevice->reportsSinceAnnounce != 0xff) {
            ++knownDevice->reportsSinceAnnounce;
        }
    }
    if (packet.isReport() && packet.serviceIndex == SERVICE_INDEX_CONTROL && packet.serviceCommand == CMD_ANNOUNCE) {
        handleAnnounce(packet);
    }
    handleAck(packet);
    handleRegisterResponse(packet);
    handleCommandError(packet);
    if (packet.isEvent() && !acceptEvent(packet, knownDevice)) {
        return;
    }
    dispatchPacket(packet);
}

void Bus::handleRegisterResponse(const PacketView &packet) {
    if (!packet.isReport() || !packet.isRegisterGet()) return;
    for (uint8_t index = 0; index < MAX_REGISTER_REQUESTS; ++index) {
        RegisterRequest &request = registerRequests_[index];
        if (request.active && request.deviceIdentifier == packet.deviceIdentifier && request.serviceIndex == packet.serviceIndex && request.serviceCommand == packet.serviceCommand) {
            RegisterResponseHandler handler = request.handler;
            void *context = request.context;
            request.active = false;
            handler(&packet, context);
            return;
        }
    }
}

void Bus::handleCommandError(const PacketView &packet) {
    if (!packet.isReport() || packet.serviceCommand != CMD_COMMAND_NOT_IMPLEMENTED) return;
    if (packet.dataSize < 4) {
        ++diagnostics_.malformedPackets;
        return;
    }
    ++diagnostics_.commandErrors;
    if (commandErrorHandler_ != nullptr) {
        const Service source = service(packet.deviceIdentifier, packet.serviceIndex);
        commandErrorHandler_(source, readU16(packet.data), readU16(packet.data + 2), commandErrorContext_);
    }
}

void Bus::dispatchPacket(const PacketView &packet) {
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        const PacketSubscription subscription = packetSubscriptions_[index];
        if (subscription.handler != nullptr && (subscription.deviceIdentifier == 0 || subscription.deviceIdentifier == packet.deviceIdentifier) && (subscription.serviceIndex == 0xff || subscription.serviceIndex == packet.serviceIndex) && (subscription.serviceCommand == 0xffff || subscription.serviceCommand == packet.serviceCommand)) {
            subscription.handler(packet, subscription.context);
        }
    }
}

void Bus::dispatchDevice(const Device &device, DeviceEvent event) {
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        const DeviceSubscription subscription = deviceSubscriptions_[index];
        if (subscription.handler != nullptr) subscription.handler(device, event, subscription.context);
    }
}

void Bus::dispatchAck(uint64_t deviceIdentifier, uint16_t packetCrc, bool acknowledged) {
    for (uint8_t index = 0; index < MAX_SUBSCRIBERS; ++index) {
        const AckSubscription subscription = ackSubscriptions_[index];
        if (subscription.handler != nullptr) subscription.handler(deviceIdentifier, packetCrc, acknowledged, subscription.context);
    }
}

bool Bus::acceptEvent(const PacketView &packet, Device *knownDevice) {
    if (knownDevice == nullptr) {
        return true;
    }
    const uint8_t counter = packet.eventCounter();
    if (!knownDevice->eventCounterValid) {
        knownDevice->eventCounter = counter;
        knownDevice->eventCounterValid = true;
        return true;
    }
    const uint8_t distance = static_cast<uint8_t>((counter - knownDevice->eventCounter) & 0x7f);
    if (distance == 0) {
        ++diagnostics_.duplicateEvents;
        return false;
    }
    if (distance != 1) {
        ++diagnostics_.outOfOrderEvents;
        return false;
    }
    knownDevice->eventCounter = counter;
    return true;
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
    return -2;
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
            dispatchAck(request.frame.deviceIdentifier, request.crc, true);
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
            dispatchAck(request.frame.deviceIdentifier, request.crc, false);
        } else if (queueFrame(request.frame)) {
            ++request.attempts;
            ++diagnostics_.ackRetries;
            request.nextRetry = now + request.attempts * ACK_DELAY_MS;
        } else {
            request.nextRetry = now + ACK_DELAY_MS;
        }
    }
}

void Bus::processRegisterRequests(uint32_t now) {
    for (uint8_t index = 0; index < MAX_REGISTER_REQUESTS; ++index) {
        RegisterRequest &request = registerRequests_[index];
        if (request.active && static_cast<int32_t>(now - request.deadline) >= 0) {
            RegisterResponseHandler handler = request.handler;
            void *context = request.context;
            request.active = false;
            ++diagnostics_.registerTimeouts;
            handler(nullptr, context);
        }
    }
}

void Bus::handleAnnounce(const PacketView &packet) {
    if (packet.dataSize < 4) {
        ++diagnostics_.malformedPackets;
        return;
    }
    Device *knownDevice = findDevice(packet.deviceIdentifier);
    bool created = false;
    if (knownDevice == nullptr) {
        for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
            if (!devices_[index].connected()) {
                knownDevice = &devices_[index];
                memset(knownDevice, 0, sizeof(*knownDevice));
                knownDevice->deviceIdentifier = packet.deviceIdentifier;
                created = true;
                break;
            }
        }
    }
    if (knownDevice == nullptr) {
        ++diagnostics_.deviceOverflows;
        return;
    }
    const uint8_t previousRestartCounter = knownDevice->restartCounter;
    const uint8_t restartCounter = static_cast<uint8_t>(readU16(packet.data) & 0x0f);
    const bool restarted = !created && restartCounter < previousRestartCounter;
    const uint8_t observedReports = knownDevice->reportsSinceAnnounce;
    const uint8_t announcedReports = packet.data[2];
    knownDevice->lastSeen = millis();
    knownDevice->announceFlags = readU16(packet.data);
    knownDevice->packetCount = packet.data[2];
    knownDevice->restartCounter = restartCounter;
    knownDevice->reportsSinceAnnounce = 0;
    if (restarted) {
        knownDevice->eventCounterValid = false;
        ++diagnostics_.deviceRestarts;
        dispatchDevice(*knownDevice, DeviceEvent::Restarted);
    }
    if (!created && announcedReports != 0 && observedReports < announcedReports) {
        diagnostics_.missedReports += announcedReports - observedReports;
        knownDevice->eventCounterValid = false;
        dispatchDevice(*knownDevice, DeviceEvent::ReportsMissed);
    }
    uint8_t serviceCount = static_cast<uint8_t>((packet.dataSize - 4) / 4);
    if (serviceCount > MAX_SERVICES_PER_DEVICE) {
        serviceCount = MAX_SERVICES_PER_DEVICE;
    }
    knownDevice->serviceCount = serviceCount;
    for (uint8_t index = 0; index < serviceCount; ++index) {
        knownDevice->serviceClasses[index] = readU32(packet.data + 4 + index * 4);
    }
    if (created) dispatchDevice(*knownDevice, DeviceEvent::Connected);
}

void Bus::queueAnnounce() {
    uint8_t announce[4] = {0x00, 0x08, 0x01, 0x00};
    Frame frame;
    resetFrame(frame, selfIdentifier_, 0);
    if (appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, announce, sizeof(announce))) {
        finalizeFrame(frame);
        queueFrame(frame);
    }
}

Device *Bus::findDevice(uint64_t identifier) {
    for (uint8_t index = 0; index < MAX_DEVICES; ++index) {
        if (devices_[index].deviceIdentifier == identifier) {
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
        transmitting_ = NrfTransport::instance().send(txQueue_[txTail_]);
    }
}

} // namespace jacdac