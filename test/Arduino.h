#ifndef ARDUINO_SHIM_H
#define ARDUINO_SHIM_H

#include <cstdint>

typedef uint8_t byte;

// Minimal Arduino API used by the library. Tests provide the implementations.
// millis() returns uint32_t to match the 32-bit timer on all Arduino targets.
uint32_t millis();
void delay(uint32_t ms);

#endif // ARDUINO_SHIM_H
