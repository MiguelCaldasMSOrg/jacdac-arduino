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

struct Device {
    uint64_t deviceIdentifier;

private:
    friend class Bus;
    uint32_t lastSeen;
    uint16_t announceFlags;
    uint8_t packetCount;

public:
    uint8_t serviceCount;

private:
    uint8_t restartCounter;
    uint8_t reportsSinceAnnounce;

public:
    uint32_t serviceClasses[MAX_SERVICES_PER_DEVICE];

    bool connected() const { return deviceIdentifier != 0; }

private:
    uint8_t eventCounter;
    bool eventCounterValid;
};

struct Service {
    uint64_t deviceIdentifier;
    uint32_t serviceClass;
    uint8_t serviceIndex;

    bool valid() const { return deviceIdentifier != 0; }
};

enum class DeviceEvent : uint8_t {
    Connected,
    Disconnected,
    Restarted,
    ReportsMissed
};

using PacketHandler = void (*)(const PacketView &packet, void *context);
using DeviceHandler = void (*)(const Device &device, DeviceEvent event, void *context);
using AckHandler = void (*)(uint64_t deviceIdentifier, uint16_t packetCrc, bool acknowledged, void *context);
using RegisterResponseHandler = void (*)(const PacketView *packet, void *context);
using CommandErrorHandler = void (*)(const Service &service, uint16_t serviceCommand, uint16_t packetCrc, void *context);

struct Diagnostics {
    uint32_t framesReceived;
    uint32_t framesSent;
    uint32_t crcErrors;
    uint32_t receiveOverflows;
    uint32_t transmitOverflows;
    uint32_t malformedPackets;
    uint32_t deviceOverflows;
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
    uint32_t fallingEdges;
    uint32_t receiveStarts;
    uint32_t receiveCompletions;
    uint32_t receiveBytes;
    uint32_t receiveTimeouts;
    uint32_t receiveShortFrames;
    uint32_t receiveInvalidFrames;
    uint32_t receiveHardwareErrors;
};

struct ServiceBinding {
    uint64_t deviceIdentifier;
    uint32_t serviceClass;
    uint8_t serviceIndex;
    uint8_t instance;

    explicit ServiceBinding(uint32_t serviceClass = 0, uint8_t instance = 0) : deviceIdentifier(0), serviceClass(serviceClass), serviceIndex(0), instance(instance) {}
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
    Error error() const { return error_; }

private:
    friend class Bus;
    Frame frame_;
    uint8_t packetCount_;
    Error error_;
};

class Bus {
public:
    Bus();
    ~Bus();
    Bus(const Bus &) = delete;
    Bus &operator=(const Bus &) = delete;
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

    void setCommandErrorHandler(CommandErrorHandler handler, void *context = nullptr);
    uint8_t addPacketHandler(PacketHandler handler, void *context = nullptr, uint64_t deviceIdentifier = 0, uint8_t serviceIndex = 0xff, uint16_t serviceCommand = 0xffff);
    uint8_t addDeviceHandler(DeviceHandler handler, void *context = nullptr);
    uint8_t addAckHandler(AckHandler handler, void *context = nullptr);
    bool removePacketHandler(uint8_t subscription);
    bool removeDeviceHandler(uint8_t subscription);
    bool removeAckHandler(uint8_t subscription);
    Error lastError() const;
    const Diagnostics &diagnostics() const;

private:
    friend class LedStripClient;
    friend class ServiceClient;
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
    void dispatchDevice(const Device &device, DeviceEvent event);
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
    Error lastError_;
    Diagnostics diagnostics_;
};

class ServiceClient {
public:
    ServiceClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
    bool connected() const;
    Service resolve() const;
    bool bind(const Service &service);
    void clearBinding();

protected:
    bool fail(Error error) const;
    Bus &bus_;
    mutable ServiceBinding binding_;
};

class SensorClient : public ServiceClient {
public:
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

};

