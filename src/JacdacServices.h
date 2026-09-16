#pragma once

#include <stdint.h>

namespace jacdac {
namespace service {

constexpr uint32_t CONTROL = 0x00000000;
constexpr uint32_t ACCELEROMETER = 0x1f140409;
constexpr uint32_t BUTTON = 0x1473a263;
constexpr uint32_t DISTANCE = 0x141a6b8a;
constexpr uint32_t HUMIDITY = 0x16c810b8;
constexpr uint32_t LED = 0x1609d4f0;
constexpr uint32_t LED_STRIP = 0x126f00e0;
constexpr uint32_t LIGHT_LEVEL = 0x17dc9a1c;
constexpr uint32_t MAGNETIC_FIELD_LEVEL = 0x12fe180f;
constexpr uint32_t POTENTIOMETER = 0x1f274746;
constexpr uint32_t POWER = 0x1fa4c95a;
constexpr uint32_t RELAY = 0x183fe656;
constexpr uint32_t ROTARY_ENCODER = 0x10fa29c9;
constexpr uint32_t SERVO = 0x12fc9103;
constexpr uint32_t TEMPERATURE = 0x1421bac7;
constexpr uint32_t VIBRATION_MOTOR = 0x183fc4a2;

} // namespace service

namespace reg {

constexpr uint16_t INTENSITY = 0x001;
constexpr uint16_t VALUE = 0x002;
constexpr uint16_t IS_STREAMING = 0x003;
constexpr uint16_t STREAMING_INTERVAL = 0x004;
constexpr uint16_t LOW_THRESHOLD = 0x005;
constexpr uint16_t HIGH_THRESHOLD = 0x006;
constexpr uint16_t MAX_POWER = 0x007;
constexpr uint16_t READING_RANGE = 0x008;
constexpr uint16_t CLIENT_VARIANT = 0x009;
constexpr uint16_t READING = 0x101;
constexpr uint16_t STREAMING_PREFERRED_INTERVAL = 0x102;
constexpr uint16_t STATUS_CODE = 0x103;
constexpr uint16_t READING_ERROR = 0x106;
constexpr uint16_t MIN_READING = 0x104;
constexpr uint16_t MAX_READING = 0x105;
constexpr uint16_t VARIANT = 0x107;
constexpr uint16_t READING_RESOLUTION = 0x108;
constexpr uint16_t INSTANCE_NAME = 0x109;
constexpr uint16_t SUPPORTED_RANGES = 0x10a;
constexpr uint16_t MIN_VALUE = 0x110;
constexpr uint16_t MAX_VALUE = 0x111;
constexpr uint16_t DEVICE_DESCRIPTION = 0x180;
constexpr uint16_t PRODUCT_IDENTIFIER = 0x181;
constexpr uint16_t MCU_TEMPERATURE = 0x182;
constexpr uint16_t BOOTLOADER_PRODUCT_IDENTIFIER = 0x184;
constexpr uint16_t FIRMWARE_VERSION = 0x185;
constexpr uint16_t UPTIME = 0x186;
constexpr uint16_t BUTTON_PRESSED = 0x181;
constexpr uint16_t BUTTON_ANALOG = 0x180;
constexpr uint16_t LED_NUM_PIXELS = 0x182;
constexpr uint16_t LED_ACTUAL_BRIGHTNESS = 0x180;
constexpr uint16_t LED_STRIP_ACTUAL_BRIGHTNESS = 0x180;
constexpr uint16_t LED_STRIP_LIGHT_TYPE = 0x080;
constexpr uint16_t LED_STRIP_NUM_PIXELS = 0x081;
constexpr uint16_t LED_STRIP_NUM_REPEATS = 0x082;
constexpr uint16_t LED_STRIP_NUM_COLUMNS = 0x083;
constexpr uint16_t LED_STRIP_MAX_PIXELS = 0x181;
constexpr uint16_t ROTARY_CLICKS_PER_TURN = 0x180;
constexpr uint16_t ROTARY_CLICKER = 0x181;
constexpr uint16_t POWER_BATTERY_VOLTAGE = 0x180;
constexpr uint16_t POWER_STATUS = 0x181;
constexpr uint16_t POWER_BATTERY_CHARGE = 0x182;
constexpr uint16_t POWER_BATTERY_CAPACITY = 0x183;
constexpr uint16_t POWER_KEEP_ON_PULSE_DURATION = 0x080;
constexpr uint16_t POWER_KEEP_ON_PULSE_PERIOD = 0x081;
constexpr uint16_t VIBRATION_MOTOR_MAX_VIBRATIONS = 0x180;

} // namespace reg

namespace command {

constexpr uint16_t LED_STRIP_RUN = 0x081;
constexpr uint16_t CONTROL_IDENTIFY = 0x081;
constexpr uint16_t CONTROL_RESET = 0x082;
constexpr uint16_t CONTROL_SET_STATUS_LIGHT = 0x084;
constexpr uint16_t CONTROL_STANDBY = 0x087;
constexpr uint16_t POWER_SHUTDOWN = 0x080;
constexpr uint16_t VIBRATION_MOTOR_VIBRATE = 0x080;

} // namespace command

enum class LedStripLightType : uint8_t {
	Ws2812bGrb = 0x00,
	Apa102 = 0x10,
	Sk9822 = 0x11
};

enum class LedStripVariant : uint8_t {
	Strip = 0x01,
	Ring = 0x02,
	Stick = 0x03,
	Jewel = 0x04,
	Matrix = 0x05
};

enum class PotentiometerVariant : uint8_t {
	Slider = 0x01,
	Rotary = 0x02,
	Hall = 0x03
};

enum class PowerStatus : uint8_t {
	Disallowed = 0,
	Powering = 1,
	Overload = 2,
	Overprovision = 3
};

namespace event {

constexpr uint16_t ACTIVE = 0x01;
constexpr uint16_t INACTIVE = 0x02;
constexpr uint16_t VALUE_CHANGED = 0x03;
constexpr uint16_t BUTTON_DOWN = ACTIVE;
constexpr uint16_t BUTTON_UP = INACTIVE;
constexpr uint16_t BUTTON_HOLD = 0x81;

} // namespace event
} // namespace jacdac