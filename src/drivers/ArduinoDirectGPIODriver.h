// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file ArduinoDirectGPIODriver.h
 * @brief Arduino `digitalWrite()` driver for the XPoint crosspoint matrix.
 *
 * Each matrix node maps to one Arduino GPIO pin via a user-supplied function
 * pointer.  begin() iterates all nodes and calls `pinMode(OUTPUT)` on each;
 * setNodeHardware() calls `pinMode(OUTPUT)` and `digitalWrite()` immediately.
 *
 * No batching, no SPI/I2C, no commitPhysicalUpdates() needed.
 * Suited for small relay matrices driven directly from MCU GPIO pins.
 *
 * @note Not usable in host (non-Arduino) builds; all methods are no-ops when
 *       `ARDUINO` is not defined.
 */

#ifndef ARDUINO_DIRECT_GPIO_DRIVER_H
#define ARDUINO_DIRECT_GPIO_DRIVER_H

#include "XPointDriver.h"
#include <stdint.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

/**
 * @brief XPointDriver implementation that calls `digitalWrite()` per node.
 *
 * Suitable for direct relay drive on any Arduino-compatible MCU.  Each node
 * change results in an immediate `digitalWrite()` call — no buffering.
 */
class ArduinoDirectGPIODriver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → Arduino pin number`. */
    typedef uint8_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct the driver.
     *
     * @param[in] rows   Row count (used by begin() to iterate all nodes).
     * @param[in] cols   Column count.
     * @param[in] map    Mapper; must return a valid `digitalWrite()` pin number.
     * @param[in] maxPin Highest pin number the mapper can return (bounds guard).
     */
    ArduinoDirectGPIODriver(uint8_t rows, uint8_t cols, MapFn map, uint8_t maxPin);

    /**
     * @brief Iterate all nodes and call `pinMode(OUTPUT)` + `digitalWrite(LOW)`.
     *
     * Configures only the pins the mapper actually uses to avoid clobbering
     * special-purpose pins (RX/TX, SPI, I2C, etc.).
     */
    void begin() override;

    /**
     * @brief Call `pinMode(OUTPUT)` and drive the pin HIGH or LOW.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = HIGH (relay on), `false` = LOW (relay off).
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /** @brief No-op — writes commit immediately in setNodeHardware(). */
    void commitPhysicalUpdates() override
    {
    }

  private:
    uint8_t _rows;   ///< Row count (for begin() iteration).
    uint8_t _cols;   ///< Column count.
    MapFn _map;      ///< Node-to-pin mapper.
    uint8_t _maxPin; ///< Highest valid pin number (bounds guard).
};

#endif // ARDUINO_DIRECT_GPIO_DRIVER_H
