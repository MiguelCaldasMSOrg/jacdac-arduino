#include <Jacdac.h>

using namespace jacdac;

ServiceBinding buttonBinding(service::BUTTON);
volatile bool responseReceived;
uint32_t nextRequest;

void registerReceived(const PacketView *packet, void *) {
    responseReceived = packet != nullptr;
}

void packetReceived(const PacketView &, void *) {
    digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();

    Service button = Jacdac.resolve(buttonBinding);
    if (!button.valid()) {
        buttonBinding.clear();
        responseReceived = false;
        digitalWrite(LED_BUILTIN, LOW);
        return;
    }
    if (!buttonBinding.bound()) {
        buttonBinding.bind(button);
    }

    uint32_t now = millis();
    if (static_cast<int32_t>(now - nextRequest) >= 0) {
        nextRequest = now + 1000;
        responseReceived = false;
        Jacdac.getRegisterAsync(button, reg::BUTTON_PRESSED, registerReceived, nullptr, 500);
    }
    digitalWrite(LED_BUILTIN, responseReceived ? HIGH : LOW);
}
