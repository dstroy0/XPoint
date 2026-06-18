# XPoint test suite {#tests}

All tests live in `test/test_xpoint.cpp` and are compiled and run on the host with a plain C++11 compiler — no Arduino hardware required. A minimal `Arduino.h` shim in `test/` satisfies any Arduino-specific headers included by the library sources.

## Building and running

**Linux / macOS**

```bash
g++ -std=c++11 -Wall -Wextra -Wpedantic -Isrc -Itest \
  test/test_xpoint.cpp \
  src/XPoint.cpp \
  src/drivers/BitPool.cpp \
  src/drivers/DirectGPIODriver.cpp \
  src/drivers/ShiftRegisterDriver.cpp \
  src/drivers/MCP23017Driver.cpp \
  src/drivers/TLC59711Driver.cpp \
  -o test_xpoint && ./test_xpoint
```

Or use the helper script (builds, runs, and opens `test/TEST_REPORT.md`):

```bash
./test/build_and_run.sh
```

**Windows (PowerShell)**

```powershell
.\test\build_and_run_windows.ps1
```

**Custom parametrised run**

```bash
./test_custom [--rows=N] [--cols=M] [--latching] [--pulse=N]
```

Helper scripts with hardware-profile presets: `test/run_custom_test.sh` / `test/run_custom_test.ps1`.

---

## Test categories

The suite is split into five default categories and one opt-in category. Pass `--skip-<name>` to disable a default category; pass `--sizes` to enable the opt-in one.

| Category     | Count | Flag              | Description |
|--------------|-------|-------------------|-------------|
| `existing`   | 25    | `--skip-existing` | Core API tests covering the primary API surface |
| `limit`      | 6     | `--skip-limit`    | Boundary and degenerate-input tests |
| `range`      | 1     | `--skip-range` / `--fast` | Full 0×0–255×255 matrix sweep (~20 s) |
| `gap`        | 13    | `--skip-gap`      | Branch-coverage tests for previously untested code paths |
| `json`       | 3     | `--skip-json`     | Data-driven scenarios loaded from `test/data/*.json` |
| `sizes`      | 1     | `--sizes`         | Pool/object sizeof measurements (opt-in, not in default run) |

Common invocations:

```bash
./test_xpoint                    # 48 tests, 1 (sizes) skipped
./test_xpoint --fast             # skip the 256×256 range sweep
./test_xpoint --sizes            # also measure and report pool/object sizes
./test_xpoint --skip-json        # skip JSON-driven tests (useful without test/data/)
```

Results are always written to `test/TEST_REPORT.md` and printed to stdout simultaneously.

---

## Existing tests (25)

### `XPointStatic basic`

Constructs an `XPointStatic<3,3>` (zero-heap, compile-time dimensions). Calls `lockRows(0, 1)`, verifies that `connect(0, 0)` succeeds and `connect(1, 0)` is blocked by the interlock. Verifies that the subsequent `disconnect(0, 0)` fires exactly one `setNodeHardware` call with `state=false`.

### `user buffer constructor`

Constructs `XPoint` using the pool constructor with a caller-supplied `uint32_t[2]` array (2 words for a 4×4 matrix). Confirms `connect(1, 2)` fires exactly one `setNodeHardware(true)` call, `disconnect(1, 2)` fires exactly one `setNodeHardware(false)` call, and that the state bit for node (1,2) is directly cleared in the caller's word array (proving the library writes into the caller's buffer rather than an internal copy).

### `I2CInterface begin() through ptr`

Obtains a `MockI2C*` through an `I2CInterface*` base pointer and calls `begin()`. Verifies the default no-op `begin()` compiles and does not crash.

### `latching connect pulse`

Creates a `RE_LATCHING_DUAL_COIL` matrix with a 10 ms pulse duration. Calls `connect(0, 1)` and checks that exactly one `setNodeHardware` call was made with `state=true` (SET coil energized) and that `releaseNode` has not yet been called. Advances the mock clock past the pulse duration and calls `update()`; checks that `releaseNode(0, 1)` was called exactly once and that no additional `setNodeHardware` calls were made.

### `latching disconnect pulse`

