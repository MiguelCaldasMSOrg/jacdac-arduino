#include "Jacdac.h"

namespace jacdac {

ServiceClient::ServiceClient(Bus &bus, uint32_t serviceClass, uint8_t instance) : bus_(bus), binding_(serviceClass, instance) {}

bool ServiceClient::connected() const {
    return resolve().valid();
}

Service ServiceClient::resolve() const {
    const Service service = bus_.resolve(binding_);
    if (!binding_.bound() && service.valid()) {
        binding_.bind(service);
    }
    return service;
}

bool ServiceClient::bind(const Service &service) {
    if (!service.valid() || service.serviceClass != binding_.serviceClass) {
        bus_.lastError_ = Error::InvalidService;
        return false;
    }
    binding_.bind(service);
    bus_.lastError_ = Error::None;
    return true;
}

void ServiceClient::clearBinding() {
    binding_.clear();
}

bool ServiceClient::fail(Error error) const {
    bus_.lastError_ = error;
    return false;
}

SensorClient::SensorClient(Bus &bus, uint32_t serviceClass, uint8_t instance) : ServiceClient(bus, serviceClass, instance) {}

bool SensorClient::requestReading() const {
    return bus_.getRegister(resolve(), reg::READING);
}

bool SensorClient::setStreaming(uint8_t samples) const {
    return bus_.setRegister(resolve(), reg::IS_STREAMING, samples);
}

bool SensorClient::setStreamingInterval(uint32_t milliseconds) const {
    return bus_.setRegister(resolve(), reg::STREAMING_INTERVAL, milliseconds);
}

bool SensorClient::setReadingRange(uint32_t range) const {
    return bus_.setRegister(resolve(), reg::READING_RANGE, range);
}

bool SensorClient::setInactiveThreshold(int32_t threshold) const {
    return bus_.setRegister(resolve(), reg::LOW_THRESHOLD, threshold);
}

bool SensorClient::setActiveThreshold(int32_t threshold) const {
    return bus_.setRegister(resolve(), reg::HIGH_THRESHOLD, threshold);
}

bool SensorClient::calibrate(bool requestAck) const {
    return bus_.sendCommand(resolve(), CMD_CALIBRATE, nullptr, 0, requestAck);
}

bool SensorClient::requestStatus() const {
    return bus_.getRegister(resolve(), reg::STATUS_CODE);
}

bool SensorClient::requestPreferredStreamingInterval() const {
    return bus_.getRegister(resolve(), reg::STREAMING_PREFERRED_INTERVAL);
}

bool SensorClient::requestReadingResolution() const {
    return bus_.getRegister(resolve(), reg::READING_RESOLUTION);
}

bool SensorClient::requestInstanceName() const {
    return bus_.getRegister(resolve(), reg::INSTANCE_NAME);
}

bool SensorClient::matchesReading(const PacketView &packet) const {
    const Service target = resolve();
    return target.valid() && packet.deviceIdentifier == target.deviceIdentifier && packet.serviceIndex == target.serviceIndex && packet.isRegisterGet() && packet.registerCode() == reg::READING;
}

ActuatorClient::ActuatorClient(Bus &bus, uint32_t serviceClass, uint8_t instance) : ServiceClient(bus, serviceClass, instance) {}

bool ActuatorClient::requestStatus() const {
    return bus_.getRegister(resolve(), reg::STATUS_CODE);
}

bool ActuatorClient::requestInstanceName() const {
    return bus_.getRegister(resolve(), reg::INSTANCE_NAME);
}

ButtonClient::ButtonClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::BUTTON, instance) {}

bool ButtonClient::requestPressure() const {
    return requestReading();
}

bool ButtonClient::requestPressed() const {
    return requestPressure();
}

bool ButtonClient::requestAnalog() const {
    return bus_.getRegister(resolve(), reg::BUTTON_ANALOG);
}

