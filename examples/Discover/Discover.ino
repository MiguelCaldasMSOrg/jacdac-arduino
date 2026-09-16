#include <Jacdac.h>

using namespace jacdac;

static void printDevice(const Device &device) {
    Serial.print(static_cast<uint32_t>(device.deviceIdentifier >> 32), HEX);
    Serial.print(static_cast<uint32_t>(device.deviceIdentifier), HEX);
    Serial.print(" services=");
    Serial.println(device.serviceCount);
    for (uint8_t index = 0; index < device.serviceCount; ++index) {
        Serial.print("  [");
        Serial.print(index + 1);
        Serial.print("] 0x");
        Serial.println(device.serviceClasses[index], HEX);
    }
}


static void deviceChanged(const Device &device, DeviceEvent event, void *) {
    if (event != DeviceEvent::Connected && event != DeviceEvent::Disconnected) {
        return;
    }
    Serial.print(event == DeviceEvent::Connected ? "connected " : "disconnected ");
    printDevice(device);
}

static void printDiagnostics() {
    const Diagnostics &diagnostics = Jacdac.diagnostics();
    Serial.print("devices=");
    Serial.print(Jacdac.deviceCount());
    Serial.print('/');
    Serial.print(MAX_DEVICES);
    Serial.print(" frames=");
    Serial.print(diagnostics.framesReceived);
    Serial.print("/");
    Serial.print(diagnostics.framesSent);
    Serial.print(" buserr=");
    Serial.print(diagnostics.busErrors);
    Serial.print(" collisions=");
    Serial.print(diagnostics.collisions);
    Serial.print(" rx edges=");
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
    Serial.print(diagnostics.receiveHardwareErrors);
    Serial.print(" device_overflow=");
    Serial.print(diagnostics.deviceOverflows);
    Serial.print(" rx_overflow=");
    Serial.print(diagnostics.receiveOverflows);
    Serial.print(" tx_overflow=");
    Serial.print(diagnostics.transmitOverflows);
    Serial.print(" malformed=");
    Serial.println(diagnostics.malformedPackets);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }
    pinMode(12, INPUT_PULLUP);
    Serial.print("P12 idle=");
    Serial.println(digitalRead(12) == HIGH ? "high" : "low");
    Jacdac.addDeviceHandler(deviceChanged);
    if (!Jacdac.begin(12)) {
        Serial.println("Jacdac requires a supported BBC micro:bit target and a valid Jacdac pin.");
    }
}

void loop() {
    Jacdac.process();
    static uint32_t lastDiagnostics;
    static uint32_t lastDeviceSummary;
    const uint32_t now = millis();
    if (now - lastDiagnostics >= 1000) {
        lastDiagnostics = now;
        printDiagnostics();
    }
    if (now - lastDeviceSummary >= 5000) {
        lastDeviceSummary = now;
        for (uint8_t index = 0; index < Jacdac.deviceCount(); ++index) {
            const Device *device = Jacdac.device(index);
            if (device != nullptr) {
                Serial.print("device ");
                printDevice(*device);
            }
        }
    }
}