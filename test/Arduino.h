#ifndef ARDUINO_SHIM_H
#define ARDUINO_SHIM_H

#include <cstdint>

typedef uint8_t byte;

// Minimal Arduino API used by the library. Tests provide the implementations.
unsigned long millis();
void delay(unsigned long ms);

#endif // ARDUINO_SHIM_H
