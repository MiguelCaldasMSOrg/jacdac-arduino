#include "Arduino.h"

static uint32_t currentMilliseconds;

uint32_t millis() { return currentMilliseconds; }
uint32_t micros() { return currentMilliseconds * 1000; }
void delayMicroseconds(unsigned int) {}
void noInterrupts() {}
void interrupts() {}
void jacdacTestSetMillis(uint32_t milliseconds) { currentMilliseconds = milliseconds; }
