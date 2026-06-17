// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file XPointDriver.h
 * @brief Abstract hardware driver interface for the XPoint crosspoint matrix.
 *
 * Derive from XPointDriver to plug any hardware backend into XPoint:
 * - Direct MCU GPIO via ArduinoDirectGPIODriver
 * - 74HC595 shift-register chain via ArduinoShiftRegisterDriver
 * - MCP23017 I2C GPIO expander via MCP23017Driver
 * - TLC59711 12-channel 16-bit SPI PWM driver via TLC59711Driver
 * - Any custom relay board, I/O expander, or solid-state switch
 *
 * **Minimum implementation:** override begin() and setNodeHardware().
 * setNodeLevel(), releaseNode(), and commitPhysicalUpdates() have safe
 * defaults so simple binary drivers require no extra code.
 *
 * **Dual-coil latching relay protocol** (RE_LATCHING_DUAL_COIL):
 * @code
 * setNodeHardware(r, c, true)   // energise SET   coil — relay closes
 * setNodeHardware(r, c, false)  // energise RESET coil — relay opens
 * releaseNode(r, c)             // de-energise coil — called by XPoint::update()
 * commitPhysicalUpdates()       // flush state — called after every operation
 * @endcode
 */

#ifndef XPOINT_DRIVER_H
#define XPOINT_DRIVER_H

#include <stdint.h>

/**
 * @brief Abstract base class for all XPoint hardware drivers.
 *
 * XPoint holds a pointer to an XPointDriver and calls its methods to translate
 * logical connect / disconnect / level requests into hardware operations.
 */
class XPointDriver
{
  public:
    virtual ~XPointDriver()
    {
    }

    /**
     * @brief Initialise hardware: pin modes, bus setup, initial output state.
     *
     * Called once by XPoint::begin().  Must be overridden.
     */
    virtual void begin() = 0;

    /**
     * @brief Drive one matrix node on or off.
     *
     * - Non-latching: @p state `true` = energise, `false` = de-energise.
     * - Latching dual-coil: @p state `true` = pulse SET coil, `false` = pulse RESET coil.
     *
     * @param[in] r     Row index (zero-based).
     * @param[in] c     Column index (zero-based).
     * @param[in] state `true` to activate the node, `false` to deactivate.
     */
    virtual void setNodeHardware(uint8_t r, uint8_t c, bool state) = 0;

    /**
     * @brief Set an analogue drive level for node (r, c).
     *
     * Range: `0x0000` (off) to `0xFFFF` (full on).
     * The default implementation delegates to setNodeHardware(`level > 0`) so
     * binary drivers work unchanged without overriding this method.
     * Override in PWM-capable drivers (TLC59711, PCA9685) to set a fractional
     * output for brightness control or hold-current reduction.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] level PWM level `0x0000`–`0xFFFF`.
     */
    virtual void setNodeLevel(uint8_t r, uint8_t c, uint16_t level)
    {
        setNodeHardware(r, c, level > 0);
    }

    /**
     * @brief De-energise the coil that was pulsed on node (r, c).
     *
     * Called automatically by XPoint::update() once @p pulseDuration ms have
     * elapsed since the coil was activated.  Non-latching drivers can leave
     * this as the default no-op.
     *
     * @param[in] r Row index.
     * @param[in] c Column index.
     */
    virtual void releaseNode(uint8_t /*r*/, uint8_t /*c*/)
    {
    }

    /**
     * @brief Flush buffered state to hardware.
     *
     * Called by XPoint after every setNodeHardware(), setNodeLevel(), and
     * releaseNode() call.  Drivers that batch writes (shift registers, SPI)
     * must override this to clock out / transmit buffered data.
     * Direct-write drivers (GPIO, I2C) can leave this as the default no-op.
     *
     * - Shift-register drivers: clock bytes out and pulse the latch pin.
     * - SPI PWM drivers (TLC59711): transmit the full daisy-chain packet.
     * - MCP23017 / direct GPIO: no-op (writes commit in setNodeHardware).
     */
    virtual void commitPhysicalUpdates()
    {
    }
};

#endif // XPOINT_DRIVER_H
