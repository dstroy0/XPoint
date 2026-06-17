// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file I2CInterface.h
 * @brief Minimal abstract I2C interface consumed by MCP23017Driver.
 *
 * Concrete implementations:
 * - WireI2C   — wraps Arduino Wire (drivers/WireI2C.h)
 * - MockI2C   — records writes for host-side unit tests (test/test_xpoint.cpp)
 *
 * Supply your own subclass for non-Arduino platforms or hardware mocking.
 */

#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include <stdint.h>

/**
 * @brief Abstract I2C bus interface.
 *
 * Decouples MCP23017Driver from any specific I2C implementation so the same
 * driver code compiles and tests on both Arduino targets and host machines.
 */
class I2CInterface
{
  public:
    virtual ~I2CInterface()
    {
    }

    /**
     * @brief Initialise the I2C bus.
     *
     * Default no-op: safe for mock objects or buses started elsewhere.
     * WireI2C overrides this to call `Wire.begin()`.
     */
    virtual void begin()
    {
    }

    /**
     * @brief Write one byte to a device register.
     *
     * @param[in] addr 7-bit I2C device address.
     * @param[in] reg  Register index within the device.
     * @param[in] val  Byte value to write.
     */
    virtual void writeRegister(uint8_t addr, uint8_t reg, uint8_t val) = 0;
};

#endif // I2C_INTERFACE_H
