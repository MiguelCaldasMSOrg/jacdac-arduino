#include <Jacdac.h>

using namespace jacdac;
HumidityClient humidity(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport() || !humidity.matchesReading(packet)) return;
    uint32_t value;
    if (readValue(packet, value)) {
        Serial.print(value / 1024.0, 2);
        Serial.println(" %RH");
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
    if (humidity.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        humidity.requestHumidity();
        nextQuery = millis() + 5000;
    }
}