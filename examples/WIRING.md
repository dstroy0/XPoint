# XPoint Wiring Guide

This short guide shows common wiring patterns for `XPoint` users.

1. Latching relays (dual-coil)

- Many latching relays use two coils: `SET` and `RESET` pins. The library treats `connect()` and `disconnect()` as logical operations; the driver must implement the physical mapping to energize the correct coil for a short pulse and then de-energize.
- Recommended wiring: use a shift register or I/O expander to drive transistor drivers for each coil (MOSFET/NPN + flyback diode). Do NOT tie coils directly to MCU pins if current is high.

2. Non-latching relays / analog switches

- Non-latching relays or analog switches (e.g., ADG1408, MT8816) are driven when `setNodeHardware(row,col,true)` is called. For these, driver implementations should set the hardware 'on' while the internal state marks the node as connected.

3. 74HC595 shift register chains

- Use `HC595Helper::rowMajor(rows, cols)` to map nodes to shift-register bits in a simple row-major order.
- When chaining multiple `74HC595`, ensure `ArduinoShiftRegisterDriver`'s `commitPhysicalUpdates()` shifts out bytes MSB-first for the last byte first (standard chain order). Adjust wiring mapping accordingly.

4. MCP23017 / I2C expanders

- `MCP23017Driver` writes the OLAT registers (OLATA/OLATB) to set outputs. Use transistors if driving relays. Address pins (A0..A2) allow multiple expanders on the bus.

5. Interlocks & safety

- Use `lockRows()` to prevent two sensitive rails from being connected to the same column. Use `exclusiveInput()` when a column must only have a single driver at once.

6. Transistor driver reference (recommended)

- For driving relay coils or higher-current loads use an NPN or N-channel MOSFET with a flyback diode across the coil. A simple NPN driver circuit:
  - MCU pin -> base resistor (4.7k) -> base of NPN (e.g., 2N2222)
  - Collector -> relay coil negative
  - Relay coil positive -> Vcc (e.g., 12V)
  - Emitter -> GND
  - Flyback diode across coil (cathode to Vcc, anode to collector)

- For MOSFETs prefer a logic-level N-channel (e.g., IRLZ44 or similar small MOSFET) with gate resistor and pull-down.

7. TLC59711 PWM expanders

- `src/drivers/TLC59711Driver.*` is a skeleton driver that can be extended to use SPI to control PWM channels for LED drivers or analog-level switching. Use a transistor/MOSFET stage when driving relays or loads larger than the output pin is capable of.
