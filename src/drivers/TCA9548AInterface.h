// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file TCA9548AInterface.h
 * @brief Transparent I2C bus-multiplexer adapter for the TCA9548A / PCA9548A.
 *
 * Drop this decorator between any I2CInterface consumer (e.g. MCP23017Driver)
 * and the physical bus (e.g. WireI2C) to transparently route I2C traffic
 * through a specific channel of a TCA9548A / PCA9548A bus multiplexer.
 *
 * **Capacity:** one TCA9548A provides 8 independent I2C segments; up to 8
 * TCA9548As share one bus (addresses 0x70–0x77), giving 64 segments total.
 * Each segment supports up to 8 MCP23017s (address pins A0–A2), so the
 * maximum reachable I/O is 64 × 8 × 16 = 8192 pins from a single bus.
 *
 * **Performance:** a per-mux channel cache (8 bytes of BSS) ensures the
 * channel-select byte is only sent when the active channel actually changes.
 * After warm-up, a relay operation on an already-selected channel incurs zero
 * extra I2C transactions — identical overhead to having no mux at all.
 *
 * **Usage:**
 * @code
 * WireI2C wire;
 *
 * // Two MCP23017s on channel 0 of TCA9548A at 0x70:
 * TCA9548AInterface ch0(&wire, 0x70, 0);
 * MCP23017Driver gpioA(&ch0, 0x20, mapperA);
 * MCP23017Driver gpioB(&ch0, 0x21, mapperB);
 *
 * // One MCP23017 on channel 1 (same mux, different segment):
 * TCA9548AInterface ch1(&wire, 0x70, 1);
 * MCP23017Driver gpioC(&ch1, 0x20, mapperC);
 *
 * // setup():
 * ch0.begin(); // initialises Wire and primes the channel cache
 * matrixA.begin();
 * matrixB.begin();
 * matrixC.begin();
 * @endcode
 *
 * @note TCA9548AInterface is header-only; no extra compilation unit required.
 */

#ifndef TCA9548A_INTERFACE_H
#define TCA9548A_INTERFACE_H

#include "I2CInterface.h"

/**
 * @brief I2C channel-select decorator for the TCA9548A / PCA9548A.
 *
 * writeRegister() selects this instance's channel on the mux (if not already
 * selected) then forwards the write to the underlying bus.  The matrix and
 * driver layers are completely unaware of the multiplexer.
 *
 * Channel state is cached per mux address in a function-local static array
 * (8 bytes of BSS).  Calling begin() invalidates the cache entry for this
 * mux so the first subsequent write always emits the channel-select byte.
 */
class TCA9548AInterface : public I2CInterface
{
    I2CInterface *_bus;    ///< Underlying I2C bus (WireI2C or another adapter).
    uint8_t _muxAddr;      ///< TCA9548A 7-bit address (0x70–0x77).
    uint8_t _channelMask;  ///< 1u << channel — precomputed at construction.

    /* Per-mux-address cache of the last channel-select byte written.
     * Indexed by (_muxAddr & 0x7): slot 0 = 0x70, …, slot 7 = 0x77.
     * Sentinel 0xFFu means "unknown / needs re-select".
     *
     * Defined inside the class body → implicitly inline in C++11, so the
     * function-local static `s[]` is shared across all translation units. */
    static uint8_t &_lastSel(uint8_t slot)
    {
        static uint8_t s[8] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
        return s[slot];
    }

    /* Select this channel on the mux only if it differs from the cached value.
     * On a cache hit: zero extra I2C transactions. */
    inline void _selectIfNeeded()
    {
        uint8_t &sel = _lastSel(_muxAddr & 0x7u);
        if (sel != _channelMask)
        {
            _bus->writeByte(_muxAddr, _channelMask);
            sel = _channelMask;
        }
    }

  public:
    /**
     * @param[in] bus      Physical I2C bus; must outlive this object.
     * @param[in] muxAddr  TCA9548A 7-bit address (0x70–0x77).
     * @param[in] channel  Bus segment to activate on writeRegister() (0–7).
     */
    TCA9548AInterface(I2CInterface *bus, uint8_t muxAddr, uint8_t channel)
        : _bus(bus), _muxAddr(muxAddr), _channelMask((uint8_t)(1u << (channel & 0x7u)))
    {
    }

    /**
     * @brief Initialise the underlying bus and invalidate this mux's channel cache.
     *
     * Call once per mux address in setup() before any matrix.begin() calls.
     * Invalidating the cache guarantees the channel-select byte is sent on the
     * very first writeRegister() even if that channel was previously active.
     */
    void begin() override
    {
        _bus->begin();
        _lastSel(_muxAddr & 0x7u) = 0xFFu;
    }

    /**
     * @brief Activate this channel (if not already selected), then forward the write.
     *
     * When the channel is already active the mux write is suppressed entirely —
     * zero overhead compared to a direct I2C connection.
     *
     * @param[in] devAddr 7-bit address of the target device (e.g. MCP23017).
     * @param[in] reg     Register index on the target device.
     * @param[in] val     Byte value to write.
     */
    void writeRegister(uint8_t devAddr, uint8_t reg, uint8_t val) override
    {
        _selectIfNeeded();
        _bus->writeRegister(devAddr, reg, val);
    }
};

#endif // TCA9548A_INTERFACE_H
