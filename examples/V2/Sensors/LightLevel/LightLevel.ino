#include <Jacdac.h>

using namespace jacdac;
LightLevelClient light(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport() || !light.matchesReading(packet)) return;
    uint16_t value;
    if (readValue(packet, value)) {
        Serial.print(value * 100.0 / 65535.0, 1);
        Serial.println(" % light");
    }
}

void setup() {
    Serial.begin(115200);
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    static uint32_t nextQuery;
    if (light.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        light.requestLightLevel();
        nextQuery = millis() + 500;
    }
}