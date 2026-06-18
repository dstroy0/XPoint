// XPoint — writing a custom driver
//
// Inherit from XPointDriver and implement the methods your hardware needs.
// Only setNodeHardware() is required; the rest have default no-op implementations.
//
// This sketch defines SerialDebugDriver, which prints every state change to
// Serial.  No external hardware is needed — useful for prototyping and learning
// the API before committing to a specific driver.
//
// After understanding the output, swap SerialDebugDriver for the real driver
// (ArduinoDirectGPIODriver, MCP23017Driver, TLC59711Driver, etc.).

#include <XPoint.h>

// ----------------------------------------------------------------------------
// SerialDebugDriver — logs all driver calls to Serial
// ----------------------------------------------------------------------------
class SerialDebugDriver : public XPointDriver
{
  public:
    void begin() override
    {
        Serial.println("[driver] begin()");
    }

    // Required: called by connect() / disconnect() / clearAll().
    // state=true  → connect  (energize relay, drive output HIGH)
    // state=false → disconnect (de-energize relay, drive output LOW)
    void setNodeHardware(uint8_t row, uint8_t col, bool state) override
    {
        Serial.print("[driver] setNodeHardware(");
        Serial.print(row);
        Serial.print(",");
        Serial.print(col);
        Serial.print(") -> ");
        Serial.println(state ? "ON" : "OFF");
    }

    // Optional: override for PWM-capable hardware (TLC59711, etc.).
    // Default implementation calls setNodeHardware(level > 0).
    void setNodeLevel(uint8_t row, uint8_t col, uint16_t level) override
    {
        Serial.print("[driver] setNodeLevel(");
        Serial.print(row);
        Serial.print(",");
        Serial.print(col);
        Serial.print(") -> 0x");
        Serial.println(level, HEX);
    }

    // Optional: override for latching relays — de-energize coil after pulse.
    void releaseNode(uint8_t row, uint8_t col) override
    {
        Serial.print("[driver] releaseNode(");
        Serial.print(row);
        Serial.print(",");
        Serial.print(col);
        Serial.println(")");
    }

    // Optional: override to batch-commit (shift registers, SPI expanders, etc.).
    // Called at the end of connect(), disconnect(), setLevel(), and clearAll().
    void commitPhysicalUpdates() override
    {
        // Serial.println("[driver] commitPhysicalUpdates()"); // uncomment to trace
    }
};
// ----------------------------------------------------------------------------

SerialDebugDriver dbg;
XPointStatic<4, 4> matrix;

void setup()
{
    Serial.begin(115200);
    matrix.setDriver(&dbg);
    matrix.begin();

    Serial.println("--- interlocks ---");
    matrix.lockRows(0, 1);    // row 0 and row 1 cannot share a column
    matrix.exclusiveInput(3); // column 3 accepts only one row at a time

    Serial.println("--- connect ---");
    matrix.connect(0, 0);
    matrix.connect(2, 3);

    Serial.println("--- setLevel (analog) ---");
    matrix.setLevel(3, 1, 0x8000); // 50 %

    Serial.println("--- clearAll ---");
    matrix.clearAll();

    Serial.println("--- latching relay (non-blocking) ---");
    // Changing to latching mode requires a new XPoint instance.
    // See examples/advanced/LatchingRelay for a complete latching example.
}

void loop()
{
    matrix.update(); // required each loop(); safe to call with any driver
}
