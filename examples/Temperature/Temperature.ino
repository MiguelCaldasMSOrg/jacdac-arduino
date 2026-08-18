#include <Jacdac.h>

using namespace jacdac;

SensorClient thermometer(Jacdac, service::TEMPERATURE);

static void packetReceived(const PacketView &packet, void *) {
    const Service currentThermometer = thermometer.resolve();
    if (!currentThermometer.valid() || packet.deviceIdentifier != currentThermometer.deviceIdentifier || packet.serviceIndex != currentThermometer.serviceIndex) {
        return;
    }
    if (packet.isRegisterGet() && packet.registerCode() == reg::READING) {
        int32_t temperature;
        if (readValue(packet, temperature)) {
            Serial.print(q10ToFloat(temperature));
            Serial.println(" C");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Jacdac.onPacket(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    static uint32_t nextQuery;
    if (thermometer.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        thermometer.requestReading();
        nextQuery = millis() + 1000;
    }
}