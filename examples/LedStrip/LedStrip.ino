#include <Jacdac.h>

using namespace jacdac;
LedStripClient strip(Jacdac);
static bool active;
static uint32_t stopAt;
static uint32_t nextColor;
static uint8_t colorIndex;
static uint16_t numPixels;

static void packetReceived(const PacketView &packet, void *) {
    const Service target = strip.resolve();
    if (packet.isReport() && packet.isRegisterGet() && packet.deviceIdentifier == target.deviceIdentifier && packet.serviceIndex == target.serviceIndex && packet.registerCode() == reg::LED_STRIP_NUM_PIXELS) readValue(packet, numPixels);
}

void setup() {
    Serial.begin(115200);
    Serial.println("t: six-second low-brightness RGB test; x: off. Requires configured strip pixels.");
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    if (Serial.available()) {
        const int command = Serial.read();
        if (command == 't' && strip.connected() && numPixels != 0 && strip.setBrightness(8)) { active = true; stopAt = now + 6000; nextColor = now; colorIndex = 0; }
        if (command == 'x') { active = true; stopAt = now; }
    }
    if (active && static_cast<int32_t>(now - stopAt) >= 0 && strip.setBrightness(0, true)) active = false;
    if (strip.connected() && static_cast<int32_t>(now - nextColor) >= 0) {
        if (active) {
            static const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
            strip.setAll(colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2]);
            colorIndex = static_cast<uint8_t>((colorIndex + 1) % 3);
        } else strip.requestNumPixels();
        nextColor = now + 2000;
    }
}