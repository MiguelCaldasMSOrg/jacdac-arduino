#pragma once

#include <stdint.h>

namespace jacdac {
namespace service {

constexpr uint32_t CONTROL = 0x00000000;
constexpr uint32_t ACCELEROMETER = 0x1f140409;
constexpr uint32_t ACIDITY = 0x1e9778c5;
constexpr uint32_t AIR_PRESSURE = 0x1e117cea;
constexpr uint32_t AIR_QUALITY_INDEX = 0x14ac6ed6;
constexpr uint32_t ARCADE_GAMEPAD = 0x1deaa06e;
constexpr uint32_t ARCADE_SOUND = 0x1fc63606;
constexpr uint32_t BARCODE_READER = 0x1c739e6c;
constexpr uint32_t BIT_RADIO = 0x1ac986cf;
constexpr uint32_t BOOTLOADER = 0x1ffa9948;
constexpr uint32_t BRAILLE_DISPLAY = 0x13bfb7cc;
constexpr uint32_t BRIDGE = 0x1fe5b46f;
constexpr uint32_t BUTTON = 0x1473a263;
constexpr uint32_t BUZZER = 0x1b57b1d7;
constexpr uint32_t CAPACITIVE_BUTTON = 0x2865adc9;
constexpr uint32_t CHARACTER_SCREEN = 0x1f37c56a;
constexpr uint32_t CLOUD_ADAPTER = 0x14606e9c;
constexpr uint32_t CLOUD_CONFIGURATION = 0x1462eefc;
constexpr uint32_t CODAL_MESSAGE_BUS = 0x121ff81d;
constexpr uint32_t COLOR = 0x1630d567;
constexpr uint32_t COMPASS = 0x15b7b9bf;
constexpr uint32_t CURSOR_CHARACTER_SCREEN = 0x195ee163;
constexpr uint32_t DASHBOARD = 0x1be59107;
constexpr uint32_t DC_CURRENT_MEASUREMENT = 0x1912c8ae;
constexpr uint32_t DC_VOLTAGE_MEASUREMENT = 0x1633ac19;
constexpr uint32_t DEVICESCRIPT_CONDITION = 0x1196796d;
constexpr uint32_t DEVICESCRIPT_DEBUGGER = 0x155b5b40;
constexpr uint32_t DEVICESCRIPT_MANAGER = 0x1134ea2b;
constexpr uint32_t DISTANCE = 0x141a6b8a;
constexpr uint32_t DMX = 0x11cf8c05;
constexpr uint32_t DOT_MATRIX = 0x110d154b;
constexpr uint32_t DUAL_MOTORS = 0x1529d537;
constexpr uint32_t ECO2 = 0x169c9dc6;
constexpr uint32_t ELECTRICAL_CONDUCTIVITY = 0x1f1f7277;
constexpr uint32_t FLEX = 0x1f47c6c6;
constexpr uint32_t GAMEPAD = 0x108f7456;
constexpr uint32_t GPIO = 0x10d85a69;
constexpr uint32_t GYROSCOPE = 0x1e1b06f2;
constexpr uint32_t HEART_RATE = 0x166c6dc4;
constexpr uint32_t HID_JOYSTICK = 0x1a112155;
constexpr uint32_t HID_KEYBOARD = 0x18b05b6a;
constexpr uint32_t HID_MOUSE = 0x1885dc1c;
constexpr uint32_t HUMIDITY = 0x16c810b8;
constexpr uint32_t I2C = 0x1c18ca43;
constexpr uint32_t ILLUMINANCE = 0x1e6ecaf2;
constexpr uint32_t INDEXED_SCREEN = 0x16fa36e5;
constexpr uint32_t INFRASTRUCTURE = 0x1e1589eb;
constexpr uint32_t KEYBOARD_CLIENT = 0x113d023e;
constexpr uint32_t LED = 0x1609d4f0;
constexpr uint32_t LED_SINGLE = 0x1e3048f8;
constexpr uint32_t LED_STRIP = 0x126f00e0;
constexpr uint32_t LIGHT_BULB = 0x1cab054c;
constexpr uint32_t LIGHT_LEVEL = 0x17dc9a1c;
constexpr uint32_t LOGGER = 0x12dc1fca;
constexpr uint32_t MAGNETIC_FIELD_LEVEL = 0x12fe180f;
constexpr uint32_t MAGNETOMETER = 0x13029088;
constexpr uint32_t MATRIX_KEYPAD = 0x13062dc8;
constexpr uint32_t MICROPHONE = 0x113dac86;
constexpr uint32_t MIDI_OUTPUT = 0x1a848cd7;
constexpr uint32_t MODEL_RUNNER = 0x140f9a78;
constexpr uint32_t MOTION = 0x1179a749;
constexpr uint32_t MOTOR = 0x17004cd8;
constexpr uint32_t MULTITOUCH = 0x1d112ab5;
constexpr uint32_t PC_CONTROLLER = 0x113d0987;
constexpr uint32_t PC_MONITOR = 0x18627b15;
constexpr uint32_t PLANAR_POSITION = 0x1dc37f55;
constexpr uint32_t POTENTIOMETER = 0x1f274746;
constexpr uint32_t POWER = 0x1fa4c95a;
constexpr uint32_t POWER_SUPPLY = 0x1f40375f;
constexpr uint32_t PRESSURE_BUTTON = 0x281740c3;
constexpr uint32_t PROTO_TEST = 0x16c7466a;
constexpr uint32_t PROXY = 0x16f19949;
constexpr uint32_t PULSE_OXIMETER = 0x10bb4eb6;
constexpr uint32_t RAIN_GAUGE = 0x13734c95;
constexpr uint32_t REAL_TIME_CLOCK = 0x1a8b1a28;
constexpr uint32_t REFLECTED_LIGHT = 0x126c4cb2;
constexpr uint32_t RELAY = 0x183fe656;
constexpr uint32_t RNG = 0x1789f0a2;
constexpr uint32_t ROLE_MANAGER = 0x1e4b7e66;
constexpr uint32_t ROS = 0x1524f42c;
constexpr uint32_t ROTARY_ENCODER = 0x10fa29c9;
constexpr uint32_t ROVER = 0x19f4d06b;
constexpr uint32_t RPM = 0x19f8e291;
constexpr uint32_t SATELLITE_NAVIGATION_SYSTEM = 0x19dd6136;
constexpr uint32_t SENSOR_AGGREGATOR = 0x1d90e1c5;
constexpr uint32_t SERIAL_SERVICE = 0x11bae5c4;
constexpr uint32_t SERVO = 0x12fc9103;
constexpr uint32_t SETTINGS = 0x1107dc4a;
constexpr uint32_t SEVEN_SEGMENT_DISPLAY = 0x196158f7;
constexpr uint32_t SOIL_MOISTURE = 0x1d4aa3b3;
constexpr uint32_t SOLENOID = 0x171723ca;
constexpr uint32_t SOUND_LEVEL = 0x14ad1a5d;
constexpr uint32_t SOUND_PLAYER = 0x1403d338;
constexpr uint32_t SOUND_RECORDER_WITH_PLAYBACK = 0x1b72bf50;
constexpr uint32_t SOUND_SPECTRUM = 0x157abc1e;
constexpr uint32_t SPEECH_SYNTHESIS = 0x1204d995;
constexpr uint32_t SWITCH = 0x1ad29402;
constexpr uint32_t TCP = 0x1b43b70b;
constexpr uint32_t TEMPERATURE = 0x1421bac7;
constexpr uint32_t TIME_SERIES_AGGREGATOR = 0x1192bdcc;
constexpr uint32_t TRAFFIC_LIGHT = 0x15c38d9b;
constexpr uint32_t TVOC = 0x12a5b597;
constexpr uint32_t UNIQUE_BRAIN = 0x103c4ee5;
constexpr uint32_t USB_BRIDGE = 0x18f61a4a;
constexpr uint32_t UV_INDEX = 0x1f6e0d90;
constexpr uint32_t VERIFIED_TELEMETRY_SENSOR = 0x2194841f;
constexpr uint32_t VIBRATION_MOTOR = 0x183fc4a2;
constexpr uint32_t WATER_LEVEL = 0x147b62ed;
constexpr uint32_t WEIGHT_SCALE = 0x1f4d5040;
constexpr uint32_t WIFI = 0x18aae1fa;
constexpr uint32_t WIND_DIRECTION = 0x186be92b;
constexpr uint32_t WIND_SPEED = 0x1b591bbf;
constexpr uint32_t WSSK = 0x13b739fe;

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
constexpr uint16_t CHARACTER_SCREEN_TEXT_DIRECTION = 0x082;
constexpr uint16_t DISPLAY_ROWS = 0x180;
constexpr uint16_t DISPLAY_COLUMNS = 0x181;
constexpr uint16_t HID_JOYSTICK_BUTTON_COUNT = 0x180;
constexpr uint16_t HID_JOYSTICK_BUTTONS_ANALOG = 0x181;
constexpr uint16_t HID_JOYSTICK_AXIS_COUNT = 0x182;
constexpr uint16_t POWER_BATTERY_VOLTAGE = 0x180;
constexpr uint16_t POWER_STATUS = 0x181;
constexpr uint16_t POWER_BATTERY_CHARGE = 0x182;
constexpr uint16_t POWER_BATTERY_CAPACITY = 0x183;
constexpr uint16_t VIBRATION_MOTOR_MAX_VIBRATIONS = 0x180;

} // namespace reg

namespace command {

constexpr uint16_t LED_STRIP_RUN = 0x081;
constexpr uint16_t CONTROL_IDENTIFY = 0x081;
constexpr uint16_t CONTROL_RESET = 0x082;
constexpr uint16_t CONTROL_SET_STATUS_LIGHT = 0x084;
constexpr uint16_t CONTROL_STANDBY = 0x087;
constexpr uint16_t BUZZER_PLAY_TONE = 0x080;
constexpr uint16_t BUZZER_PLAY_NOTE = 0x081;
constexpr uint16_t CURSOR_SCREEN_HOME = 0x083;
constexpr uint16_t CURSOR_SCREEN_CLEAR = 0x084;
constexpr uint16_t CURSOR_SCREEN_SET_CURSOR = 0x085;
constexpr uint16_t CURSOR_SCREEN_SHOW = 0x086;
constexpr uint16_t HID_KEYBOARD_KEY = 0x080;
constexpr uint16_t HID_KEYBOARD_CLEAR = 0x081;
constexpr uint16_t HID_MOUSE_SET_BUTTON = 0x080;
constexpr uint16_t HID_MOUSE_MOVE = 0x081;
constexpr uint16_t HID_MOUSE_WHEEL = 0x082;
constexpr uint16_t HID_JOYSTICK_SET_BUTTONS = 0x080;
constexpr uint16_t HID_JOYSTICK_SET_AXIS = 0x081;
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

namespace event {

constexpr uint16_t ACTIVE = 0x01;
constexpr uint16_t INACTIVE = 0x02;
constexpr uint16_t VALUE_CHANGED = 0x03;
constexpr uint16_t BUTTON_DOWN = ACTIVE;
constexpr uint16_t BUTTON_UP = INACTIVE;
constexpr uint16_t BUTTON_HOLD = 0x81;

} // namespace event
} // namespace jacdac