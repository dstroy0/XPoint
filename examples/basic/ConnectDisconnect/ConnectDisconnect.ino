// XPoint — basic API walkthrough
//
// Connects and disconnects every node of a 2×2 matrix one at a time,
// printing each action to Serial.  Uses direct GPIO with pins 2–5.
//
// Wiring (one MCU output pin per relay driver input):
//   (0,0) → pin 2    (0,1) → pin 3
//   (1,0) → pin 4    (1,1) → pin 5

#include <XPoint.h>
#include <drivers/ArduinoDirectGPIODriver.h>

static uint8_t mapper(uint8_t r, uint8_t c)
{
    return (uint8_t)(2 + r * 2 + c);
}

ArduinoDirectGPIODriver driver(2, 2, mapper, 5);

// XPointStatic embeds all buffers — no heap allocation.
XPointStatic<2, 2> matrix;

void setup()
{
    Serial.begin(115200);
    driver.begin();
    matrix.setDriver(&driver);
    matrix.begin();
    Serial.println("XPoint ready (2x2)");
}

void loop()
{
    // Connect each node individually
    for (uint8_t r = 0; r < 2; r++)
    {
        for (uint8_t c = 0; c < 2; c++)
        {
            matrix.connect(r, c);
            Serial.print("connect(");
            Serial.print(r);
            Serial.print(",");
            Serial.print(c);
            Serial.println(")");
            delay(500);
        }
    }

    delay(1000);

    // Disconnect everything in one call
    matrix.clearAll();
    Serial.println("clearAll()");

    delay(1000);
}
