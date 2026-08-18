#pragma once

#include <Arduino.h>
#include "JacdacProtocol.h"
#include "JacdacServices.h"

namespace jacdac {

constexpr uint8_t MAX_DEVICES = JACDAC_MAX_DEVICES;
constexpr uint8_t MAX_SERVICES_PER_DEVICE = JACDAC_MAX_SERVICES_PER_DEVICE;
constexpr uint8_t RX_QUEUE_SIZE = JACDAC_RX_QUEUE_SIZE;
constexpr uint8_t TX_QUEUE_SIZE = JACDAC_TX_QUEUE_SIZE;
constexpr uint8_t MAX_ACK_REQUESTS = JACDAC_MAX_ACK_REQUESTS;
constexpr uint8_t MAX_REGISTER_REQUESTS = JACDAC_MAX_REGISTER_REQUESTS;
constexpr uint8_t MAX_SUBSCRIBERS = JACDAC_MAX_SUBSCRIBERS;
constexpr uint8_t INVALID_SUBSCRIPTION = 0xff;
constexpr uint8_t ACK_ATTEMPTS = 4;
constexpr uint32_t ACK_DELAY_MS = 40;
constexpr uint32_t DEVICE_TIMEOUT_MS = 2000;

struct Device {
    uint64_t identifier;
    uint32_t lastSeen;
    uint16_t announceFlags;
    uint8_t packetCount;
    uint8_t serviceCount;
    uint8_t restartCounter;
    uint8_t eventCounter;
    uint8_t reportsSinceAnnounce;
    bool eventCounterValid;
    uint32_t serviceClasses[MAX_SERVICES_PER_DEVICE];

    bool connected() const { return identifier != 0; }
};

struct Service {
    uint64_t deviceIdentifier;
    uint32_t serviceClass;
    uint8_t serviceIndex;

    bool valid() const { return deviceIdentifier != 0; }
};

using PacketHandler = void (*)(const PacketView &packet, void *context);
using DeviceHandler = void (*)(const Device &device, bool connected, void *context);
using AckHandler = void (*)(uint64_t deviceIdentifier, uint16_t packetCrc, bool acknowledged, void *context);
using RegisterResponseHandler = void (*)(const PacketView *packet, void *context);
using CommandErrorHandler = void (*)(const Service &service, uint16_t serviceCommand, uint16_t packetCrc, void *context);

enum class DeviceEvent : uint8_t {
    Connected,
    Disconnected,
    Restarted,
    ReportsMissed
};

using DeviceEventHandler = void (*)(const Device &device, DeviceEvent event, void *context);

struct Diagnostics {
    uint32_t framesReceived;
    uint32_t framesSent;
    uint32_t crcErrors;
    uint32_t receiveOverflows;
    uint32_t transmitOverflows;
    uint32_t busErrors;
    uint32_t collisions;
    uint32_t acksReceived;
    uint32_t ackRetries;
    uint32_t ackTimeouts;
    uint32_t duplicateEvents;
    uint32_t outOfOrderEvents;
    uint32_t deviceRestarts;
    uint32_t missedReports;
    uint32_t registerTimeouts;
    uint32_t commandErrors;
};

struct ServiceBinding {
    uint32_t serviceClass;
    uint64_t deviceIdentifier;
    uint8_t serviceIndex;
    uint8_t instance;

    explicit ServiceBinding(uint32_t serviceClass = 0, uint8_t instance = 0) : serviceClass(serviceClass), deviceIdentifier(0), serviceIndex(0), instance(instance) {}
    bool bound() const { return deviceIdentifier != 0; }
    void bind(const Service &service) { deviceIdentifier = service.deviceIdentifier; serviceIndex = service.serviceIndex; serviceClass = service.serviceClass; }
    void clear() { deviceIdentifier = 0; serviceIndex = 0; }
};

class CommandBatch {
public:
    explicit CommandBatch(uint64_t deviceIdentifier, bool requestAck = false);
    bool add(const Service &service, uint16_t command, const void *data = nullptr, uint8_t size = 0);
    template <typename T> bool add(const Service &service, uint16_t command, const T &value) { return add(service, command, &value, sizeof(value)); }
    uint8_t packetCount() const { return packetCount_; }

private:
    friend class Bus;
    Frame frame_;
    uint8_t packetCount_;
};

class Bus {
public:
    Bus();
    bool begin(uint8_t pin = 12);
    void end();
    void process();
    bool running() const;

