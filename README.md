# XPoint

[![Host Tests](https://github.com/dstroy0/XPoint/actions/workflows/host-tests.yml/badge.svg)](https://github.com/dstroy0/XPoint/actions/workflows/host-tests.yml)
[![PlatformIO](https://github.com/dstroy0/XPoint/actions/workflows/platformio.yml/badge.svg)](https://github.com/dstroy0/XPoint/actions/workflows/platformio.yml)
[![Arduino](https://github.com/dstroy0/XPoint/actions/workflows/arduino.yml/badge.svg)](https://github.com/dstroy0/XPoint/actions/workflows/arduino.yml)
[![Docs](https://github.com/dstroy0/XPoint/actions/workflows/docs.yml/badge.svg)](https://dstroy0.github.io/XPoint/)
[![Changelog](https://github.com/dstroy0/XPoint/actions/workflows/changelog.yml/badge.svg)](https://github.com/dstroy0/XPoint/actions/workflows/changelog.yml)

A small, hardware-agnostic C++11 library for managing crosspoint matrices and signal routing on Arduino and PlatformIO targets. Designed to work on everything from AVR (Uno/Nano) to ESP32 and ARM without pulling in the C++ standard library.

**[API Documentation](https://dstroy0.github.io/XPoint/)**

## Features

- `connect(row, col)` / `disconnect(row, col)` / `setLevel(row, col, level)` API
- `XPointStatic<ROWS, COLS>` — zero-heap variant with compile-time dimensions for AVR
- Non-blocking pulse manager for dual-coil latching relays (no blocking `delay()`)
- Interlock and exclusive-input protections against illegal shorting states
- Driver abstraction (`XPointDriver`) — swap hardware without touching application code
- No `std::vector`, no `std::function`, no exceptions — compatible with avr-libc

## Quick Start

Add to your `platformio.ini`:

```ini
lib_deps = https://github.com/dstroy0/XPoint
```

## Usage

### Choosing a constructor

**Heap-allocating (recommended for ESP32, STM32, native)**

```cpp
XPoint matrix(4, 4, RE_NON_LATCHING);
```

Uses `new[]`/`delete[]`. Dimensions are set at runtime. Suitable for any target with adequate heap.

**`XPointStatic<ROWS, COLS>` (recommended for AVR / zero-heap)**

```cpp
XPointStatic<4, 4> matrix(RE_NON_LATCHING);
```

All state arrays live inside the object — no heap allocation. Dimensions must be known at compile time. Safe to declare globally on an ATmega328P.

**User-buffer (maximum control)**

```cpp
bool matState[4 * 4], lockMap[4 * 4], excl[4];
XPoint matrix(4, 4, matState, lockMap, excl, RE_NON_LATCHING);
```

Pass your own pre-allocated buffers. The object does not own or free them.

### RAM usage

Total SRAM consumed = **class overhead** + **state buffers**.

**State buffers** hold the logical connection state and are sized by the matrix dimensions:

| Buffer                                    | Size (bytes)                 |
| ----------------------------------------- | ---------------------------- |
| `_matrixState` — one `bool` per node      | `rows × cols`                |
| `_interlockMap` — one `bool` per row-pair | `rows × rows`                |
| `_exclusiveCols` — one `bool` per column  | `cols`                       |
| **Total state buffers**                   | **rows×cols + rows² + cols** |

Common matrix sizes:

| Matrix | rows×cols | rows² | cols | **State total** |
| ------ | --------- | ----- | ---- | --------------- |
| 2×8    | 16        | 4     | 8    | **28 B**        |
| 4×4    | 16        | 16    | 4    | **36 B**        |
| 1×12   | 12        | 1     | 12   | **25 B**        |
| 4×8    | 32        | 16    | 8    | **56 B**        |
| 8×8    | 64        | 64    | 8    | **136 B**       |
| 8×16   | 128       | 64    | 16   | **208 B**       |
| 16×16  | 256       | 256   | 16   | **528 B**       |

**Class overhead** is fixed per `XPoint` instance, regardless of matrix size:

| Platform                                 | `sizeof(XPoint)` |
| ---------------------------------------- | ---------------- |
| AVR (ATmega328P, 8-bit, 2-byte pointers) | ~71 B            |
| 32-bit ARM / ESP32                       | ~100 B           |
| 64-bit host                              | 152 B (measured) |

The overhead is dominated by `PulseEvent _activePulses[8]` — 8 slots × 7 bytes each on AVR (56 B), 8 × 12 bytes on 32-bit (96 B).

Verify on your target with:

```cpp
Serial.println(sizeof(XPoint));
Serial.println(sizeof(XPointStatic<4,4>));
```

**Heap vs. `XPointStatic`**: both strategies consume the same total bytes. `XPointStatic<R,C>` embeds the buffers directly in the object (BSS / global storage), so the heap is never touched and there is no fragmentation risk.

### Basic matrix operations

```cpp
matrix.setDriver(&myDriver);
matrix.begin();

matrix.connect(0, 1);      // connect row 0 to column 1
matrix.disconnect(0, 1);   // disconnect
matrix.clearAll();         // de-energize everything

// In loop():
matrix.update();           // required for latching relays
```

### analog level (`setLevel`)

`setLevel(row, col, level)` calls `driver->setNodeLevel()` instead of `setNodeHardware()`. Binary drivers (GPIO, shift register) treat `level > 0` as on and `0` as off. PWM-capable drivers (TLC59711) set the actual fractional output:

```cpp
// Binary driver — identical to connect/disconnect:
matrix.setLevel(0, 1, 0xFFFF); // full on
matrix.setLevel(0, 1, 0x8000); // treated as on (level > 0)
matrix.setLevel(0, 1, 0);      // off

// TLC59711 driver — true analog control:
matrix.setLevel(0, 0, 0xFFFF); // full brightness / drive current
matrix.setLevel(0, 0, 0x4000); // 25 % — useful for hold-current reduction
matrix.setLevel(0, 0, 0);      // off
```

Interlock and exclusive-input protections apply to `setLevel` the same as `connect`.

### Interlocks and exclusive inputs

```cpp
matrix.lockRows(0, 1);     // row 0 and row 1 can never share a column
matrix.exclusiveInput(3);  // column 3 accepts only one row at a time
```

### Latching relay configuration

```cpp
// pulseDuration in milliseconds — how long the SET/RESET coil is energized
XPointStatic<4, 4> matrix(RE_LATCHING_DUAL_COIL, 20);
```

Call `matrix.update()` in `loop()`. It de-energizes coils automatically after the pulse duration via `driver->releaseNode()`.

## Drivers

All drivers are in `src/drivers/`. Arduino-specific drivers compile only when `ARDUINO` is defined; host-side stubs are provided for testing.

| Driver                       | File                           | Use case                                                  |
| ---------------------------- | ------------------------------ | --------------------------------------------------------- |
| `ArduinoDirectGPIODriver`    | `ArduinoDirectGPIODriver.*`    | One MCU pin per node via `digitalWrite`                   |
| `ArduinoShiftRegisterDriver` | `ArduinoShiftRegisterDriver.*` | 74HC595 chain driven by `digitalWrite`                    |
| `MCP23017Driver`             | `MCP23017Driver.*`             | MCP23017 16-bit I2C GPIO expander                         |
| `TLC59711Driver`             | `TLC59711Driver.*`             | TLC59711 12-channel 16-bit PWM SPI expander               |
| `DirectGPIODriver`           | `DirectGPIODriver.*`           | Virtual pin-state driver (testing / simulation)           |
| `ShiftRegisterDriver`        | `ShiftRegisterDriver.*`        | Virtual byte-shadow shift register (testing / simulation) |

### ArduinoDirectGPIODriver

```cpp
// mapper returns the Arduino pin number for each (row, col) node
static uint8_t mapper(uint8_t r, uint8_t c) { return (uint8_t)(2 + r * 4 + c); }

// rows, cols, mapper, maxPinIndex
ArduinoDirectGPIODriver driver(4, 4, mapper, 17);
```

`begin()` configures only the pins your mapper actually returns, so serial/I2C/SPI pins are never disturbed.

### ArduinoShiftRegisterDriver

```cpp
static uint16_t mapper(uint8_t r, uint8_t c) { return (uint16_t)(r * 4 + c); }

// outputs, mapper, dataPin, clockPin, latchPin
ArduinoShiftRegisterDriver sr(16, mapper, 2, 3, 4);
```

`commitPhysicalUpdates()` shifts bytes MSB-first, last register first (standard 74HC595 daisy-chain order).

### MCP23017Driver

```cpp
#include "src/drivers/WireI2C.h"
#include "src/drivers/MCP23017Driver.h"

WireI2C i2c;
static uint8_t mapper(uint8_t r, uint8_t c) { return (uint8_t)(r * 4 + c); }
MCP23017Driver expander(&i2c, 0x20, mapper);

// setup():
i2c.begin();
expander.begin();      // sets IODIRA/IODIRB to all-outputs before driving OLAT
```

Up to 8 MCP23017s can share one I2C bus (address pins A0–A2). Use a transistor stage when driving relay coils.

### TLC59711Driver

```cpp
static uint16_t mapper(uint8_t r, uint8_t c) { return (uint16_t)(r * 4 + c); }
TLC59711Driver pwmDrv(1 /*chipCount*/, mapper);

// Use connect/disconnect for full on/off, or setLevel for fractional drive:
matrix.setLevel(row, col, 0x8000); // 50 % output
pwmDrv.setPWM(channel, value);     // direct channel access if needed
```

`commitPhysicalUpdates()` assembles the correct 28-byte-per-chip SPI packet (4-byte control word + GS11→GS0 channel order) and transfers it once.

## Writing a custom driver

Inherit from `XPointDriver` and implement `begin()` and `setNodeHardware()`. All other methods have default no-op implementations.

```cpp
class MyDriver : public XPointDriver {
public:
    void begin() override {
        // initialize hardware
    }

    void setNodeHardware(uint8_t row, uint8_t col, bool state) override {
        // state=true  → energize / connect
        // state=false → de-energize / disconnect
        //               (for latching dual-coil: RESET coil direction)
    }

    // Optional — override for PWM-capable hardware:
    void setNodeLevel(uint8_t row, uint8_t col, uint16_t level) override {
        // level 0..0xFFFF
    }

    // Optional — override for latching relays to de-energize after pulse:
    void releaseNode(uint8_t row, uint8_t col) override {
        // stop driving whichever coil was pulsed
    }

    // Optional — override to batch-commit (e.g. latch pulse for shift registers):
    void commitPhysicalUpdates() override {}
};
```

### Latching dual-coil relay conventions

| XPoint call        | setNodeHardware state | Meaning          |
| ------------------ | --------------------- | ---------------- |
| `connect()`        | `true`                | Pulse SET coil   |
| `disconnect()`     | `false`               | Pulse RESET coil |
| `update()` expires | `releaseNode()`       | De-energize coil |

### Mapper functions

Drivers take a plain C function pointer whose return type matches the driver:

| Driver                       | MapFn return type | Range                  |
| ---------------------------- | ----------------- | ---------------------- |
| `ArduinoDirectGPIODriver`    | `uint8_t`         | Arduino pin number     |
| `ArduinoShiftRegisterDriver` | `uint16_t`        | bit index in SR chain  |
| `MCP23017Driver`             | `uint8_t`         | pin index 0–15         |
| `TLC59711Driver`             | `uint16_t`        | channel index 0–(N×12) |

Non-capturing lambdas convert to function pointers automatically in C++11:

```cpp
// Named function (ArduinoDirectGPIODriver — uint8_t):
static uint8_t gpioMap(uint8_t r, uint8_t c) { return (uint8_t)(2 + r * 4 + c); }

// Named function (TLC59711 / shift-register — uint16_t):
static uint16_t srMap(uint8_t r, uint8_t c) { return (uint16_t)(r * COLS + c); }

// Non-capturing lambda (C++11 implicit conversion):
auto myMapper = [](uint8_t r, uint8_t c) -> uint8_t { return (uint8_t)(r * 4 + c); };
```

`HC595Helper::rowMajorIndex(row, col, cols)` computes a row-major shift-register bit index inline.

## Testing

See [test/TESTS.md](test/TESTS.md) for the full test suite description, build instructions, and mock infrastructure reference.

## Project layout

```
src/
  XPoint.h / XPoint.cpp        — core matrix class + XPointStatic template
  drivers/
    XPointDriver.h             — abstract driver interface
    I2CInterface.h             — abstract I2C interface
    ArduinoDirectGPIODriver.*  — Arduino GPIO driver
    ArduinoShiftRegisterDriver.* — Arduino 74HC595 shift-out driver
    MCP23017Driver.*           — MCP23017 I2C expander driver
    TLC59711Driver.*           — TLC59711 SPI PWM driver
    WireI2C.*                  — Arduino Wire wrapper for I2CInterface
    DirectGPIODriver.*         — virtual GPIO driver (host tests)
    ShiftRegisterDriver.*      — virtual shift-register driver (host tests)
    HC595Helper.h              — shift-register index utility
test/
  test_xpoint.cpp              — host test suite (17 tests, no framework required)
  Arduino.h                    — minimal Arduino shim for host builds
  TESTS.md                     — test descriptions and build instructions
examples/
  basic/
    ConnectDisconnect/  — minimal API walkthrough: connect, clearAll, Serial log
    DirectGPIO/         — 4×4 matrix via one MCU pin per node
  advanced/
    ShiftRegister/      — 4×4 via daisy-chained 74HC595 with interlocks
    LatchingRelay/      — dual-coil latching relay with non-blocking pulse timing
    MCP23017Matrix/     — 2×8 via MCP23017 I2C expander
  expert/
    TLC59711PWM/        — 12-channel analog level sweep via TLC59711
    CustomDriver/       — implement XPointDriver from scratch (no hardware needed)
  WIRING.md             — wiring patterns, transistor driver circuit
  schematics/           — transistor driver reference schematic
.github/workflows/ci.yml       — CI: host tests + PlatformIO 8-board × 7-example matrix + Doxygen
```

## CI

GitHub Actions runs on every push and PR to `main`/`master`:

1. **Host tests** — `g++` compiles and runs `test/test_xpoint.cpp` (17 tests, no Arduino headers)
2. **PlatformIO** — 8 boards × 7 examples (56 parallel jobs): ATmega328P/2560/32U4, SAMD21, SAM3X8E, ESP8266, ESP32, iMXRT1062
3. **Doxygen** — generates dark-themed HTML docs and deploys to [GitHub Pages](https://dstroy0.github.io/XPoint/)

## License

AGPL-3.0-only — Copyright (c) 2026 Douglas Quigg (dstroy0) \<dquigg123@gmail.com\>
