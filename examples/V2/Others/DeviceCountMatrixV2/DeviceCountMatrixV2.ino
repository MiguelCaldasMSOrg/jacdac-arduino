#include <Jacdac.h>

using namespace jacdac;

#ifndef _MICROBIT_V2_
#error "This example requires a BBC micro:bit V2."
#endif

static const uint8_t MATRIX_ROWS[] = {21, 22, 23, 24, 25};
static const uint8_t MATRIX_COLUMNS[] = {4, 7, 3, 6, 10};

static const uint8_t DIGITS[][5] = {
    {0x0e, 0x11, 0x11, 0x11, 0x0e},
    {0x04, 0x0c, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x02, 0x04, 0x1f},
    {0x1e, 0x01, 0x06, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x1f, 0x02},
    {0x1f, 0x10, 0x1e, 0x01, 0x1e},
    {0x0e, 0x10, 0x1e, 0x11, 0x0e},
    {0x1f, 0x01, 0x02, 0x04, 0x04},
    {0x0e, 0x11, 0x0e, 0x11, 0x0e},
    {0x0e, 0x11, 0x0f, 0x01, 0x0e},
};

static uint8_t matrixRow = 4;

static void refreshMatrix(uint8_t value) {
    digitalWrite(MATRIX_ROWS[matrixRow], LOW);
    matrixRow = static_cast<uint8_t>((matrixRow + 1) % 5);

    const uint8_t pixels = DIGITS[value > 9 ? 9 : value][matrixRow];
    for (uint8_t column = 0; column < 5; ++column) {
        digitalWrite(MATRIX_COLUMNS[column], pixels & (0x10 >> column) ? LOW : HIGH);
    }

    digitalWrite(MATRIX_ROWS[matrixRow], HIGH);
    delayMicroseconds(1000);
}

void setup() {
    for (uint8_t row = 0; row < 5; ++row) {
        pinMode(MATRIX_ROWS[row], OUTPUT);
        digitalWrite(MATRIX_ROWS[row], LOW);
    }
    for (uint8_t column = 0; column < 5; ++column) {
        pinMode(MATRIX_COLUMNS[column], OUTPUT);
        digitalWrite(MATRIX_COLUMNS[column], HIGH);
    }

    Jacdac.begin(12);
}

void loop() {
    Jacdac.process();
    refreshMatrix(Jacdac.deviceCount());
}