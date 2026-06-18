# XPoint Test Report

**Generated:** 2026-06-18 18:44:36

## Categories

| Category   | Tests | Status  |
|------------|-------|---------|
| existing   | 25     | enabled |
| limit      | 6     | enabled |
| range      | 1     | SKIPPED |
| gap        | 13     | enabled |
| json       | 3     | enabled |
| sizes      | 1     | SKIPPED |

## Results

| # | Status | Test |
|---|--------|------|
| 1 | PASS | XPointStatic basic |
| 2 | PASS | user buffer constructor |
| 3 | PASS | I2CInterface begin() through ptr |
| 4 | PASS | latching connect pulse |
| 5 | PASS | latching disconnect pulse |
| 6 | PASS | latching rapid connect+disconnect |
| 7 | PASS | nonlatching disconnect no spurious |
| 8 | PASS | interlock |
| 9 | PASS | exclusive input |
| 10 | PASS | MCP23017 driver |
| 11 | PASS | TCA9548A transparent mux (MCP23017 via channel 3) |
| 12 | PASS | DirectGPIO driver |
| 13 | PASS | ShiftRegister driver |
| 14 | PASS | TLC59711 packet |
| 15 | PASS | TLC59711 setNodeHardware |
| 16 | PASS | setLevel binary driver |
| 17 | PASS | setLevel TLC59711 PWM |
| 18 | PASS | setLevel interlock |
| 19 | PASS | connect slot-full returns false |
| 20 | PASS | disconnect slot-full returns false |
| 21 | PASS | clearAll skips hardware when slot full |
| 22 | PASS | interlock desync blocked during RESET pulse |
| 23 | PASS | exclusive desync blocked during RESET pulse |
| 24 | PASS | setLevel latching registers pulse |
| 25 | PASS | setLevel latching slot-full returns false |
| 26 | PASS | bounds: degenerate 0-dim matrices |
| 27 | PASS | bounds: out-of-range indices return false |
| 28 | PASS | lockRows self-interlock noop |
| 29 | PASS | pdur=0 fires on first update() |
| 30 | PASS | millis() 32-bit rollover |
| 31 | PASS | no driver attached — no crash |
| 32 | SKIP | all matrix sizes 0x0 to 255x255 |
| 33 | PASS | gap: clearAll SET-pulse cancel and reuse |
| 34 | PASS | gap: setLevel latching disconnect path |
| 35 | PASS | gap: setLevel latching idempotent |
| 36 | PASS | gap: clearAll non-latching batch commit |
| 37 | PASS | gap: interlock fanout (row 0 to rows 1&2) |
| 38 | PASS | gap: setLevel respects exclusiveInput |
| 39 | PASS | gap: update() noop in non-latching mode |
| 40 | PASS | gap: connect idempotent with driver |
| 41 | PASS | gap: disconnect idempotent with driver |
| 42 | PASS | gap: MCP23017 port B (pins 8-15) |
| 43 | PASS | gap: MCP23017 shadow register accumulation |
| 44 | PASS | gap: ShiftRegister byte 1 bit packing |
| 45 | PASS | gap: TLC59711 2-chip daisy-chain ordering |
| 46 | PASS | json: slot-full scenarios |
| 47 | PASS | json: desync scenarios |
| 48 | PASS | json: interlock patterns |
| 49 | SKIP | sizes: pool/object sizes (this platform) |

## Summary

47 tests run, 2 skipped, **0 failure(s)**
