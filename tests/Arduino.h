#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t millis();
uint32_t micros();
void delayMicroseconds(unsigned int microseconds);
void noInterrupts();
void interrupts();
void jacdacTestSetMillis(uint32_t milliseconds);
