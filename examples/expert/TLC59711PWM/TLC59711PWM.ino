// XPoint — TLC59711 analog level control (1×12 matrix, one chip)
//
// The TLC59711 is a 12-channel 16-bit PWM constant-current LED/relay driver.
// connect()      → channel full on  (0xFFFF)
// disconnect()   → channel off      (0x0000)
// setLevel()     → fractional drive (0x0000–0xFFFF)
//
// Wiring (hardware SPI):
//   MOSI (Arduino 11) → TLC59711 SIN
//   SCK  (Arduino 13) → TLC59711 SCLK
//   No CS pin — TLC59711 latches on rising edge after 28-byte packet.
//   IREF resistor on each chip sets the full-scale output current.
//
// Daisy-chaining: for N chips pass chipCount=N to the driver; each chip's
// SIN connects to the previous chip's SOUT.  The driver sends N*28 bytes.

#include <SPI.h>
#include <XPoint.h>
#include <drivers/TLC59711Driver.h>

// Channel index for each (row, col) node.
// With 1 row and 12 cols, channel = col (0–11).
static uint16_t mapper(uint8_t /*r*/, uint8_t c)
{
    return (uint16_t)c;
}

TLC59711Driver pwmDrv(1 /*chipCount*/, mapper);

// 1 row × 12 cols — all 12 channels of one TLC59711.
XPointStatic<1, 12> matrix;

// Sweep state
uint16_t sweepLevel = 0;
bool sweepUp = true;

void setup()
{
    Serial.begin(115200);
    pwmDrv.begin(); // calls SPI.begin()
    matrix.setDriver(&pwmDrv);
    matrix.begin();

    // Set all channels to half-power at startup.
    for (uint8_t c = 0; c < 12; c++)
        matrix.setLevel(0, c, 0x7FFF);

    Serial.println("TLC59711 ready — 12-channel PWM sweep");
}

void loop()
{
    // Smooth triangle-wave sweep across all 12 channels simultaneously.
    if (sweepUp)
    {
        sweepLevel += 0x0100;
        if (sweepLevel >= 0xFF00)
            sweepUp = false;
    }
    else
    {
        sweepLevel -= 0x0100;
        if (sweepLevel <= 0x0100)
            sweepUp = true;
    }

    for (uint8_t c = 0; c < 12; c++)
        matrix.setLevel(0, c, sweepLevel);

    // Demonstrate: connect and disconnect are also available.
    // connect(0, 0)    → channel 0 full on  (0xFFFF)
    // disconnect(0, 0) → channel 0 full off (0x0000)

    delay(8); // ~120 Hz update rate
}
