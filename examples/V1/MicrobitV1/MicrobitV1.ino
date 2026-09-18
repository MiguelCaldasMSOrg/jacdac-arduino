#include <Jacdac.h>

using namespace jacdac;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    digitalWrite(LED_BUILTIN, Jacdac.deviceCount() == 0 ? LOW : HIGH);
}
