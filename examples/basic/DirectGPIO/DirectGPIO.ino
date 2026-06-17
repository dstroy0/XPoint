// XPoint — direct GPIO example (4×4 matrix, one MCU pin per node)
// Node (r, c) maps to pin 2 + r*4 + c, so a 4×4 matrix uses pins 2–17.

#include <XPoint.h>
#include <drivers/ArduinoDirectGPIODriver.h>

static uint16_t mapper(uint8_t r, uint8_t c)
{
    return (uint16_t)(2 + r * 4 + c);
}

// rows=4, cols=4, mapper, maxPinIndex=17
ArduinoDirectGPIODriver driver(4, 4, mapper, 17);
XPointStatic<4, 4> matrix;

void setup()
{
    Serial.begin(115200);

    driver.begin(); // calls pinMode(OUTPUT) only on pins 2–17
    matrix.setDriver(&driver);
    matrix.begin();

    matrix.lockRows(0, 1); // row 0 and row 1 cannot share a column
    matrix.connect(0, 0);  // connect row 0 → column 0
}

void loop()
{
    matrix.update();
    delay(200);
}