class ActuatorClient : public ServiceClient {
public:
    ActuatorClient(Bus &bus, uint32_t serviceClass, uint8_t instance = 0);
    bool requestStatus() const;
    bool requestInstanceName() const;

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

class LedStripClient : public ServiceClient {
public:
    explicit LedStripClient(Bus &bus, uint8_t instance = 0);
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

};

class LedClient : public ServiceClient {
public:
    explicit LedClient(Bus &bus, uint8_t instance = 0);
    bool setBrightness(uint8_t brightness, bool requestAck = false) const;
    bool setPixels(const uint8_t *rgb, uint8_t byteCount, bool requestAck = false) const;
};

class ServoClient : public ServiceClient {
public:
    explicit ServoClient(Bus &bus, uint8_t instance = 0);
    bool setAngle(float angleDegrees, bool requestAck = false) const;
    bool setAngleQ16(int32_t angleDegreesQ16, bool requestAck = false) const;
    bool setEnabled(bool enabled, bool requestAck = false) const;
};

class RelayClient : public ActuatorClient {
public:
    explicit RelayClient(Bus &bus, uint8_t instance = 0);
    bool setActive(bool active, bool requestAck = false) const;
    bool requestVariant() const;
    bool requestMaxSwitchingCurrent() const;
};

class LightBulbClient : public ActuatorClient {
public:
    explicit LightBulbClient(Bus &bus, uint8_t instance = 0);
    bool setBrightness(uint16_t brightness, bool requestAck = false) const;
    bool requestDimmable() const;
};

class MotorClient : public ActuatorClient {
public:
    explicit MotorClient(Bus &bus, uint8_t instance = 0);
    bool setSpeed(int16_t speedQ15, bool requestAck = false) const;
    bool setEnabled(bool enabled, bool requestAck = false) const;
};

class DualMotorsClient : public ActuatorClient {
public:
    explicit DualMotorsClient(Bus &bus, uint8_t instance = 0);
    bool setSpeeds(int16_t leftQ15, int16_t rightQ15, bool requestAck = false) const;
    bool setEnabled(bool enabled, bool requestAck = false) const;
};

class BuzzerClient : public ActuatorClient {
public:
    explicit BuzzerClient(Bus &bus, uint8_t instance = 0);
    bool setVolume(uint8_t volume, bool requestAck = false) const;
    bool playTone(uint16_t periodMicroseconds, uint16_t dutyMicroseconds, uint16_t durationMilliseconds, bool requestAck = false) const;
    bool playNote(uint16_t frequency, uint16_t volume, uint16_t durationMilliseconds, bool requestAck = false) const;
};

struct VibrationStep {
    uint8_t duration8Milliseconds;
    uint8_t intensity;
};

class VibrationMotorClient : public ServiceClient {
public:
    explicit VibrationMotorClient(Bus &bus, uint8_t instance = 0);
    bool vibrate(const VibrationStep *steps, uint8_t count, bool requestAck = false) const;
    bool stop(bool requestAck = false) const;
    bool requestMaxVibrations() const;
};

class HidKeyboardClient : public ServiceClient {
public:
    explicit HidKeyboardClient(Bus &bus, uint8_t instance = 0);
    bool key(uint16_t selector, uint8_t modifiers = 0, uint8_t action = 0, bool requestAck = false) const;
    bool clear(bool requestAck = false) const;
};

class HidMouseClient : public ServiceClient {
public:
    explicit HidMouseClient(Bus &bus, uint8_t instance = 0);
    bool setButton(uint16_t buttons, uint8_t event, bool requestAck = false) const;
    bool move(int16_t deltaX, int16_t deltaY, uint16_t timeMilliseconds = 0, bool requestAck = false) const;
    bool wheel(int16_t deltaY, uint16_t timeMilliseconds = 0, bool requestAck = false) const;
};

class HidJoystickClient : public ServiceClient {
public:
    explicit HidJoystickClient(Bus &bus, uint8_t instance = 0);
    bool setButtons(const uint8_t *pressures, uint8_t count, bool requestAck = false) const;
    bool setAxes(const int16_t *positionsQ15, uint8_t count, bool requestAck = false) const;
    bool requestButtonCount() const;
    bool requestAnalogButtons() const;
    bool requestAxisCount() const;
};

class CharacterScreenClient : public ActuatorClient {
public:
    explicit CharacterScreenClient(Bus &bus, uint8_t instance = 0);
    bool setMessage(const char *message, uint8_t size, bool requestAck = false) const;
    bool setBrightness(uint16_t brightness, bool requestAck = false) const;
    bool requestRows() const;
    bool requestColumns() const;
    bool requestVariant() const;
};

class CursorCharacterScreenClient : public ActuatorClient {
public:
    explicit CursorCharacterScreenClient(Bus &bus, uint8_t instance = 0);
    bool setEnabled(uint16_t enabled, bool requestAck = false) const;
    bool home(bool requestAck = false) const;
    bool clear(bool requestAck = false) const;
    bool setCursor(uint8_t x, uint8_t y, bool requestAck = false) const;
    bool show(const char *message, uint8_t size, bool requestAck = false) const;
    bool requestRows() const;
    bool requestColumns() const;
};

class PowerClient : public ActuatorClient {
public:
    explicit PowerClient(Bus &bus, uint8_t instance = 0);
    bool setAllowed(bool allowed, bool requestAck = false) const;
    bool setMaxPower(uint16_t milliamps, bool requestAck = false) const;
    bool requestCurrentDraw() const;
    bool requestBatteryVoltage() const;
    bool requestPowerStatus() const;
    bool requestBatteryCharge() const;
    bool requestBatteryCapacity() const;
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