Extends the latching connect scenario: after the SET pulse expires, clears the call log, calls `disconnect(0, 1)`, and verifies exactly one `setNodeHardware(false)` call (RESET coil). Advances the clock and calls `update()`; verifies `releaseNode` is called once and no spurious hardware calls appear.

### `nonlatching disconnect no spurious call`

Connects then disconnects a node on a `RE_NON_LATCHING` matrix. Verifies that `disconnect` produces exactly one `setNodeHardware(false)` call.

### `interlock`

Calls `lockRows(0, 1)` on a 3×3 matrix, connects row 0 to column 0, then attempts to connect row 1 to column 0. Verifies the first call returns `true` and the second returns `false`.

### `exclusive input`

Calls `exclusiveInput(2)` on a 3×3 matrix, connects row 0 to column 2, then attempts to connect row 1 to column 2. Verifies the first call returns `true` and the second returns `false`.

### `TCA9548A transparent mux`

Wraps a `MockI2C` in a `TCA9548AInterface` (address 0x71, channel 3) and passes it to an `MCP23017Driver`. Calls `mux.begin()` (invalidates the channel cache), then `m.begin()`. Verifies that exactly 5 I2C writes were recorded — the first being the channel-select `{0x71, 0x08, 0x00}` (channel 3 mask = 1<<3) and the remaining 4 being the MCP23017 IODIR/OLAT writes — confirming the cache-miss path fires exactly once. Clears the write log and calls `connect(1, 2)`; verifies exactly 2 writes (OLATA + OLATB, no channel-select) — confirming zero extra I2C overhead on a cache-hit path.

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

Calls `setPWM(0, 0x1234)` and `setPWM(1, 0xABCD)` on a single-chip `TLC59711Driver` and calls `commitPhysicalUpdates()`. Verifies the 28-byte packet (correct control word, correct GS byte positions, all unused bytes zero).

### `TLC59711 setNodeHardware`

Calls `setNodeHardware(0, 2, true)` (channel 2 = GS2, packet bytes 22–23) and verifies both bytes are `0xFF`. Calls `setNodeHardware(0, 2, false)` and verifies both bytes are `0x00`.

### `setLevel binary driver`

Calls `setLevel(0, 1, 0x8000)` on a `MockDriver`-backed matrix and confirms one `setNodeHardware(true)` call. Calls `setLevel(0, 1, 0)` and confirms one `setNodeHardware(false)` call.

### `setLevel TLC59711 PWM`

Calls `setLevel(0, 0, 0x8000)` through a `TLC59711Driver`-backed matrix and verifies that channel 0 (GS0, packet bytes 26–27) contains `0x80 0x00` — the exact 16-bit value without clamping. Calls `setLevel(0, 0, 0)` and verifies `0x00 0x00`.

### `setLevel interlock`

Calls `lockRows(0, 1)`, connects row 0 to column 0 via `setLevel(0, 0, 0x8000)`, then attempts `setLevel(1, 0, 0x4000)`. Verifies the first returns `true` and the second returns `false`.

### `latching rapid connect/disconnect`

Tests the in-flight pulse race condition. Calls `connect(0, 0)` to start a SET coil pulse, then immediately calls `disconnect(0, 0)` before the pulse expires — verifies it returns `false` (coil busy). Advances the clock past `pulseDuration` and calls `update()` to fire `releaseNode`. Verifies `disconnect(0, 0)` now succeeds.

### `connect slot-full returns false`

Fills all 8 pulse slots, then attempts a 9th `connect()`. Verifies the 9th returns `false` and emits no `setNodeHardware` call.

### `disconnect slot-full returns false`

Connects a node, lets its SET pulse expire (slot freed), fills all 8 slots again, then attempts `disconnect()` on the first node. Verifies it returns `false` with no RESET coil call.

### `clearAll skips hardware when slot full`

Connects 9 nodes in two passes, then calls `clearAll()`. Verifies that exactly 8 RESET coil calls are emitted (the 9th node is silently skipped), that no stuck coil results, and that after pulse expiry the 9th node can be re-connected.

### `interlock desync blocked during RESET pulse`

Connects row 0, waits for SET pulse to expire, disconnects row 0 (RESET pulse in-flight), then verifies that interlocked row 1 is blocked during the RESET window but succeeds after `update()` clears the slot.

