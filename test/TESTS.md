# XPoint test suite {#tests}

All tests live in `test/test_xpoint.cpp` and are compiled and run on the host with a plain C++11 compiler — no Arduino hardware required. A minimal `Arduino.h` shim in `test/` satisfies any Arduino-specific headers included by the library sources.

## Building and running

**Linux / macOS**

```bash
g++ -std=c++11 -Wall -Wextra -Wpedantic -Isrc -Itest \
  test/test_xpoint.cpp \
  src/XPoint.cpp \
  src/drivers/DirectGPIODriver.cpp \
  src/drivers/ShiftRegisterDriver.cpp \
  src/drivers/MCP23017Driver.cpp \
  src/drivers/TLC59711Driver.cpp \
  -o test_xpoint && ./test_xpoint
```

Or use the helper script:

```bash
./test/build_and_run.sh
```

**Windows (PowerShell)**

```powershell
.\test\build_and_run_windows.ps1
```

**Custom parameterised run**

```bash
./test_xpoint --custom [--rows=N] [--cols=M] [--latching] [--pulse=N]
```

Helper scripts with hardware-profile presets: `test/run_custom_test.sh` / `test/run_custom_test.ps1`.

---

## Fixed tests (run by default)

### `XPointStatic basic`

Constructs an `XPointStatic<3,3>` (zero-heap, compile-time dimensions). Calls `lockRows(0, 1)`, verifies that `connect(0, 0)` succeeds and `connect(1, 0)` is blocked by the interlock. Verifies that the subsequent `disconnect(0, 0)` fires exactly one `setNodeHardware` call with `state=false`.

### `user buffer constructor`

