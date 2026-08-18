#include <Jacdac.h>

using namespace jacdac;

static void deviceChanged(const Device &device, bool connected, void *) {
    Serial.print(connected ? "connected " : "disconnected ");
    Serial.print(static_cast<uint32_t>(device.identifier >> 32), HEX);
    Serial.print(static_cast<uint32_t>(device.identifier), HEX);
    Serial.print(" services=");
    Serial.println(device.serviceCount);
    for (uint8_t index = 0; connected && index < device.serviceCount; ++index) {
        Serial.print("  [");
        Serial.print(index + 1);
        Serial.print("] 0x");
        Serial.println(device.serviceClasses[index], HEX);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }
    Jacdac.onDevice(deviceChanged);
    if (!Jacdac.begin(12)) {
        Serial.println("Jacdac requires a supported BBC micro:bit target and a valid Jacdac pin.");
    }
}

void loop() {
    Jacdac.process();
}