### `exclusive desync blocked during RESET pulse`

Same as above but with an exclusive-input column instead of an interlocked row pair.

### `setLevel latching registers pulse`

Calls `setLevel(0, 0, 0x8000)` in `RE_LATCHING_DUAL_COIL` mode and verifies that `update()` calls `releaseNode(0, 0)` after the pulse duration.

### `setLevel latching slot-full returns false`

Fills all 8 slots then calls `setLevel()` on a 9th node. Verifies it returns `false` with no hardware call.

---

## Limit tests (6)

### `bounds: degenerate 0-dim matrices`

Constructs `XPoint(0, 0)`, `XPoint(0, 4)`, and `XPoint(4, 0)`. Verifies all operations return `false` without crashing.

### `bounds: out-of-range indices return false`

On a 4×4 matrix, verifies that `connect(4, 0)`, `connect(0, 4)`, `connect(255, 0)`, `connect(0, 255)`, `disconnect(4, 0)`, and `setLevel(4, 0, ...)` all return `false` and emit no driver calls.

### `lockRows self-interlock noop`

Calls `lockRows(0, 0)` (self-lock) and verifies `connect(0, 0)` still succeeds.

### `pdur=0 fires on first update()`

Creates a `RE_LATCHING_DUAL_COIL` matrix with `pdur=0`. Verifies that `connect()` succeeds and that the very next `update()` call (elapsed = 0 ≥ 0) fires `releaseNode`.

### `millis() 32-bit rollover`

Sets the mock clock to `0xFFFFFFFA` (5 ms before rollover), connects, advances 6 ms (wraps to 0 — not yet expired), then advances 5 more ms (total elapsed = 11 ≥ 10). Verifies the pulse fires exactly once after the second advance.

### `no driver attached — no crash`

Calls `connect`, `disconnect`, `clearAll`, and `update` without ever calling `setDriver`. Verifies no crash and that logical state is updated correctly.

---

## Range test (1)

### `all matrix sizes 0×0 to 255×255`

Iterates every `(rows, cols)` combination from `(0,0)` to `(255,255)`. For degenerate matrices, verifies that `connect` and `disconnect` return `false`. For valid matrices, verifies idempotent `connect`/`disconnect` on the first and last nodes plus out-of-range guard checks. This is the most time-consuming test (~20 s); use `--fast` to skip it.

---

## Gap coverage tests (13)

These tests were added to reach branch coverage on paths previously untested by the existing suite.

### `gap: clearAll SET-pulse cancel and reuse`

Connects 2 nodes (SET pulses in-flight), then calls `clearAll()` while the pulses are still live. Verifies that `clearAll()` cancels the in-flight SET entries and reuses those slots for RESET pulses — exactly 2 `setNodeHardware(false)` calls, exactly 1 `commitPhysicalUpdates()`, no releases yet — and that after advancing the clock, 2 `releaseNode` calls fire.

### `gap: setLevel latching disconnect path`

Connects a node, lets the SET pulse expire, then calls `setLevel(0, 0, 0)` (level=0 → disconnecting path). Verifies one `setNodeHardware(false)` call and correct `releaseNode` timing.

### `gap: setLevel latching idempotent`

Calls `setLevel` with the same value as the current state (already connected, level > 0; or already disconnected, level = 0). Verifies no hardware call is emitted.

### `gap: clearAll non-latching batch commit`

Connects 3 nodes on a `RE_NON_LATCHING` matrix, calls `clearAll()`, and verifies exactly 3 `setNodeHardware(false)` calls and exactly 1 `commitPhysicalUpdates()` (batched, not one per node).

### `gap: interlock fanout (row 0 to rows 1 & 2)`

Sets `lockRows(0,1)` and `lockRows(0,2)`. With row 0 holding column 0, verifies rows 1 and 2 are each blocked from column 0, yet both can connect to other columns, and that rows 1 and 2 are not locked with each other.

### `gap: setLevel respects exclusiveInput`

Marks column 0 exclusive, connects via `setLevel(0, 0, 0x8000)`, verifies row 1 is blocked, then releases via `setLevel(0, 0, 0)` (disconnect path bypasses exclusive check) and verifies row 1 can now connect.

