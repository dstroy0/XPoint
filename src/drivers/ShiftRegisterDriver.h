// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file ShiftRegisterDriver.h
 * @brief Virtual (host-side) shift-register driver for unit testing.
 *
 * Maintains a byte-shadow array mirroring what a real 74HC595 chain would hold.
 * No hardware is touched; used by the host test suite.  Individual bytes are
 * inspectable via byteAt() to verify bit-packing logic.
 *
 * **Bit packing:** output index `idx` maps to byte `idx/8`, bit `idx%8`
 * (LSB = bit 0).  The mapper function must return indices in `[0, nOut)`.
 */

#ifndef SHIFT_REGISTER_DRIVER_H
#define SHIFT_REGISTER_DRIVER_H

#include "XPointDriver.h"
#include <stdint.h>

/**
 * @brief Host-only virtual shift-register driver — no hardware dependency.
 *
 * Useful for verifying bit-packing logic without Arduino headers or SPI/GPIO
 * hardware.  The shadow buffer is directly readable via byteAt().
 */
class ShiftRegisterDriver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → output bit index`. */
    typedef uint16_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct with an output count and a node-to-bit-index mapper.
     *
     * @param[in] nOut Total number of shift-register output bits.
     * @param[in] map  Mapper; must return indices in `[0, nOut)`.
     */
    ShiftRegisterDriver(uint16_t nOut, MapFn map);

    /** @brief Destructor — frees the shadow buffer. */
    ~ShiftRegisterDriver();

    /** @brief Zero the shadow buffer. */
    void begin() override;

    /**
     * @brief Set or clear one bit in the shadow buffer.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = set bit, `false` = clear bit.
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /** @brief No-op for this virtual driver (no physical chain to clock). */
    void commitPhysicalUpdates() override;

    /**
     * @brief Read one byte from the shadow buffer.
     *
     * @param[in] idx Byte index in `[0, byteCount)`.
     * @return Shadow byte value, or `0` if @p idx is out of range.
     */
    uint8_t byteAt(uint16_t idx) const;

    /**
     * @brief Return the number of bytes in the shadow buffer.
     * @return `ceil(nOut / 8)`.
     */
    uint16_t byteCount() const
    {
        return _nBytes;
    }

  private:
    uint16_t _nOut;   ///< Total output bit count.
    MapFn _map;       ///< Node-to-bit-index mapper.
    uint16_t _nBytes; ///< Shadow buffer byte count = ceil(_nOut / 8).
    uint8_t *_buf;    ///< Heap-allocated byte shadow [_nBytes].
};

#endif // SHIFT_REGISTER_DRIVER_H
