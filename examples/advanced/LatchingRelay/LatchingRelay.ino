// XPoint — dual-coil latching relay example
//
// Latching relays hold their switched position without continuous power.
// connect()    → pulses the SET  coil for pulseDuration ms, then releases.
// disconnect() → pulses the RESET coil for pulseDuration ms, then releases.
// matrix.update() must be called every loop() to expire pulses on time.
//
// This sketch includes a minimal dual-coil driver.
// Each node (r,c) has two output pins: SET and RESET.
//
//   Node (r,c)  SET pin            RESET pin
//   (0,0)       2                  3
//   (0,1)       4                  5
//   (1,0)       6                  7
//   (1,1)       8                  9
//
// Drive each pin through a transistor or relay-driver IC; never connect a
// coil directly to an MCU pin.  See examples/WIRING.md for circuit details.

#include <XPoint.h>

// ---- Inline dual-coil driver -----------------------------------------------
// Maps (row, col) to SET and RESET output pins.
static uint8_t setPin(uint8_t r, uint8_t c)
{
    return 2 + (r * 2 + c) * 2;
}
static uint8_t resetPin(uint8_t r, uint8_t c)
{
    return 3 + (r * 2 + c) * 2;
}

class DualCoilDriver : public XPointDriver
{
    uint8_t _rows, _cols;

  public:
    DualCoilDriver(uint8_t rows, uint8_t cols) : _rows(rows), _cols(cols)
    {
    }

    void begin() override
    {
        for (uint8_t r = 0; r < _rows; r++)
        {
            for (uint8_t c = 0; c < _cols; c++)
            {
                pinMode(setPin(r, c), OUTPUT);
                digitalWrite(setPin(r, c), LOW);
                pinMode(resetPin(r, c), OUTPUT);
                digitalWrite(resetPin(r, c), LOW);
            }
        }
    }

    // state=true  → energise SET   coil (relay closes)
    // state=false → energise RESET coil (relay opens)
    void setNodeHardware(uint8_t row, uint8_t col, bool state) override
    {
        if (state)
        {
            digitalWrite(setPin(row, col), HIGH);
            digitalWrite(resetPin(row, col), LOW);
        }
        else
        {
            digitalWrite(setPin(row, col), LOW);
            digitalWrite(resetPin(row, col), HIGH);
        }
    }

    // Called automatically after pulseDuration ms to de-energise the coil.
    // The relay keeps its position with no power until the next pulse.
    void releaseNode(uint8_t row, uint8_t col) override
    {
        digitalWrite(setPin(row, col), LOW);
        digitalWrite(resetPin(row, col), LOW);
    }
};
// ----------------------------------------------------------------------------

DualCoilDriver driver(2, 2);

// 20 ms coil pulse — check your relay's datasheet for the minimum required.
XPointStatic<2, 2> matrix(RE_LATCHING_DUAL_COIL, 20);

unsigned long lastToggle = 0;
bool relaysOn = false;

void setup()
{
    Serial.begin(115200);
    driver.begin();
    matrix.setDriver(&driver);
    matrix.begin();

    // Pulse SET coil — relays latch closed, coils release after 20 ms.
    matrix.connect(0, 0);
    matrix.connect(1, 1);
    relaysOn = true;
    lastToggle = millis();
    Serial.println("Latched CLOSED (0,0) and (1,1)");
}

void loop()
{
    // Required: expires pulses and calls releaseNode() after pulseDuration ms.
    matrix.update();

    // Toggle every 3 seconds to demonstrate SET / RESET cycle.
    if (millis() - lastToggle >= 3000)
    {
        lastToggle = millis();
        if (relaysOn)
        {
            matrix.disconnect(0, 0);
            matrix.disconnect(1, 1);
            relaysOn = false;
            Serial.println("Latched OPEN  (0,0) and (1,1)");
        }
        else
        {
            matrix.connect(0, 0);
            matrix.connect(1, 1);
            relaysOn = true;
            Serial.println("Latched CLOSED (0,0) and (1,1)");
        }
    }
}