### `gap: update() noop in non-latching mode`

Calls `update()` 10 times on a `RE_NON_LATCHING` matrix with a connected node. Verifies no `releaseNode` calls, no `setNodeHardware` calls, and no `commitPhysicalUpdates` calls.

### `gap: connect idempotent with driver`

On a `RE_NON_LATCHING` matrix with a driver, calls `connect(0,0)` twice. Verifies the second call returns `true` but emits no additional `setNodeHardware` call.

### `gap: disconnect idempotent with driver`

Connects then disconnects a node, then calls `disconnect` again. Verifies the second `disconnect` returns `true` but emits no additional hardware call.

### `gap: MCP23017 port B (pins 8–15)`

Connects node `(2,0)` → pin 8 → port B bit 0; verifies OLATB (reg `0x15`) is written with bit 0 set. Then connects `(3,3)` → pin 15 → port B bit 7; verifies OLATB = `0x81`.

### `gap: MCP23017 shadow register accumulation`

Connects two nodes on port A, verifies the shadow accumulates both bits, and that a combined OLATA write (`0xC0`) appears. Disconnects one; verifies only the remaining bit is in the shadow.

### `gap: ShiftRegister byte 1 bit packing`

Connects nodes mapping to bit indices 8 and 15 (byte 1 of the shift-register chain). Verifies correct bit positions in `byteAt(1)` and that `byteAt(0)` is unaffected.

### `gap: TLC59711 2-chip daisy-chain ordering`

Creates a 2-chip `TLC59711Driver` (56-byte packet). Sets channel 0 (GS0 of chip 0, bytes 54–55) and channel 12 (GS0 of chip 1, bytes 26–27). Verifies both control words, both GS values, and that all other GS bytes are `0x00`.

---

## JSON-driven tests (3)

These tests load scenarios from `test/data/` and are skipped gracefully (returning `true`) when the files are not present, making them safe to run from outside the project root.

### `json: slot-full scenarios`

Loads `slot_full_scenarios.json`. Each object drives a `connect_overflow`, `disconnect_overflow`, or `clearall_overflow` scenario with configurable matrix dimensions, fill nodes, and overflow node. Verifies the slot-full invariants for each scenario.

### `json: desync scenarios`

Loads `desync_scenarios.json`. Each object configures an interlock or exclusive-input desync scenario with a RESET pulse in-flight. Verifies the blocked node is refused during the pulse window and allowed after `update()` clears the slot.

### `json: interlock patterns`

Loads `interlock_patterns.json`. Each object defines multiple `lockRows()` pairs and verifies that hold/blocked/allowed row assignments behave correctly under the configured interlock graph.

---

## Sizes test (1, opt-in)

### `sizes: pool/object sizes (this platform)`

Enabled only with `--sizes`. Prints a pool-sizes table (bits/words/bytes) for common matrix dimensions and a sizeof table for `XPoint`, `BitPool`, `PulseEvent`, and `XPointStatic` at several representative sizes. Output is written to both stdout and the `## Size Measurements` section of `test/TEST_REPORT.md`.

---

## Custom parametrised tests (`test_custom.cpp`)

Each sub-test uses `MockDriver` so all driver calls are directly observable.

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

### `latching slot-full returns false`

Only runs when `--latching` is passed and the matrix has at least 9 nodes. Fills 8 pulse slots, then verifies a 9th `connect()` returns `false` without emitting a hardware call.

---

## Mock infrastructure

| Component | Purpose |
|-----------|---------|
| `MockDriver` | Records every `setNodeHardware` call in `calls[]`, every `releaseNode` call in `releases[]`, and counts `commitPhysicalUpdates` invocations in `commits`. |
| `MockI2C` | Records every `writeRegister` call in `writes[]` for inspection by MCP23017 tests. |
| `millis()` / `delay()` shim | Backed by a global `g_millis` counter. `advanceMillis(ms)` increments it to simulate elapsed time without real waits. Reset to 0 before each test. |

## Report

Every run of `test_xpoint` writes `test/TEST_REPORT.md` — a Markdown file with a categories table, per-test pass/fail results, and a summary. With `--sizes` an additional size-measurement section is appended. The file is included in the Doxygen documentation.
