#include <Jacdac.h>

using namespace jacdac;

LedStripClient ring(Jacdac);

void setup() {
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    static uint32_t nextUpdate;
    static uint8_t step;
    if (ring.connected() && static_cast<int32_t>(millis() - nextUpdate) >= 0) {
        if (step == 0) {
            ring.setBrightness(32);
            ring.requestNumPixels();
            ring.requestVariant();
        } else {
            static const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0}};
            const uint8_t *color = colors[(step - 1) % 5];
            ring.setAll(color[0], color[1], color[2]);
        }
        ++step;
        nextUpdate = millis() + 1000;
    }
}
