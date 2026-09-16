#include <Jacdac.h>

using namespace jacdac;
PowerClient channels[] = {PowerClient(Jacdac, 0), PowerClient(Jacdac, 1)};
static const uint16_t registers[] = {reg::POWER_STATUS, reg::INTENSITY, reg::MAX_POWER, reg::READING, reg::POWER_BATTERY_VOLTAGE, reg::POWER_BATTERY_CHARGE, reg::POWER_BATTERY_CAPACITY, reg::POWER_KEEP_ON_PULSE_DURATION, reg::POWER_KEEP_ON_PULSE_PERIOD};
static const char *labels[] = {"status", "allowed", "limit mA", "current mA", "input mV", "charge u0.16", "capacity mWh", "keep-on duration ms", "keep-on period ms"};
static bool unsupported[2][9];

static void packetReceived(const PacketView &packet, void *) {
    if (!packet.isReport()) return;
    for (uint8_t channel = 0; channel < 2; ++channel) {
        const Service target = channels[channel].resolve();
        if (!target.valid() || packet.deviceIdentifier != target.deviceIdentifier || packet.serviceIndex != target.serviceIndex) continue;
        const bool rejected = packet.serviceCommand == CMD_COMMAND_NOT_IMPLEMENTED && packet.dataSize >= 4;
        const uint16_t code = rejected ? static_cast<uint16_t>((packet.data[0] | (packet.data[1] << 8)) & REGISTER_CODE_MASK) : packet.registerCode();
        if (!rejected && !packet.isRegisterGet()) continue;
        for (uint8_t index = 0; index < 9; ++index) {
            if (registers[index] != code) continue;
            Serial.print("power "); Serial.print(channel + 1); Serial.print(' '); Serial.print(labels[index]); Serial.print(": ");
            if (rejected) {
                unsupported[channel][index] = true;
                Serial.println("unsupported");
            } else if (code == reg::INTENSITY || code == reg::POWER_STATUS) {
                uint8_t value;
                if (readValue(packet, value)) Serial.println(value);
            } else if (code == reg::POWER_BATTERY_CAPACITY) {
                uint32_t value;
                if (readValue(packet, value)) Serial.println(value);
            } else {
                uint16_t value;
                if (readValue(packet, value)) Serial.println(value);
            }
        }
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
    static uint8_t channel;
    static uint8_t index;
    if (static_cast<int32_t>(millis() - nextQuery) < 0) return;
    if (channels[channel].connected() && !unsupported[channel][index]) Jacdac.getRegister(channels[channel].resolve(), registers[index]);
    if (++index == 9) { index = 0; channel ^= 1; }
    nextQuery = millis() + 250;
}