#include <Jacdac.h>

using namespace jacdac;

ButtonClient button(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    const Service currentButton = button.resolve();
    if (!currentButton.valid() || packet.deviceIdentifier != currentButton.deviceIdentifier || packet.serviceIndex != currentButton.serviceIndex) {
        return;
    }
    if (packet.isEvent()) {
        if (packet.eventCode() == event::BUTTON_DOWN) {
            Serial.println("button down");
        } else if (packet.eventCode() == event::BUTTON_UP) {
            Serial.println("button up");
        } else if (packet.eventCode() == event::BUTTON_HOLD) {
            Serial.println("button hold");
        }
    } else if (packet.isRegisterGet() && packet.registerCode() == reg::BUTTON_PRESSED && packet.dataSize >= 1) {
        Serial.println(packet.data[0] ? "pressed" : "released");
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
    if (button.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        button.requestPressed();
        nextQuery = millis() + 500;
    }
}