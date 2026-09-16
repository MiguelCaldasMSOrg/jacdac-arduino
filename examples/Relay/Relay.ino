#include <Jacdac.h>

using namespace jacdac;
RelayClient relay(Jacdac);
static bool active;
static uint32_t stopAt;

static void packetReceived(const PacketView &packet, void *) {
    const Service target = relay.resolve();
    if (!packet.isReport() || packet.deviceIdentifier != target.deviceIdentifier || packet.serviceIndex != target.serviceIndex) return;
    if (packet.isRegisterGet() && packet.registerCode() == reg::INTENSITY && packet.dataSize == 1) Serial.println(packet.data[0] ? "relay on" : "relay off");
}

void setup() {
    Serial.begin(115200);
    Serial.println("Unloaded relay contacts only. Send t for two seconds on; x stops.");
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    if (Serial.available()) {
        const int command = Serial.read();
        if (command == 't' && relay.connected() && relay.setActive(true, true)) { active = true; stopAt = now + 2000; }
        if (command == 'x') { active = true; stopAt = now; }
    }
    if (active && static_cast<int32_t>(now - stopAt) >= 0 && relay.setActive(false, true)) active = false;
    static uint32_t nextRead;
    if (relay.connected() && static_cast<int32_t>(now - nextRead) >= 0) {
        relay.requestActive();
        nextRead = now + 500;
    }
}