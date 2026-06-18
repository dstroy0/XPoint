// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file DirectGPIODriver.h
 * @brief Virtual (host-side) GPIO driver for unit testing.
 *
 * Backs each matrix node with a bool in a heap array; no real hardware is
 * touched.  Used by the host test suite to verify XPoint logic without Arduino
 * dependencies.  The bool array is directly inspectable via pinState().
 *
 * The mapper function must return a pin index in `[0, nPins)`.
 */

#ifndef DIRECT_GPIO_DRIVER_H
#define DIRECT_GPIO_DRIVER_H

#include "XPointDriver.h"
#include <stdint.h>

/**
 * @brief Host-only virtual GPIO driver — no hardware dependency.
 *
 * Useful for unit tests and simulation.  Does not link against any Arduino
 * headers, making it safe to build with a plain C++ toolchain.
 */
class DirectGPIODriver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → pin index`. */
    typedef uint16_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct with a virtual pin count and a node-to-pin mapper.
     *
     * @param[in] nPins Total number of virtual pins to allocate (heap).
     * @param[in] map   Mapper function; must return indices in `[0, nPins)`.
     */
    DirectGPIODriver(uint16_t nPins, MapFn map);

    /** @brief Destructor — frees the virtual pin array. */
    ~DirectGPIODriver();

    /** @brief Zero all virtual pins. */
    void begin() override;

    /**
     * @brief Set the virtual pin for node (r, c) to @p state.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = pin HIGH, `false` = pin LOW.
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /** @brief No-op for this virtual driver (writes are immediate). */
    void commitPhysicalUpdates() override;

    /**
     * @brief Read the logical state of virtual pin @p idx.
     *
     * @param[in] idx Pin index in `[0, pinCount)`.
     * @return `true` if the pin is HIGH, `false` if LOW or out of range.
     */
    bool pinState(uint16_t idx) const;

    /**
     * @brief Return the total virtual pin count.
     * @return Number of pins allocated at construction.
     */
    uint16_t pinCount() const
    {
        return _nPins;
    }

  private:
    uint16_t _nPins; ///< Total virtual pin count.
    MapFn _map;      ///< Node-to-pin mapper.
    bool *_pins;     ///< Heap-allocated pin state array [_nPins].
};

#endif // DIRECT_GPIO_DRIVER_H
