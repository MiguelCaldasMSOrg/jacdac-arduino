#include <Jacdac.h>

using namespace jacdac;

LedStripClient ring(Jacdac);
RotaryEncoderClient rotary(Jacdac);
PotentiometerClient slider(Jacdac);

static int32_t rotaryPosition;
static uint16_t sliderPosition = 16384;
static bool rotaryPressed;
static bool keycapPressed;
static bool outputDirty = true;

static Service findKeycap() {
    const Service rotaryButton = rotary.buttonService();
    for (uint8_t instance = 0; instance < MAX_DEVICES; ++instance) {
        const Service candidate = Jacdac.findService(service::BUTTON, instance);
        if (!candidate.valid()) {
            break;
        }
        if (!rotaryButton.valid() || candidate.deviceIdentifier != rotaryButton.deviceIdentifier || candidate.serviceIndex != rotaryButton.serviceIndex) {
            return candidate;
        }
    }
    return {0, service::BUTTON, 0};
}

static void updateButton(const PacketView &packet, const Service &button, bool &pressed) {
    if (!button.valid() || packet.deviceIdentifier != button.deviceIdentifier || packet.serviceIndex != button.serviceIndex) {
        return;
    }
    if (packet.isEvent() && packet.eventCode() == event::BUTTON_DOWN) {
        pressed = true;
        outputDirty = true;
    } else if (packet.isEvent() && packet.eventCode() == event::BUTTON_UP) {
        pressed = false;
        outputDirty = true;
    } else if (packet.isRegisterGet() && packet.registerCode() == reg::BUTTON_PRESSED && packet.dataSize >= 1) {
        const bool nextPressed = packet.data[0] != 0;
        if (pressed != nextPressed) {
            pressed = nextPressed;
            outputDirty = true;
        }
    }
}

static void packetReceived(const PacketView &packet, void *) {
    const Service currentRotary = rotary.resolve();
    if (currentRotary.valid() && packet.deviceIdentifier == currentRotary.deviceIdentifier && packet.serviceIndex == currentRotary.serviceIndex && packet.isRegisterGet() && packet.registerCode() == reg::READING) {
        int32_t nextPosition;
        if (readValue(packet, nextPosition) && rotaryPosition != nextPosition) {
            rotaryPosition = nextPosition;
            outputDirty = true;
        }
    }
    const Service currentSlider = slider.resolve();
    if (currentSlider.valid() && packet.deviceIdentifier == currentSlider.deviceIdentifier && packet.serviceIndex == currentSlider.serviceIndex && packet.isRegisterGet() && packet.registerCode() == reg::READING) {
        uint16_t nextPosition;
        if (readValue(packet, nextPosition) && sliderPosition != nextPosition) {
            sliderPosition = nextPosition;
            outputDirty = true;
        }
    }
    updateButton(packet, rotary.buttonService(), rotaryPressed);
    updateButton(packet, findKeycap(), keycapPressed);
}

static void updateRing() {
    if (!ring.connected() || !outputDirty) {
        return;
    }
    bool brightnessQueued = ring.setBrightness(static_cast<uint8_t>(sliderPosition >> 8));
    bool colorQueued;
    if (rotaryPressed) {
        colorQueued = ring.setAll(0, 0, 0);
    } else if (keycapPressed) {
        colorQueued = ring.setAll(255, 255, 255);
    } else {
        static const uint8_t colors[][3] = {{255, 0, 0}, {255, 64, 0}, {255, 255, 0}, {0, 255, 0}, {0, 0, 255}, {128, 0, 255}};
        int32_t colorIndex = rotaryPosition % 6;
        if (colorIndex < 0) {
            colorIndex += 6;
        }
        colorQueued = ring.setAll(colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2]);
    }
    outputDirty = !brightnessQueued || !colorQueued;
}

void setup() {
    Jacdac.addPacketHandler(packetReceived);
    Jacdac.begin();
}

void loop() {
    Jacdac.process();
    static uint32_t nextQuery;
    static uint8_t queryPhase;
    if (static_cast<int32_t>(millis() - nextQuery) >= 0) {
        if (queryPhase == 0) {
            rotary.requestPosition();
        } else if (queryPhase == 1) {
            slider.requestPosition();
        } else if (queryPhase == 2) {
            const Service rotaryButton = rotary.buttonService();
            if (rotaryButton.valid()) {
                Jacdac.getRegister(rotaryButton, reg::BUTTON_PRESSED);
            }
        } else {
            const Service keycap = findKeycap();
            if (keycap.valid()) {
                Jacdac.getRegister(keycap, reg::BUTTON_PRESSED);
            }
        }
        queryPhase = (queryPhase + 1) & 3;
        nextQuery = millis() + 50;
    }
    updateRing();
}
