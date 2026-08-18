#include <Jacdac.h>

using namespace jacdac;

PotentiometerClient slider(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    const Service currentSlider = slider.resolve();
    if (!currentSlider.valid() || packet.deviceIdentifier != currentSlider.deviceIdentifier || packet.serviceIndex != currentSlider.serviceIndex || !packet.isRegisterGet()) {
        return;
    }
    if (packet.registerCode() == reg::READING) {
        uint16_t position;
        if (readValue(packet, position)) {
            Serial.print("position: ");
            Serial.print(position * 100UL / 65535UL);
            Serial.println("%");
        }
    } else if (packet.registerCode() == reg::VARIANT && packet.dataSize >= 1) {
        Serial.print("variant: ");
        Serial.println(packet.data[0] == static_cast<uint8_t>(PotentiometerVariant::Slider) ? "slider" : "other");
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
    if (slider.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        slider.requestPosition();
        slider.requestVariant();
        nextQuery = millis() + 250;
    }
}
