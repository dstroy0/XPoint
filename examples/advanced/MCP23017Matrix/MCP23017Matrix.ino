// XPoint — MCP23017 I2C expander example (2×8 matrix, 16 nodes)
//
// The MCP23017 provides 16 GPIO outputs (GPA0–GPA7, GPB0–GPB7) via I2C.
// A 2-row × 8-column matrix fits exactly on one chip.
// Up to 8 MCP23017s can share one I2C bus (address pins A0–A2 set 0x20–0x27).
//
// Wiring:
//   SDA → Arduino A4 (Uno) / pin 20 (Mega)
//   SCL → Arduino A5 (Uno) / pin 21 (Mega)
//   A0, A1, A2 → GND  (I2C address = 0x20)
//   GPA0–GPA7  → row 0 relay driver inputs (cols 0–7)
//   GPB0–GPB7  → row 1 relay driver inputs (cols 0–7)
//   Drive coils through transistors — MCP23017 outputs are 25 mA max each.

#include <Wire.h>
#include <XPoint.h>
#include <drivers/MCP23017Driver.h>
#include <drivers/WireI2C.h>

// Node (r,c) maps to MCP23017 pin r*8 + c (0–15: GPA0=0 … GPB7=15).
static uint8_t mapper(uint8_t r, uint8_t c)
{
    return (uint8_t)(r * 8 + c);
}

WireI2C i2c;
MCP23017Driver expander(&i2c, 0x20, mapper);

// 2 rows × 8 cols — all 16 outputs of one MCP23017.
XPointStatic<2, 8> matrix;

void setup()
{
    Serial.begin(115200);

    // begin() initialises Wire and configures MCP23017 IODIRA/B as all-outputs.
    i2c.begin();
    expander.begin();

    matrix.setDriver(&expander);
    matrix.begin();

    // Row 0 and row 1 cannot share any column (standard crossppoint protection).
    matrix.lockRows(0, 1);

    // Column 7 is an exclusive bus — only one row may connect at a time.
    matrix.exclusiveInput(7);

    Serial.println("MCP23017 matrix ready (2x8)");
}

void loop()
{
    // Walk across row 0, one column at a time.
    for (uint8_t c = 0; c < 8; c++)
    {
        matrix.clearAll();
        matrix.connect(0, c);
        Serial.print("row 0 -> col ");
        Serial.println(c);
        delay(400);
    }

    // Walk across row 1.
    for (uint8_t c = 0; c < 8; c++)
    {
        matrix.clearAll();
        matrix.connect(1, c);
        Serial.print("row 1 -> col ");
        Serial.println(c);
        delay(400);
    }

    matrix.clearAll();
    delay(800);
}
