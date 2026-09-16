#include <Jacdac.h>

using namespace jacdac;
VibrationMotorClient haptic(Jacdac);
static bool stopping;
static uint32_t stopAt;

static void acknowledged(uint64_t identifier, uint16_t, bool received, void *) {
    if (identifier == haptic.resolve().deviceIdentifier) Serial.println(received ? "haptic command acknowledged" : "haptic ACK timeout");
}

void setup() {
    Serial.begin(115200);
    Serial.println("t: 200 ms vibration at half intensity; x: stop.");
    Jacdac.addAckHandler(acknowledged);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    if (Serial.available()) {
        const int command = Serial.read();
        const VibrationStep pulse[] = {{25, 128}};
        if (command == 't' && haptic.connected() && haptic.vibrate(pulse, 1, true)) { stopping = true; stopAt = now + 1000; }
        if (command == 'x') { stopping = true; stopAt = now; }
    }
    if (stopping && static_cast<int32_t>(now - stopAt) >= 0 && haptic.stop(true)) stopping = false;
}