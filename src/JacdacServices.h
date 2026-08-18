#pragma once

#include <stdint.h>

namespace jacdac {
namespace service {

constexpr uint32_t CONTROL = 0x00000000;
constexpr uint32_t ACCELEROMETER = 0x1f140409;
constexpr uint32_t AIR_PRESSURE = 0x1e117cea;
constexpr uint32_t BUTTON = 0x1473a263;
constexpr uint32_t BUZZER = 0x1b57b1d7;
constexpr uint32_t CAPACITIVE_BUTTON = 0x2865adc9;
constexpr uint32_t COLOR = 0x1630d567;
constexpr uint32_t COMPASS = 0x15b7b9bf;
constexpr uint32_t DISTANCE = 0x141a6b8a;
constexpr uint32_t DUAL_MOTORS = 0x1529d537;
constexpr uint32_t ECO2 = 0x169c9dc6;
constexpr uint32_t FLEX = 0x1f47c6c6;
constexpr uint32_t GYROSCOPE = 0x1e1b06f2;
constexpr uint32_t HUMIDITY = 0x16c810b8;
constexpr uint32_t ILLUMINANCE = 0x1e6ecaf2;
constexpr uint32_t LED = 0x1609d4f0;
constexpr uint32_t LED_SINGLE = 0x1e3048f8;
constexpr uint32_t LED_STRIP = 0x126f00e0;
constexpr uint32_t LIGHT_LEVEL = 0x17dc9a1c;
constexpr uint32_t MAGNETIC_FIELD_LEVEL = 0x12fe180f;
constexpr uint32_t MICROPHONE = 0x113dac86;
constexpr uint32_t MOTION = 0x1179a749;
constexpr uint32_t POTENTIOMETER = 0x1f274746;
constexpr uint32_t PRESSURE_BUTTON = 0x281740c3;
constexpr uint32_t REFLECTED_LIGHT = 0x126c4cb2;
constexpr uint32_t RELAY = 0x183fe656;
constexpr uint32_t ROTARY_ENCODER = 0x10fa29c9;
constexpr uint32_t SERVO = 0x12fc9103;
constexpr uint32_t SOIL_MOISTURE = 0x1d4aa3b3;
constexpr uint32_t SOUND_LEVEL = 0x14ad1a5d;
constexpr uint32_t SWITCH = 0x1ad29402;
constexpr uint32_t TEMPERATURE = 0x1421bac7;
constexpr uint32_t TRAFFIC_LIGHT = 0x15c38d9b;
constexpr uint32_t TVOC = 0x12a5b597;
constexpr uint32_t UV_INDEX = 0x1f6e0d90;
constexpr uint32_t VIBRATION_MOTOR = 0x183fc4a2;
constexpr uint32_t WATER_LEVEL = 0x147b62ed;
constexpr uint32_t WEIGHT_SCALE = 0x1f4d5040;
constexpr uint32_t WIND_DIRECTION = 0x186be92b;
constexpr uint32_t WIND_SPEED = 0x1b591bbf;

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
constexpr uint16_t DEVICE_DESCRIPTION = 0x180;
constexpr uint16_t PRODUCT_IDENTIFIER = 0x181;
constexpr uint16_t MCU_TEMPERATURE = 0x182;
constexpr uint16_t BOOTLOADER_PRODUCT_IDENTIFIER = 0x184;
constexpr uint16_t FIRMWARE_VERSION = 0x185;
constexpr uint16_t UPTIME = 0x186;
constexpr uint16_t BUTTON_PRESSED = 0x181;
constexpr uint16_t BUTTON_ANALOG = 0x180;
constexpr uint16_t LED_NUM_PIXELS = 0x182;
constexpr uint16_t LED_STRIP_ACTUAL_BRIGHTNESS = 0x180;
constexpr uint16_t LED_STRIP_LIGHT_TYPE = 0x080;
constexpr uint16_t LED_STRIP_NUM_PIXELS = 0x081;
constexpr uint16_t LED_STRIP_NUM_REPEATS = 0x082;
constexpr uint16_t LED_STRIP_NUM_COLUMNS = 0x083;
constexpr uint16_t LED_STRIP_MAX_PIXELS = 0x181;
constexpr uint16_t ROTARY_CLICKS_PER_TURN = 0x180;
constexpr uint16_t ROTARY_CLICKER = 0x181;

} // namespace reg

namespace command {

constexpr uint16_t LED_STRIP_RUN = 0x081;
constexpr uint16_t CONTROL_IDENTIFY = 0x081;
constexpr uint16_t CONTROL_RESET = 0x082;
constexpr uint16_t CONTROL_SET_STATUS_LIGHT = 0x084;
constexpr uint16_t CONTROL_STANDBY = 0x087;

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

namespace event {

constexpr uint16_t ACTIVE = 0x01;
constexpr uint16_t INACTIVE = 0x02;
constexpr uint16_t VALUE_CHANGED = 0x03;
constexpr uint16_t BUTTON_DOWN = ACTIVE;
constexpr uint16_t BUTTON_UP = INACTIVE;
constexpr uint16_t BUTTON_HOLD = 0x81;

} // namespace event
} // namespace jacdac