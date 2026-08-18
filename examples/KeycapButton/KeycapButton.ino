#include <Jacdac.h>

using namespace jacdac;

ButtonClient keycap(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    const Service button = keycap.resolve();
    if (!button.valid() || packet.deviceIdentifier != button.deviceIdentifier || packet.serviceIndex != button.serviceIndex) {
        return;
    }
    if (packet.isEvent()) {
        if (packet.eventCode() == event::BUTTON_DOWN) {
            Serial.println("keycap down");
        } else if (packet.eventCode() == event::BUTTON_UP) {
            uint32_t heldMilliseconds;
            if (readValue(packet, heldMilliseconds)) {
                Serial.print("keycap up after ");
                Serial.print(heldMilliseconds);
                Serial.println(" ms");
            }
        } else if (packet.eventCode() == event::BUTTON_HOLD) {
            uint32_t heldMilliseconds;
            if (readValue(packet, heldMilliseconds)) {
                Serial.print("keycap hold at ");
                Serial.print(heldMilliseconds);
                Serial.println(" ms");
            }
        }
    } else if (packet.isRegisterGet()) {
        if (packet.registerCode() == reg::READING) {
            uint16_t pressure;
            if (readValue(packet, pressure)) {
                Serial.print("pressure: ");
                Serial.print(pressure * 100UL / 65535UL);
                Serial.println("%");
            }
        } else if (packet.registerCode() == reg::BUTTON_PRESSED && packet.dataSize >= 1) {
            Serial.print("pressed: ");
            Serial.println(packet.data[0] ? "yes" : "no");
        } else if (packet.registerCode() == reg::BUTTON_ANALOG && packet.dataSize >= 1) {
            Serial.print("analog: ");
            Serial.println(packet.data[0] ? "yes" : "no");
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
    if (keycap.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        keycap.requestPressure();
        keycap.requestPressed();
        keycap.requestAnalog();
        nextQuery = millis() + 250;
    }
}
