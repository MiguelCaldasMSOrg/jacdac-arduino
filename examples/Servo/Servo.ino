#include <Jacdac.h>

using namespace jacdac;
ServoClient servo(Jacdac);
static int32_t minimumAngle;
static int32_t maximumAngle;
static bool haveMinimum;
static bool haveMaximum;
static bool active;
static uint32_t stopAt;

static void packetReceived(const PacketView &packet, void *) {
    const Service target = servo.resolve();
    if (!packet.isReport() || !packet.isRegisterGet() || packet.deviceIdentifier != target.deviceIdentifier || packet.serviceIndex != target.serviceIndex) return;
    if (packet.registerCode() == reg::MIN_VALUE) haveMinimum = readValue(packet, minimumAngle);
    if (packet.registerCode() == reg::MAX_VALUE) haveMaximum = readValue(packet, maximumAngle);
    if (packet.registerCode() == reg::VALUE) {
        int32_t angle;
        if (readValue(packet, angle)) { Serial.print("requested degrees: "); Serial.println(angle / 65536.0, 2); }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Unloaded motor only. t: midpoint for two seconds; x: disable. Continuous servos may rotate.");
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    const uint32_t now = millis();
    if (Serial.available()) {
        const int command = Serial.read();
        if (command == 't' && !active && servo.connected() && haveMinimum && haveMaximum && minimumAngle < maximumAngle) {
            const int32_t center = static_cast<int32_t>((static_cast<int64_t>(minimumAngle) + maximumAngle) / 2);
            if (servo.setEnabled(false) && servo.setAngleQ16(center) && servo.setEnabled(true, true)) { active = true; stopAt = now + 2000; }
        }
        if (command == 'x') { active = true; stopAt = now; }
    }
    if (active && static_cast<int32_t>(now - stopAt) >= 0 && servo.setEnabled(false, true)) active = false;
    static uint32_t nextRead;
    static uint8_t phase;
    if (servo.connected() && static_cast<int32_t>(now - nextRead) >= 0) {
        if (phase == 0) servo.requestMinAngle();
        else if (phase == 1) servo.requestMaxAngle();
        else servo.requestAngle();
        phase = static_cast<uint8_t>((phase + 1) % 3);
        nextRead = now + 500;
    }
}