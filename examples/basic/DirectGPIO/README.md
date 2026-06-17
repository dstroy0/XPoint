# DirectGPIO Example

This example shows how to wire a small matrix directly to MCU pins and use `ArduinoDirectGPIODriver`.

Wiring

- Connect each matrix node to a dedicated MCU pin through a driver transistor (recommended) or directly for low-current signals.
- Define a free function to map `(row, col)` to a pin number and pass it to the driver constructor.

Pin mapping example (4x4, pins 2..17):

```cpp
static uint16_t mapper(uint8_t r, uint8_t c) { return (uint16_t)(2 + r * 4 + c); }
ArduinoDirectGPIODriver driver(4, 4, mapper, 17);
```

`begin()` will configure only the pins returned by `mapper`, so special-purpose pins (RX/TX, I2C, SPI) are not disturbed.

Safety

- Avoid driving relay coils directly from MCU pins. Use transistor/MOSFET drivers and proper flyback diodes.