bool ButtonClient::readPressed(const PacketView &packet, bool &pressed) const {
    const Service target = resolve();
    if (!target.valid() || !packet.isReport() || packet.deviceIdentifier != target.deviceIdentifier || packet.serviceIndex != target.serviceIndex) return false;
    if (packet.isEvent()) {
        if (packet.eventCode() == event::BUTTON_DOWN || packet.eventCode() == event::BUTTON_HOLD) pressed = true;
        else if (packet.eventCode() == event::BUTTON_UP) pressed = false;
        else return false;
    } else if (packet.isRegisterGet() && packet.registerCode() == reg::READING) {
        uint16_t pressure;
        if (!readValue(packet, pressure)) return false;
        pressed = pressure != 0;
    } else {
        return false;
    }
    return true;
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

LightLevelClient::LightLevelClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::LIGHT_LEVEL, instance) {}

bool LightLevelClient::requestLightLevel() const {
    return requestReading();
}

bool LightLevelClient::requestVariant() const {
    return bus_.getRegister(resolve(), reg::VARIANT);
}

MagneticFieldLevelClient::MagneticFieldLevelClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::MAGNETIC_FIELD_LEVEL, instance) {}
bool MagneticFieldLevelClient::requestStrength() const { return requestReading(); }
bool MagneticFieldLevelClient::requestVariant() const { return bus_.getRegister(resolve(), reg::VARIANT); }

AccelerometerClient::AccelerometerClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::ACCELEROMETER, instance) {}

bool AccelerometerClient::requestForces() const {
    return requestReading();
}

DistanceClient::DistanceClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::DISTANCE, instance) {}

bool DistanceClient::requestDistance() const {
    return requestReading();
}

bool DistanceClient::requestVariant() const {
    return bus_.getRegister(resolve(), reg::VARIANT);
}

TemperatureClient::TemperatureClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::TEMPERATURE, instance) {}

bool TemperatureClient::requestTemperature() const {
    return requestReading();
}

bool TemperatureClient::requestVariant() const {
    return bus_.getRegister(resolve(), reg::VARIANT);
}

HumidityClient::HumidityClient(Bus &bus, uint8_t instance) : SensorClient(bus, service::HUMIDITY, instance) {}

bool HumidityClient::requestHumidity() const {
    return requestReading();
}

LedStripClient::LedStripClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::LED_STRIP, instance) {}

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
        bus_.lastError_ = Error::InvalidArgument;
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

LedClient::LedClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::LED, instance) {}

bool LedClient::setBrightness(uint8_t brightness, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::INTENSITY, brightness, requestAck);
}

bool LedClient::setPixels(const uint8_t *rgb, uint8_t byteCount, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::VALUE, static_cast<const void *>(rgb), byteCount, requestAck);
}

bool LedClient::requestPixels() const { return bus_.getRegister(resolve(), reg::VALUE); }
bool LedClient::requestNumPixels() const { return bus_.getRegister(resolve(), reg::LED_NUM_PIXELS); }
bool LedClient::requestVariant() const { return bus_.getRegister(resolve(), reg::VARIANT); }
bool LedClient::requestActualBrightness() const { return bus_.getRegister(resolve(), reg::LED_ACTUAL_BRIGHTNESS); }

ServoClient::ServoClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::SERVO, instance) {}

bool ServoClient::setAngle(float angleDegrees, bool requestAck) const {
    return setAngleQ16(floatToQ16(angleDegrees), requestAck);
}

bool ServoClient::setAngleQ16(int32_t angleDegreesQ16, bool requestAck) const {
    return bus_.setRegister(resolve(), reg::VALUE, angleDegreesQ16, requestAck);
}

bool ServoClient::setEnabled(bool enabled, bool requestAck) const {
    const uint8_t intensity = enabled ? 1 : 0;
    return bus_.setRegister(resolve(), reg::INTENSITY, intensity, requestAck);
}

bool ServoClient::requestAngle() const {
    return bus_.getRegister(resolve(), reg::VALUE);
}

bool ServoClient::requestEnabled() const {
    return bus_.getRegister(resolve(), reg::INTENSITY);
}

bool ServoClient::requestMinAngle() const {
    return bus_.getRegister(resolve(), reg::MIN_VALUE);
}

