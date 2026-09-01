#include <Jacdac.h>

using namespace jacdac;

ServiceBinding buttonBinding(service::BUTTON);
uint32_t nextAction;
uint8_t action;

void registerReceived(const PacketView *packet, void *) {
    if (packet == nullptr) {
        Serial.println("register timeout");
        return;
    }
    uint8_t pressed;
    if (readValue(*packet, pressed)) {
        Serial.print("button pressed: ");
        Serial.println(pressed != 0 ? "yes" : "no");
    }
}

void packetReceived(const PacketView &packet, void *) {
    if (packet.isEvent()) {
        Serial.print("event: 0x");
        Serial.println(packet.eventCode(), HEX);
    }
}

void deviceEvent(const Device &device, DeviceEvent event, void *) {
    Serial.print("device event: ");
    Serial.print(static_cast<uint8_t>(event));
    Serial.print(" id=0x");
    Serial.println(static_cast<uint32_t>(device.deviceIdentifier), HEX);
}

void commandError(const Service &target, uint16_t command, uint16_t packetCrc, void *) {
    Serial.print("command rejected: service=");
    Serial.print(target.serviceIndex);
    Serial.print(" command=0x");
    Serial.print(command, HEX);
    Serial.print(" crc=0x");
    Serial.println(packetCrc, HEX);
}

void setup() {
    Serial.begin(115200);
    Jacdac.addDeviceHandler(deviceEvent);
    Jacdac.setCommandErrorHandler(commandError);
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();

    Service button = Jacdac.resolve(buttonBinding);
    if (!button.valid()) {
        action = 0;
        return;
    }
    if (!buttonBinding.bound()) {
        buttonBinding.bind(button);
    }

    uint32_t now = millis();
    if (static_cast<int32_t>(now - nextAction) < 0) {
        return;
    }
    nextAction = now + 100;

    bool queued = false;
    switch (action) {
    case 0:
        queued = Jacdac.getRegisterAsync(button, reg::BUTTON_PRESSED, registerReceived, nullptr, 1000);
        break;
    case 1:
        queued = Jacdac.requestDeviceDescription(button.deviceIdentifier);
        break;
    case 2:
        queued = Jacdac.requestProductIdentifier(button.deviceIdentifier);
        break;
    case 3:
        queued = Jacdac.requestFirmwareVersion(button.deviceIdentifier);
        break;
    case 4:
        queued = Jacdac.requestUptime(button.deviceIdentifier);
        break;
    case 5:
        queued = Jacdac.setStatusLight(button.deviceIdentifier, 0, 32, 0);
        break;
    case 6: {
        CommandBatch batch(button.deviceIdentifier);
        queued = batch.add(button, CMD_GET_REGISTER | reg::BUTTON_ANALOG) && batch.add(button, CMD_GET_REGISTER | reg::READING) && Jacdac.sendBatch(batch);
        break;
    }
    default:
        queued = Jacdac.sendMulticast(service::BUTTON, CMD_GET_REGISTER | reg::BUTTON_PRESSED);
        break;
    }
    if (queued) {
        action = static_cast<uint8_t>((action + 1) % 8);
    }
}
