// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file TLC59711Driver.h
 * @brief Driver for the TLC59711 12-channel 16-bit PWM constant-current LED driver.
 *
 * **SPI packet format** (28 bytes per chip, sent MSB first):
 *
 * | Bytes | Field | Description |
 * |-------|-------|-------------|
 * | 0–3   | Control word (32-bit) | CMD=0x25, OUTTMG=1, TMGRST=1, DSPRPT=1, BC=0x7F all |
 * | 4–27  | GS11 down to GS0 | 16 bits each, MSB first |
 *
 * Resulting control bytes: `0x96 0xDF 0xFF 0xFF`.
 *
 * **Daisy-chain byte order:** in a chain of N chips the first bytes shifted
 * out propagate to the last physical chip.  commitPhysicalUpdates() sends chip
 * N−1 data first so that mapper channel 0 always maps to the first physical
 * chip (nearest MOSI).
 *
 * - connect() / disconnect() drive the channel to `0xFFFF` / `0x0000`.
 * - setLevel() sets any 16-bit value without clamping.
 * - setPWM() provides direct channel access bypassing the matrix state table.
 */

#ifndef TLC59711_DRIVER_H
#define TLC59711_DRIVER_H

#include "XPointDriver.h"
#include <stdint.h>

/**
 * @brief XPointDriver implementation for the TLC59711 SPI PWM driver.
 *
 * Supports daisy-chaining multiple chips.  Each chip provides 12 channels,
 * giving `nChips * 12` total addressable outputs.
 *
 * All PWM values are held in a heap buffer and transmitted as a single SPI
 * burst by commitPhysicalUpdates().
 */
class TLC59711Driver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → channel index [0, nChips*12)`. */
    typedef uint16_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct the driver.
     *
     * @param[in] nChips Number of daisy-chained TLC59711 chips (1 chip = 12 channels).
     * @param[in] map    Mapper; must return channel indices in `[0, nChips*12)`.
     */
    TLC59711Driver(uint8_t nChips, MapFn map);

    /** @brief Destructor — frees PWM and packet buffers. */
    ~TLC59711Driver();

    /**
     * @brief Initialize the SPI bus by calling `SPI.begin()`.
     * No-op on host builds.
     */
    void begin() override;

    /**
     * @brief Set channel to `0xFFFF` (state=true) or `0x0000` (state=false).
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = full on, `false` = off.
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /**
     * @brief Set channel to an exact 16-bit PWM value without clamping.
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] level PWM value `0x0000`–`0xFFFF`.
     */
    void setNodeLevel(uint8_t r, uint8_t c, uint16_t level) override;

    /**
     * @brief Assemble and transmit the SPI packet for all chips.
     *
     * Sends chip N−1 data first so the chain's physical ordering matches
     * the mapper's channel numbering.  No-op on host builds.
     */
    void commitPhysicalUpdates() override;

    /**
     * @brief Direct channel write, bypassing the XPoint state table.
     *
     * @param[in] ch  Channel index `[0, nChips*12)`.
     * @param[in] val 16-bit PWM value.
     */
    void setPWM(uint16_t ch, uint16_t val);

    /**
     * @brief Return a pointer to the most recently assembled SPI packet.
     *
     * Valid until the next commitPhysicalUpdates() call.
     *
     * @return Pointer to the internal packet buffer.
     */
    const uint8_t *lastPacket() const
    {
        return _pkt;
    }

    /**
     * @brief Return the size of the SPI packet in bytes.
     * @return `nChips * 28`.
     */
    uint16_t lastPacketSize() const
    {
        return _pktSz;
    }

  private:
    uint8_t _nChips; ///< Number of daisy-chained chips.
    MapFn _map;      ///< Node-to-channel mapper.
    uint16_t _nCh;   ///< Total channel count = _nChips * 12.
    uint16_t _pktSz; ///< Packet size in bytes = _nChips * 28.
    uint16_t *_pwms; ///< Heap [_nCh] — current PWM values.
    uint8_t *_pkt;   ///< Heap [_pktSz] — last assembled SPI packet.
};

#endif // TLC59711_DRIVER_H
