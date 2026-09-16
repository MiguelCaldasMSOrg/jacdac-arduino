#include <Jacdac.h>

using namespace jacdac;

LedClient ring(Jacdac);
static uint16_t numPixels;
static uint32_t nextUpdate;
static uint32_t stopAt;
static bool active;
static uint8_t colorIndex;

static void packetReceived(const PacketView &packet, void *) {
    const Service target = ring.resolve();
    if (packet.isReport() && packet.deviceIdentifier == target.deviceIdentifier && packet.serviceIndex == target.serviceIndex && packet.isRegisterGet() && packet.registerCode() == reg::LED_NUM_PIXELS) {
        readValue(packet, numPixels);
    }
}

void setup() {
    Serial.begin(115200);
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    if (Serial.available()) {
        const int command = Serial.read();
        if (command == 't' && ring.connected() && numPixels > 0 && numPixels <= 64 && ring.setBrightness(8)) {
            active = true;
            stopAt = now + 6000;
            nextUpdate = now;
            colorIndex = 0;
        } else if (command == 'x') {
            active = true;
            stopAt = now;
        }
    }
    if (active && static_cast<int32_t>(now - stopAt) >= 0) {
        if (ring.setBrightness(0, true)) active = false;
    }
    if (ring.connected() && static_cast<int32_t>(now - nextUpdate) >= 0) {
        if (!active) {
            ring.requestNumPixels();
        } else {
            static const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
            uint8_t pixels[64 * 3];
            for (uint16_t index = 0; index < numPixels; ++index) memcpy(pixels + index * 3, colors[colorIndex], 3);
            ring.setPixels(pixels, static_cast<uint8_t>(numPixels * 3));
            colorIndex = static_cast<uint8_t>((colorIndex + 1) % 3);
        }
        nextUpdate = now + 2000;
    }
}
