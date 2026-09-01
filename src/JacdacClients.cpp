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

RelayClient::RelayClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::RELAY, instance) {}
bool RelayClient::setActive(bool active, bool requestAck) const { const uint8_t value = active ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }
bool RelayClient::requestVariant() const { return bus_.getRegister(resolve(), reg::VARIANT); }
bool RelayClient::requestMaxSwitchingCurrent() const { return bus_.getRegister(resolve(), 0x180); }

LightBulbClient::LightBulbClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::LIGHT_BULB, instance) {}
bool LightBulbClient::setBrightness(uint16_t brightness, bool requestAck) const { return bus_.setRegister(resolve(), reg::INTENSITY, brightness, requestAck); }
bool LightBulbClient::requestDimmable() const { return bus_.getRegister(resolve(), 0x180); }

MotorClient::MotorClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::MOTOR, instance) {}
bool MotorClient::setSpeed(int16_t speedQ15, bool requestAck) const { return bus_.setRegister(resolve(), reg::VALUE, speedQ15, requestAck); }
bool MotorClient::setEnabled(bool enabled, bool requestAck) const { const uint8_t value = enabled ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }

DualMotorsClient::DualMotorsClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::DUAL_MOTORS, instance) {}
bool DualMotorsClient::setSpeeds(int16_t leftQ15, int16_t rightQ15, bool requestAck) const { const int16_t speeds[] = {leftQ15, rightQ15}; return bus_.setRegister(resolve(), reg::VALUE, speeds, sizeof(speeds), requestAck); }
bool DualMotorsClient::setEnabled(bool enabled, bool requestAck) const { const uint8_t value = enabled ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }

BuzzerClient::BuzzerClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::BUZZER, instance) {}
bool BuzzerClient::setVolume(uint8_t volume, bool requestAck) const { return bus_.setRegister(resolve(), reg::INTENSITY, volume, requestAck); }
bool BuzzerClient::playTone(uint16_t periodMicroseconds, uint16_t dutyMicroseconds, uint16_t durationMilliseconds, bool requestAck) const { const uint16_t data[] = {periodMicroseconds, dutyMicroseconds, durationMilliseconds}; return bus_.sendCommand(resolve(), command::BUZZER_PLAY_TONE, data, sizeof(data), requestAck); }
bool BuzzerClient::playNote(uint16_t frequency, uint16_t volume, uint16_t durationMilliseconds, bool requestAck) const { const uint16_t data[] = {frequency, volume, durationMilliseconds}; return bus_.sendCommand(resolve(), command::BUZZER_PLAY_NOTE, data, sizeof(data), requestAck); }

VibrationMotorClient::VibrationMotorClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::VIBRATION_MOTOR, instance) {}
bool VibrationMotorClient::vibrate(const VibrationStep *steps, uint8_t count, bool requestAck) const { if (count > SERIAL_PAYLOAD_SIZE / sizeof(VibrationStep)) { return fail(Error::PacketTooLarge); } return bus_.sendCommand(resolve(), command::VIBRATION_MOTOR_VIBRATE, steps, static_cast<uint8_t>(count * sizeof(VibrationStep)), requestAck); }
bool VibrationMotorClient::stop(bool requestAck) const { return bus_.sendCommand(resolve(), command::VIBRATION_MOTOR_VIBRATE, nullptr, 0, requestAck); }
bool VibrationMotorClient::requestMaxVibrations() const { return bus_.getRegister(resolve(), reg::VIBRATION_MOTOR_MAX_VIBRATIONS); }

HidKeyboardClient::HidKeyboardClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::HID_KEYBOARD, instance) {}
bool HidKeyboardClient::key(uint16_t selector, uint8_t modifiers, uint8_t action, bool requestAck) const { const uint8_t data[] = {static_cast<uint8_t>(selector), static_cast<uint8_t>(selector >> 8), modifiers, action}; return bus_.sendCommand(resolve(), command::HID_KEYBOARD_KEY, data, sizeof(data), requestAck); }
bool HidKeyboardClient::clear(bool requestAck) const { return bus_.sendCommand(resolve(), command::HID_KEYBOARD_CLEAR, nullptr, 0, requestAck); }

HidMouseClient::HidMouseClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::HID_MOUSE, instance) {}
bool HidMouseClient::setButton(uint16_t buttons, uint8_t event, bool requestAck) const { const uint8_t data[] = {static_cast<uint8_t>(buttons), static_cast<uint8_t>(buttons >> 8), event}; return bus_.sendCommand(resolve(), command::HID_MOUSE_SET_BUTTON, data, sizeof(data), requestAck); }
bool HidMouseClient::move(int16_t deltaX, int16_t deltaY, uint16_t timeMilliseconds, bool requestAck) const { const uint16_t data[] = {static_cast<uint16_t>(deltaX), static_cast<uint16_t>(deltaY), timeMilliseconds}; return bus_.sendCommand(resolve(), command::HID_MOUSE_MOVE, data, sizeof(data), requestAck); }
bool HidMouseClient::wheel(int16_t deltaY, uint16_t timeMilliseconds, bool requestAck) const { const uint16_t data[] = {static_cast<uint16_t>(deltaY), timeMilliseconds}; return bus_.sendCommand(resolve(), command::HID_MOUSE_WHEEL, data, sizeof(data), requestAck); }

