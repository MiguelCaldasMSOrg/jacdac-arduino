#include <Jacdac.h>

using namespace jacdac;
DistanceClient distance(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport() || !distance.matchesReading(packet)) return;
    uint32_t value;
    if (readValue(packet, value)) {
        Serial.print(value / 65536.0, 4);
        Serial.println(" m");
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
    if (distance.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        distance.requestDistance();
        nextQuery = millis() + 500;
    }
}