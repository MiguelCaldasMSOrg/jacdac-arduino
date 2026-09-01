#include <Jacdac.h>

using namespace jacdac;

LedClient led(Jacdac);
ServoClient servo(Jacdac);

void setup() {
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    static uint32_t nextUpdate;
    static bool alternate;
    if (static_cast<int32_t>(millis() - nextUpdate) >= 0) {
        if (led.connected()) {
            const uint8_t pixels[] = {static_cast<uint8_t>(alternate ? 0 : 255), 0, static_cast<uint8_t>(alternate ? 255 : 0)};
            led.setBrightness(64);
            led.setPixels(pixels, sizeof(pixels));
        }
        if (servo.connected()) {
            servo.setEnabled(true);
            servo.setAngle(alternate ? 135.0f : 45.0f);
        }
        alternate = !alternate;
        nextUpdate = millis() + 1000;
    }
}