#include <Jacdac.h>

using namespace jacdac;
AccelerometerClient accelerometer(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport() || !accelerometer.matchesReading(packet)) return;
    int32_t forces[3];
    if (readValue(packet, forces)) {
        Serial.print("XYZ g: ");
        for (uint8_t axis = 0; axis < 3; ++axis) {
            if (axis != 0) Serial.print(", ");
            Serial.print(forces[axis] / 1048576.0, 4);
        }
        Serial.println();
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
    if (accelerometer.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        accelerometer.requestForces();
        nextQuery = millis() + 250;
    }
}