HidJoystickClient::HidJoystickClient(Bus &bus, uint8_t instance) : ServiceClient(bus, service::HID_JOYSTICK, instance) {}
bool HidJoystickClient::setButtons(const uint8_t *pressures, uint8_t count, bool requestAck) const { return bus_.sendCommand(resolve(), command::HID_JOYSTICK_SET_BUTTONS, pressures, count, requestAck); }
bool HidJoystickClient::setAxes(const int16_t *positionsQ15, uint8_t count, bool requestAck) const { if (count > SERIAL_PAYLOAD_SIZE / sizeof(int16_t)) { return fail(Error::PacketTooLarge); } return bus_.sendCommand(resolve(), command::HID_JOYSTICK_SET_AXIS, positionsQ15, static_cast<uint8_t>(count * sizeof(int16_t)), requestAck); }
bool HidJoystickClient::requestButtonCount() const { return bus_.getRegister(resolve(), reg::HID_JOYSTICK_BUTTON_COUNT); }
bool HidJoystickClient::requestAnalogButtons() const { return bus_.getRegister(resolve(), reg::HID_JOYSTICK_BUTTONS_ANALOG); }
bool HidJoystickClient::requestAxisCount() const { return bus_.getRegister(resolve(), reg::HID_JOYSTICK_AXIS_COUNT); }

CharacterScreenClient::CharacterScreenClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::CHARACTER_SCREEN, instance) {}
bool CharacterScreenClient::setMessage(const char *message, uint8_t size, bool requestAck) const { return bus_.setRegister(resolve(), reg::VALUE, message, size, requestAck); }
bool CharacterScreenClient::setBrightness(uint16_t brightness, bool requestAck) const { return bus_.setRegister(resolve(), reg::INTENSITY, brightness, requestAck); }
bool CharacterScreenClient::requestRows() const { return bus_.getRegister(resolve(), reg::DISPLAY_ROWS); }
bool CharacterScreenClient::requestColumns() const { return bus_.getRegister(resolve(), reg::DISPLAY_COLUMNS); }
bool CharacterScreenClient::requestVariant() const { return bus_.getRegister(resolve(), reg::VARIANT); }

CursorCharacterScreenClient::CursorCharacterScreenClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::CURSOR_CHARACTER_SCREEN, instance) {}
bool CursorCharacterScreenClient::setEnabled(uint16_t enabled, bool requestAck) const { return bus_.setRegister(resolve(), reg::INTENSITY, enabled, requestAck); }
bool CursorCharacterScreenClient::home(bool requestAck) const { return bus_.sendCommand(resolve(), command::CURSOR_SCREEN_HOME, nullptr, 0, requestAck); }
bool CursorCharacterScreenClient::clear(bool requestAck) const { return bus_.sendCommand(resolve(), command::CURSOR_SCREEN_CLEAR, nullptr, 0, requestAck); }
bool CursorCharacterScreenClient::setCursor(uint8_t x, uint8_t y, bool requestAck) const { const uint8_t data[] = {x, y}; return bus_.sendCommand(resolve(), command::CURSOR_SCREEN_SET_CURSOR, data, sizeof(data), requestAck); }
bool CursorCharacterScreenClient::show(const char *message, uint8_t size, bool requestAck) const { return bus_.sendCommand(resolve(), command::CURSOR_SCREEN_SHOW, message, size, requestAck); }
bool CursorCharacterScreenClient::requestRows() const { return bus_.getRegister(resolve(), reg::DISPLAY_ROWS); }
bool CursorCharacterScreenClient::requestColumns() const { return bus_.getRegister(resolve(), reg::DISPLAY_COLUMNS); }

PowerClient::PowerClient(Bus &bus, uint8_t instance) : ActuatorClient(bus, service::POWER, instance) {}
bool PowerClient::setAllowed(bool allowed, bool requestAck) const { const uint8_t value = allowed ? 1 : 0; return bus_.setRegister(resolve(), reg::INTENSITY, value, requestAck); }
bool PowerClient::setMaxPower(uint16_t milliamps, bool requestAck) const { return bus_.setRegister(resolve(), reg::MAX_POWER, milliamps, requestAck); }
bool PowerClient::requestCurrentDraw() const { return bus_.getRegister(resolve(), reg::READING); }
bool PowerClient::requestBatteryVoltage() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_VOLTAGE); }
bool PowerClient::requestPowerStatus() const { return bus_.getRegister(resolve(), reg::POWER_STATUS); }
bool PowerClient::requestBatteryCharge() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_CHARGE); }
bool PowerClient::requestBatteryCapacity() const { return bus_.getRegister(resolve(), reg::POWER_BATTERY_CAPACITY); }

} // namespace jacdac