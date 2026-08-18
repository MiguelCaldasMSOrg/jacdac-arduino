#include <Jacdac.h>

using namespace jacdac;

RotaryEncoderClient rotary(Jacdac);

static void packetReceived(const PacketView &packet, void *) {
    const Service encoder = rotary.resolve();
    const Service button = rotary.buttonService();
    if (encoder.valid() && packet.deviceIdentifier == encoder.deviceIdentifier && packet.serviceIndex == encoder.serviceIndex && packet.isRegisterGet()) {
        if (packet.registerCode() == reg::READING) {
            int32_t position;
            if (readValue(packet, position)) {
                Serial.print("position: ");
                Serial.println(position);
            }
        } else if (packet.registerCode() == reg::ROTARY_CLICKS_PER_TURN) {
            uint16_t clicks;
            if (readValue(packet, clicks)) {
                Serial.print("clicks per turn: ");
                Serial.println(clicks);
            }
        } else if (packet.registerCode() == reg::ROTARY_CLICKER && packet.dataSize >= 1) {
            Serial.print("clicker: ");
            Serial.println(packet.data[0] ? "enabled" : "disabled");
        }
    }
    if (button.valid() && packet.deviceIdentifier == button.deviceIdentifier && packet.serviceIndex == button.serviceIndex && packet.isEvent()) {
        if (packet.eventCode() == event::BUTTON_DOWN) {
            Serial.println("rotary button down");
        } else if (packet.eventCode() == event::BUTTON_UP) {
            uint32_t heldMilliseconds;
            if (readValue(packet, heldMilliseconds)) {
                Serial.print("rotary button up after ");
                Serial.print(heldMilliseconds);
                Serial.println(" ms");
            }
        } else if (packet.eventCode() == event::BUTTON_HOLD) {
            uint32_t heldMilliseconds;
            if (readValue(packet, heldMilliseconds)) {
                Serial.print("rotary button hold at ");
                Serial.print(heldMilliseconds);
                Serial.println(" ms");
            }
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
    if (rotary.connected() && static_cast<int32_t>(millis() - nextQuery) >= 0) {
        rotary.requestPosition();
        rotary.requestClicksPerTurn();
        rotary.requestClicker();
        nextQuery = millis() + 500;
    }
}
