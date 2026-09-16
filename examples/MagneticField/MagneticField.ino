#include <Jacdac.h>

using namespace jacdac;
MagneticFieldLevelClient magnet(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport() || !magnet.matchesReading(packet)) return;
    int16_t strength;
    if (readValue(packet, strength)) {
        Serial.print("magnetic field: ");
        Serial.println(strength / 32768.0, 4);
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
    if (magnet.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        magnet.requestStrength();
        nextQuery = millis() + 250;
    }
}