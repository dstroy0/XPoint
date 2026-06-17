// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file MCP23017Driver.h
 * @brief Driver for the MCP23017 16-bit I2C GPIO expander.
 *
 * The MCP23017 provides two 8-bit GPIO ports (GPA and GPB):
 * - GPA0–GPA7 → pin indices 0–7
 * - GPB0–GPB7 → pin indices 8–15
 *
 * The mapper function must return a pin index in `[0, 15]`.
 *
 * begin() writes `IODIRA = IODIRB = 0x00` (all outputs) before driving OLAT,
 * overriding the device's power-on default of all-inputs.
 *
 * Up to eight MCP23017s can share one I2C bus; address pins A0–A2 select
 * addresses 0x20–0x27.  Drive relay coils via transistors — MCP23017 output
 * current is limited to 25 mA per pin.
 *
 * @note commitPhysicalUpdates() is a no-op because MCP23017 writes commit
 * immediately inside setNodeHardware().
 */

#ifndef MCP23017_DRIVER_H
#define MCP23017_DRIVER_H

#include "../I2CInterface.h"
#include "../XPointDriver.h"
#include <stdint.h>

/**
 * @brief XPointDriver implementation for the MCP23017 16-bit I2C GPIO expander.
 *
 * Maintains shadow copies of OLATA and OLATB to minimise I2C transactions;
 * both registers are written atomically on each setNodeHardware() call.
 */
class MCP23017Driver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → MCP23017 pin index 0–15`. */
    typedef uint8_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct the driver.
     *
     * @param[in] i2c  I2C bus implementation (WireI2C or MockI2C).
     * @param[in] addr 7-bit device address (0x20–0x27 set by A0–A2 pins).
     * @param[in] map  Mapper; must return pin indices in `[0, 15]`.
     */
    MCP23017Driver(I2CInterface *i2c, uint8_t addr, MapFn map);

    /**
     * @brief Set IODIRA and IODIRB to 0x00 (all outputs) then write OLATA/OLATB.
     *
     * Must be called before the first setNodeHardware() so the device does
     * not fight the MCU on the bus lines.
     */
    void begin() override;

    /**
     * @brief Set or clear one output pin and immediately write both OLAT registers.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = pin HIGH, `false` = pin LOW.
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /** @brief No-op — MCP23017 writes commit inside setNodeHardware(). */
    void commitPhysicalUpdates() override
    {
    }

    /**
     * @brief Read the OLATA shadow register (for diagnostics / testing).
     * @return Current shadow value of OLATA.
     */
    uint8_t gpioA() const
    {
        return _ga;
    }

    /**
     * @brief Read the OLATB shadow register (for diagnostics / testing).
     * @return Current shadow value of OLATB.
     */
    uint8_t gpioB() const
    {
        return _gb;
    }

  private:
    I2CInterface *_i2c; ///< I2C bus implementation.
    uint8_t _addr;      ///< 7-bit I2C device address.
    MapFn _map;         ///< Node-to-pin mapper.
    uint8_t _ga;        ///< Shadow copy of OLATA.
    uint8_t _gb;        ///< Shadow copy of OLATB.

    /** @brief Write shadow values to OLATA and OLATB. */
    void _commit();
};

#endif // MCP23017_DRIVER_H
