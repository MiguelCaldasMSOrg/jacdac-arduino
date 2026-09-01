#include <Jacdac.h>

using namespace jacdac;

static void deviceChanged(const Device &device, DeviceEvent event, void *) {
    if (event != DeviceEvent::Connected && event != DeviceEvent::Disconnected) {
        return;
    }
    const bool connected = event == DeviceEvent::Connected;
    Serial.print(connected ? "connected " : "disconnected ");
    Serial.print(static_cast<uint32_t>(device.deviceIdentifier >> 32), HEX);
    Serial.print(static_cast<uint32_t>(device.deviceIdentifier), HEX);
    Serial.print(" services=");
    Serial.println(device.serviceCount);
    for (uint8_t index = 0; connected && index < device.serviceCount; ++index) {
        Serial.print("  [");
        Serial.print(index + 1);
        Serial.print("] 0x");
        Serial.println(device.serviceClasses[index], HEX);
    }
}

static void printDiagnostics() {
    const Diagnostics &diagnostics = Jacdac.diagnostics();
    Serial.print("rx edges=");
    Serial.print(diagnostics.fallingEdges);
    Serial.print(" starts=");
    Serial.print(diagnostics.receiveStarts);
    Serial.print(" complete=");
    Serial.print(diagnostics.receiveCompletions);
    Serial.print(" bytes=");
    Serial.print(diagnostics.receiveBytes);
    Serial.print(" short=");
    Serial.print(diagnostics.receiveShortFrames);
    Serial.print(" invalid=");
    Serial.print(diagnostics.receiveInvalidFrames);
    Serial.print(" timeout=");
    Serial.print(diagnostics.receiveTimeouts);
    Serial.print(" hwerr=");
    Serial.println(diagnostics.receiveHardwareErrors);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }
    Jacdac.addDeviceHandler(deviceChanged);
    if (!Jacdac.begin(12)) {
        Serial.println("Jacdac requires a supported BBC micro:bit target and a valid Jacdac pin.");
    }
}

void loop() {
    Jacdac.process();
    static uint32_t lastDiagnostics;
    const uint32_t now = millis();
    if (now - lastDiagnostics >= 1000) {
        lastDiagnostics = now;
        printDiagnostics();
    }
}