    uint8_t deviceCount() const;
    const Device *device(uint8_t index) const;
    Service findService(uint32_t serviceClass, uint8_t instance = 0) const;
    Service service(uint64_t deviceIdentifier, uint8_t serviceIndex) const;
    Service resolve(const ServiceBinding &binding) const;

    bool sendCommand(const Service &service, uint16_t command, const void *data = nullptr, uint8_t size = 0, bool requestAck = false);
    bool getRegister(const Service &service, uint16_t reg);
    bool setRegister(const Service &service, uint16_t reg, const void *data, uint8_t size, bool requestAck = false);
    bool getRegisterAsync(const Service &service, uint16_t reg, RegisterResponseHandler handler, void *context = nullptr, uint32_t timeoutMs = 1000);
    bool sendMulticast(uint32_t serviceClass, uint16_t command, const void *data = nullptr, uint8_t size = 0);
    bool sendBatch(const CommandBatch &batch);

    bool identify(uint64_t deviceIdentifier, bool requestAck = false);
    bool resetDevice(uint64_t deviceIdentifier, bool requestAck = false);
    bool standby(uint64_t deviceIdentifier, uint32_t durationMs, bool requestAck = false);
    bool setStatusLight(uint64_t deviceIdentifier, uint8_t red, uint8_t green, uint8_t blue, uint8_t speed = 0);
    bool requestDeviceDescription(uint64_t deviceIdentifier);
    bool requestProductIdentifier(uint64_t deviceIdentifier);
    bool requestFirmwareVersion(uint64_t deviceIdentifier);
    bool requestUptime(uint64_t deviceIdentifier);

    template <typename T> bool setRegister(const Service &service, uint16_t reg, const T &value, bool requestAck = false) {
        return setRegister(service, reg, &value, sizeof(value), requestAck);
    }

    void onPacket(PacketHandler handler, void *context = nullptr);
    void onDevice(DeviceHandler handler, void *context = nullptr);
    void onAck(AckHandler handler, void *context = nullptr);
    void onDeviceEvent(DeviceEventHandler handler, void *context = nullptr);
    void onCommandError(CommandErrorHandler handler, void *context = nullptr);
    uint8_t addPacketHandler(PacketHandler handler, void *context = nullptr, uint64_t deviceIdentifier = 0, uint8_t serviceIndex = 0xff, uint16_t serviceCommand = 0xffff);
    uint8_t addDeviceHandler(DeviceHandler handler, void *context = nullptr);
    uint8_t addAckHandler(AckHandler handler, void *context = nullptr);
    bool removePacketHandler(uint8_t subscription);
    bool removeDeviceHandler(uint8_t subscription);
    bool removeAckHandler(uint8_t subscription);
    const Diagnostics &diagnostics() const;

private:
    static void receiveFromTransport(const Frame &frame, void *context);
    static void transmitDoneFromTransport(void *context);
    void handleFrame(const Frame &frame);
    void handlePacket(const PacketView &packet);
    void handleAnnounce(const PacketView &packet);
    bool acceptEvent(const PacketView &packet, Device *device);
    int8_t trackAck(const Frame &frame);
    void handleAck(const PacketView &packet);
    void processAcks(uint32_t now);
    void processRegisterRequests(uint32_t now);
    void handleRegisterResponse(const PacketView &packet);
    void handleCommandError(const PacketView &packet);
    void dispatchPacket(const PacketView &packet);
    void dispatchDevice(const Device &device, bool connected);
    void dispatchAck(uint64_t deviceIdentifier, uint16_t packetCrc, bool acknowledged);
    void queueAnnounce();
    Device *findDevice(uint64_t identifier);
    bool queueFrame(const Frame &frame);
    void startNextTransmission();

