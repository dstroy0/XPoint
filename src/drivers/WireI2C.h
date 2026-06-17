// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file WireI2C.h
 * @brief Arduino Wire wrapper implementing I2CInterface.
 *
 * On Arduino targets (`ARDUINO` defined) begin() calls `Wire.begin()` and
 * writeRegister() performs a standard two-byte I2C register write using the
 * Wire library.
 *
 * On host targets (no `ARDUINO`) both methods are no-ops so the library
 * compiles without Arduino headers for testing.
 */

#ifndef WIRE_I2C_H
#define WIRE_I2C_H

#include "../I2CInterface.h"

#if defined(ARDUINO)
#include <Wire.h>
#endif

/**
 * @brief I2CInterface implementation that delegates to the Arduino Wire library.
 *
 * Create one instance per I2C bus and pass a pointer to MCP23017Driver.
 *
 * @code
 * WireI2C bus;
 * MCP23017Driver gpio(&bus, 0x20, myMapper);
 * @endcode
 */
class WireI2C : public I2CInterface
{
  public:
    WireI2C()
    {
    }

    /**
     * @brief Initialise the I2C bus by calling `Wire.begin()`.
     *
     * No-op on host builds.
     */
    void begin() override;

    /**
     * @brief Write @p val to register @p reg on device at 7-bit address @p addr.
     *
     * Sequence: `beginTransmission` → `write(reg)` → `write(val)` → `endTransmission`.
     * No-op on host builds.
     *
     * @param[in] addr 7-bit I2C device address.
     * @param[in] reg  Register index.
     * @param[in] val  Byte to write.
     */
    void writeRegister(uint8_t addr, uint8_t reg, uint8_t val) override;
};

#endif // WIRE_I2C_H
