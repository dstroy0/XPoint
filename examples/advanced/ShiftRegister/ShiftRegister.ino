// XPoint — shift-register example (4×4 matrix, two daisy-chained 74HC595)
// See examples/WIRING.md for wiring patterns and transistor driver circuit.

#include <XPoint.h>
#include <drivers/ArduinoShiftRegisterDriver.h>

// Node (r, c) → shift-register bit r*4 + c  (0–15 across two 74HC595 chips)
static uint16_t mapRowMajor(uint8_t r, uint8_t c)
{
    return (uint16_t)(r * 4 + c);
}

// 16 output bits, data=2, clock=3, latch=4
ArduinoShiftRegisterDriver sr(16, mapRowMajor, 2, 3, 4);

// XPointStatic stores all state inside the object — no heap allocation.
XPointStatic<4, 4> matrix;

void setup()
{
    Serial.begin(115200);

    sr.begin();
    matrix.setDriver(&sr);
    matrix.begin();

    matrix.lockRows(0, 1);    // row 0 and row 1 cannot share a column
    matrix.exclusiveInput(3); // column 3 accepts only one row at a time

    matrix.connect(0, 1); // connect row 0 → column 1
}

void loop()
{
    matrix.update(); // drives latching-relay coil release; harmless for non-latching
    delay(100);
}