    Device devices_[MAX_DEVICES];
    Frame rxQueue_[RX_QUEUE_SIZE + 1];
    Frame txQueue_[TX_QUEUE_SIZE + 1];
    volatile uint8_t rxHead_;
    volatile uint8_t rxTail_;
    volatile uint8_t txHead_;
    volatile uint8_t txTail_;
    volatile bool transmitting_;
    bool running_;
    PacketHandler packetHandler_;
    DeviceHandler deviceHandler_;
    void *packetContext_;
    void *deviceContext_;
    AckHandler ackHandler_;
    void *ackContext_;
    DeviceEventHandler deviceEventHandler_;
    void *deviceEventContext_;
    CommandErrorHandler commandErrorHandler_;
    void *commandErrorContext_;
    uint64_t selfIdentifier_;
    uint32_t nextAnnounce_;
    struct AckRequest {
        Frame frame;
        uint32_t nextRetry;
        uint16_t crc;
        uint8_t attempts;
        bool active;
    } ackRequests_[MAX_ACK_REQUESTS];
    struct RegisterRequest {
        uint64_t deviceIdentifier;
        uint32_t deadline;
        RegisterResponseHandler handler;
        void *context;
        uint16_t serviceCommand;
        uint8_t serviceIndex;
        bool active;
    } registerRequests_[MAX_REGISTER_REQUESTS];
    struct PacketSubscription {
        PacketHandler handler;
        void *context;
        uint64_t deviceIdentifier;
        uint16_t serviceCommand;
        uint8_t serviceIndex;
    } packetSubscriptions_[MAX_SUBSCRIBERS];
    struct DeviceSubscription {
        DeviceHandler handler;
        void *context;
    } deviceSubscriptions_[MAX_SUBSCRIBERS];
    struct AckSubscription {
        AckHandler handler;
        void *context;
    } ackSubscriptions_[MAX_SUBSCRIBERS];
    Diagnostics diagnostics_;
};

class SensorClient {
public:
    SensorClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
    bool connected() const;
    Service resolve() const;
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

protected:
    Bus &bus_;
    uint32_t serviceClass_;
    uint8_t instance_;
};

class ActuatorClient {
public:
    ActuatorClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
    bool connected() const;
    Service resolve() const;
    bool setIntensity(uint32_t intensity, bool requestAck = false) const;
    bool setValue(int32_t value, bool requestAck = false) const;
    bool requestStatus() const;
    bool requestInstanceName() const;

private:
    Bus &bus_;
    uint32_t serviceClass_;
    uint8_t instance_;
};

class ButtonClient : public SensorClient {
public:
    explicit ButtonClient(Bus &bus, uint8_t instance = 0);
    bool requestPressure() const;
    bool requestPressed() const;
    bool requestAnalog() const;
};

class RotaryEncoderClient : public SensorClient {
public:
    explicit RotaryEncoderClient(Bus &bus, uint8_t instance = 0);
    bool requestPosition() const;
    bool requestClicksPerTurn() const;
    bool requestClicker() const;
    Service buttonService() const;
};

class PotentiometerClient : public SensorClient {
public:
    explicit PotentiometerClient(Bus &bus, uint8_t instance = 0);
    bool requestPosition() const;
    bool requestVariant() const;
};

class LedStripClient {
public:
    explicit LedStripClient(Bus &bus, uint8_t instance = 0);
    bool connected() const;
    Service resolve() const;
    bool setBrightness(uint8_t brightness, bool requestAck = false) const;
    bool setNumPixels(uint16_t numPixels, bool requestAck = false) const;
    bool setMaxPower(uint16_t milliamps, bool requestAck = false) const;
    bool setNumRepeats(uint16_t repeats, bool requestAck = false) const;
    bool requestActualBrightness() const;
    bool requestNumPixels() const;
    bool requestMaxPixels() const;
    bool requestVariant() const;
    bool runProgram(const uint8_t *program, uint8_t size, bool requestAck = false) const;
    bool setAll(uint8_t red, uint8_t green, uint8_t blue, bool requestAck = false) const;
    bool setPixel(uint16_t pixel, uint8_t red, uint8_t green, uint8_t blue, bool requestAck = false) const;

private:
    Bus &bus_;
    uint8_t instance_;
};

class LedClient {
public:
    explicit LedClient(Bus &bus, uint8_t instance = 0);
    bool connected() const;
    bool setBrightness(uint8_t brightness) const;
    bool setPixels(const uint8_t *rgb, uint8_t byteCount) const;

private:
    Bus &bus_;
    uint8_t instance_;
};

class ServoClient {
public:
    explicit ServoClient(Bus &bus, uint8_t instance = 0);
    bool connected() const;
    bool setAngle(int32_t angleDegreesQ16) const;
    bool setEnabled(bool enabled) const;

private:
    Bus &bus_;
    uint8_t instance_;
};

extern Bus Jacdac;

template <typename T> bool readValue(const PacketView &packet, T &value) {
    if (packet.dataSize < sizeof(T)) {
        return false;
    }
    memcpy(&value, packet.data, sizeof(T));
    return true;
}

inline float q10ToFloat(int32_t value) { return value / 1024.0f; }
inline float uq16ToFloat(uint16_t value) { return value / 65535.0f; }
inline int32_t floatToQ16(float value) { return static_cast<int32_t>(value * 65536.0f); }

} // namespace jacdac