Constructs `XPoint` using the buffer constructor with stack-allocated arrays. Confirms `connect(1, 2)` fires exactly one `setNodeHardware(true)` call, `disconnect(1, 2)` fires exactly one `setNodeHardware(false)` call, and that the user-supplied `matrixBuf[1*4+2]` byte is directly observable as `false` after disconnect (proving the library writes into the caller's buffer rather than an internal copy).

### `I2CInterface begin() through ptr`

Obtains a `MockI2C*` through an `I2CInterface*` base pointer and calls `begin()`. Verifies the default no-op `begin()` compiles and does not crash — confirms the virtual dispatch chain is intact.

### `latching connect pulse`

Creates a `RE_LATCHING_DUAL_COIL` matrix with a 10 ms pulse duration. Calls `connect(0, 1)` and checks that exactly one `setNodeHardware` call was made with `state=true` (SET coil energized) and that `releaseNode` has not yet been called. Advances the mock clock past the pulse duration and calls `update()`; checks that `releaseNode(0, 1)` was called exactly once and that no additional `setNodeHardware` calls were made.

### `latching disconnect pulse`

Extends the latching connect scenario: after the SET pulse expires, clears the call log, calls `disconnect(0, 1)`, and verifies exactly one `setNodeHardware(false)` call (RESET coil). Advances the clock and calls `update()`; verifies `releaseNode` is called once and no spurious hardware calls appear.

### `nonlatching disconnect no spurious call`

Connects then disconnects a node on a `RE_NON_LATCHING` matrix. Verifies that `disconnect` produces exactly one `setNodeHardware(false)` call — catching the historical bug where an unconditional `setNodeHardware(true)` was fired before the de-energize call.

### `interlock`

Calls `lockRows(0, 1)` on a 3×3 matrix, connects row 0 to column 0, then attempts to connect row 1 to column 0. Verifies the first call returns `true` and the second returns `false`.

### `exclusive input`

Calls `exclusiveInput(2)` on a 3×3 matrix, connects row 0 to column 2, then attempts to connect row 1 to column 2. Verifies the first call returns `true` and the second returns `false`.

### `MCP23017 driver`

Constructs an `MCP23017Driver` backed by a `MockI2C` and calls `begin()`. Inspects the I2C write log to confirm that IODIRA (reg `0x00`) and IODIRB (reg `0x01`) are each written with `0x00` (all-outputs) before any OLAT write. Then calls `connect(1, 2)` (pin index 6 → OLATA bit 6) and confirms that OLATA (reg `0x14`) is written with bit 6 set.

### `DirectGPIO driver`

Calls `setNodeHardware(1, 2, true)` on a `DirectGPIODriver` with a row-major mapper (pin index = `r*4+c`). Checks that `pinState(6)` returns `true`. Calls `setNodeHardware(1, 2, false)` and checks that `pinState(6)` returns `false`.

### `ShiftRegister driver`

Verifies the bit-packing logic of `ShiftRegisterDriver`:

- `setNodeHardware(0, 7, true)` sets bit 7 of byte 0 → `byteAt(0) == 0x80`
- `setNodeHardware(1, 0, true)` also sets bit 4 of byte 0 → `byteAt(0) == 0x90`
- `setNodeHardware(0, 7, false)` clears bit 7 → `byteAt(0) == 0x10`

### `TLC59711 packet`

Calls `setPWM(0, 0x1234)` and `setPWM(1, 0xABCD)` on a single-chip `TLC59711Driver` and calls `commitPhysicalUpdates()`. Verifies the 28-byte packet:

- Packet size is exactly 28 bytes.
- Bytes 0–3 are the correct control word: `0x96 0xDF 0xFF 0xFF` (command `0x25`, OUTTMG=1, EXTGCK=0, TMGRST=1, DSPRPT=1, BLANK=0, BC=0x7F for all channels).
- Bytes 4–23 (GS11–GS2) are all `0x00`.
- Bytes 24–25 are `0xAB 0xCD` (GS1 = channel 1).
- Bytes 26–27 are `0x12 0x34` (GS0 = channel 0).

### `TLC59711 setNodeHardware`

Calls `setNodeHardware(0, 2, true)` (channel 2 = GS2, packet bytes 22–23) and verifies both bytes are `0xFF`. Calls `setNodeHardware(0, 2, false)` and verifies both bytes are `0x00`.

### `setLevel binary driver`

Calls `setLevel(0, 1, 0x8000)` on a `MockDriver`-backed matrix and confirms one `setNodeHardware(true)` call (default `setNodeLevel` delegates to `setNodeHardware(level > 0)`). Calls `setLevel(0, 1, 0)` and confirms one `setNodeHardware(false)` call.

### `setLevel TLC59711 PWM`

Calls `setLevel(0, 0, 0x8000)` through a `TLC59711Driver`-backed matrix and verifies that channel 0 (GS0, packet bytes 26–27) contains `0x80 0x00` — the exact 16-bit value without clamping. Calls `setLevel(0, 0, 0)` and verifies `0x00 0x00`.

### `setLevel interlock`

Calls `lockRows(0, 1)`, connects row 0 to column 0 via `setLevel(0, 0, 0x8000)`, then attempts `setLevel(1, 0, 0x4000)`. Verifies the first returns `true` and the second returns `false`, confirming interlock protections apply to `setLevel` on the connecting path.

### `latching rapid connect/disconnect`

Regression test for the in-flight pulse race condition. Calls `connect(0, 0)` to start a SET coil pulse, then immediately calls `disconnect(0, 0)` before the pulse expires — verifies it returns `false` (coil busy). Advances the clock past `pulseDuration` and calls `update()` to fire `releaseNode`. Verifies `disconnect(0, 0)` now succeeds.

---

## Custom parameterised tests (`--custom` flag)

Each sub-test uses `MockDriver` so all driver calls are directly observable regardless of the matrix size.

### `connect / disconnect all nodes (idempotent)`

Connects every `rows × cols` node and verifies each returns `true`. Re-connects `(0, 0)` and confirms no additional driver call is made (idempotent). Disconnects every node and verifies each returns `true`. Re-disconnects `(0, 0)` and confirms no additional driver call.

### `setLevel connect + disconnect`

Calls `setLevel(0, 0, 0x8000)` and verifies `setNodeHardware(true)` was recorded. Calls `setLevel(0, 0, 0)` and verifies `setNodeHardware(false)` was recorded.

### `interlock lockRows(0, 1)`

Skipped automatically when `rows < 2`. Calls `lockRows(0, 1)`, connects row 0 to column 0 (must succeed), attempts row 1 to column 0 (must fail), disconnects row 0, then attempts row 1 again (must now succeed).

### `exclusive input col 0`

Skipped automatically when `rows < 2`. Calls `exclusiveInput(0)`, connects row 0 to column 0 (must succeed), attempts row 1 to column 0 (must fail), disconnects row 0, then attempts row 1 again (must now succeed).

### `latching relay SET + RESET pulse`

Only runs when `--latching` is passed. Connects `(0, 0)` and verifies `setNodeHardware(true)` (SET coil). Advances mock clock past `pulseDuration` and calls `update()`; verifies `releaseNode` fired. Disconnects `(0, 0)` and verifies `setNodeHardware(false)` (RESET coil). Advances clock and calls `update()`; verifies `releaseNode` fired again.

---

## Mock infrastructure

| Component                   | Purpose                                                                                                                                                  |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `MockDriver`                | Records every `setNodeHardware` call in `calls[]` and every `releaseNode` call in `releases[]`. `commitPhysicalUpdates` is a no-op.                      |
| `MockI2C`                   | Records every `writeRegister` call in `writes[]` for inspection by MCP23017 tests.                                                                       |
| `millis()` / `delay()` shim | Backed by a global `g_millis` counter. `advanceMillis(ms)` increments it to simulate elapsed time without real waits. Reset to 0 before each fixed test. |