bool ServoClient::requestMaxAngle() const {
    return bus_.getRegister(resolve(), reg::MAX_VALUE);
}

bool ServoClient::requestActualAngle() const {
    return bus_.getRegister(resolve(), reg::READING);
}

RelayClient::RelayClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::RELAY, instance) {}
bool RelayClient::setActive(bool active, bool requestAck) const { const uint8_t value = active ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }
bool RelayClient::requestActive() const { return bus_.getRegister(resolve(), reg::INTENSITY); }
bool RelayClient::requestVariant() const { return bus_.getRegister(resolve(), reg::VARIANT); }
bool RelayClient::requestMaxSwitchingCurrent() const { return bus_.getRegister(resolve(), 0x180); }

VibrationMotorClient::VibrationMotorClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::VIBRATION_MOTOR, instance) {}
bool VibrationMotorClient::vibrate(const VibrationStep *steps, uint8_t count, bool requestAck) const { if (count > SERIAL_PAYLOAD_SIZE / sizeof(VibrationStep)) { return fail(Error::PacketTooLarge); } return bus_.sendCommand(resolve(), command::VIBRATION_MOTOR_VIBRATE, steps, static_cast<uint8_t>(count * sizeof(VibrationStep)), requestAck); }
bool VibrationMotorClient::stop(bool requestAck) const { return bus_.sendCommand(resolve(), command::VIBRATION_MOTOR_VIBRATE, nullptr, 0, requestAck); }
bool VibrationMotorClient::requestMaxVibrations() const { return bus_.getRegister(resolve(), reg::VIBRATION_MOTOR_MAX_VIBRATIONS); }

PowerClient::PowerClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::POWER, instance) {}
bool PowerClient::setAllowed(bool allowed, bool requestAck) const { const uint8_t value = allowed ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }
bool PowerClient::setMaxPower(uint16_t milliamps, bool requestAck) const { return bus_.setRegister(resolve(), reg::MAX_POWER, milliamps, requestAck); }
bool PowerClient::requestAllowed() const { return bus_.getRegister(resolve(), reg::INTENSITY); }
bool PowerClient::requestMaxPower() const { return bus_.getRegister(resolve(), reg::MAX_POWER); }
bool PowerClient::requestCurrentDraw() const { return bus_.getRegister(resolve(), reg::READING); }
bool PowerClient::requestBatteryVoltage() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_VOLTAGE); }
bool PowerClient::requestPowerStatus() const { return bus_.getRegister(resolve(), reg::POWER_STATUS); }
bool PowerClient::requestBatteryCharge() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_CHARGE); }
bool PowerClient::requestBatteryCapacity() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_CAPACITY); }
bool PowerClient::requestKeepOnPulseDuration() const { return bus_.getRegister(resolve(), reg::POWER_KEEP_ON_PULSE_DURATION); }
bool PowerClient::requestKeepOnPulsePeriod() const { return bus_.getRegister(resolve(), reg::POWER_KEEP_ON_PULSE_PERIOD); }

bool PowerClient::setKeepOnPulse(uint16_t durationMilliseconds, uint16_t periodMilliseconds, bool requestAck) const {
    if (periodMilliseconds == 0 || static_cast<uint32_t>(durationMilliseconds) * 10 > periodMilliseconds) return fail(Error::InvalidArgument);
    const Service target = resolve();
    if (!target.valid()) return fail(Error::InvalidService);
    CommandBatch batch(target.deviceIdentifier, requestAck);
    const uint16_t disabledDuration = 0;
    if (!batch.add(target, CMD_SET_REGISTER | reg::POWER_KEEP_ON_PULSE_DURATION, disabledDuration) ||
        !batch.add(target, CMD_SET_REGISTER | reg::POWER_KEEP_ON_PULSE_PERIOD, periodMilliseconds) ||
        !batch.add(target, CMD_SET_REGISTER | reg::POWER_KEEP_ON_PULSE_DURATION, durationMilliseconds)) return fail(batch.error());
    return bus_.sendBatch(batch);
}

} // namespace jacdac