// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file ArduinoShiftRegisterDriver.h
 * @brief Software bit-bang shift-register driver for the XPoint crosspoint matrix.
 *
 * Supports any number of daisy-chained 8-bit shift registers (e.g. 74HC595).
 * setNodeHardware() updates a shadow byte array; commitPhysicalUpdates() clocks
 * all bytes out MSB-first, last byte first, which loads bit 0 of the first
 * register in a standard 74HC595 chain.
 *
 * begin() must be called before first use; it configures the three control pins
 * and drives all outputs LOW via commitPhysicalUpdates().
 *
 * @note Not usable in host (non-Arduino) builds; all methods are no-ops when
 *       `ARDUINO` is not defined.
 */

#ifndef ARDUINO_SHIFT_REGISTER_DRIVER_H
#define ARDUINO_SHIFT_REGISTER_DRIVER_H

#include "XPointDriver.h"
#include <stdint.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

/**
 * @brief XPointDriver implementation for daisy-chained 74HC595 shift registers.
 *
 * Uses software bit-bang via Arduino `digitalWrite()` — no SPI hardware
 * required.  Suitable when hardware SPI pins are already in use or unavailable.
 */
class ArduinoShiftRegisterDriver : public XPointDriver
{
  public:
    /** @brief Function pointer type: `(row, col) → bit index within the shift register chain`. */
    typedef uint16_t (*MapFn)(uint8_t r, uint8_t c);

    /**
     * @brief Construct the driver.
     *
     * @param[in] nOut  Total number of shift register output bits.
     * @param[in] map   Mapper; must return indices in `[0, nOut)`.
     * @param[in] dPin  DATA (DS) pin number.
     * @param[in] ckPin CLOCK (SH_CP) pin number.
     * @param[in] ltPin LATCH (ST_CP) pin number.
     */
    ArduinoShiftRegisterDriver(uint16_t nOut, MapFn map, int dPin, int ckPin, int ltPin);

    /** @brief Destructor — frees the shadow byte buffer. */
    ~ArduinoShiftRegisterDriver();

    /**
     * @brief Configure DATA, CLOCK, and LATCH pins as OUTPUT then clear all outputs.
     *
     * Calls commitPhysicalUpdates() to clock zeros to all shift register outputs.
     */
    void begin() override;

    /**
     * @brief Update the shadow buffer for the bit mapped from (r, c).
     *
     * Does **not** immediately clock to hardware; call commitPhysicalUpdates()
     * to flush (XPoint does this automatically after every operation).
     *
     * @param[in] r     Row index.
     * @param[in] c     Column index.
     * @param[in] state `true` = set bit, `false` = clear bit.
     */
    void setNodeHardware(uint8_t r, uint8_t c, bool state) override;

    /**
     * @brief Clock the full shadow buffer out to the shift register chain.
     *
     * Sends bytes MSB-first, last byte first (standard 74HC595 daisy-chain
     * order) then pulses the latch pin to transfer data to outputs.
     */
    void commitPhysicalUpdates() override;

  private:
    uint16_t _nOut;   ///< Total output bit count.
    MapFn _map;       ///< Node-to-bit-index mapper.
    uint16_t _nBytes; ///< Shadow buffer size = ceil(_nOut / 8).
    uint8_t *_buf;    ///< Heap-allocated shadow byte array [_nBytes].
    int _dPin;        ///< DATA (DS) pin.
    int _ckPin;       ///< CLOCK (SH_CP) pin.
    int _ltPin;       ///< LATCH (ST_CP) pin.
};

#endif // ARDUINO_SHIFT_REGISTER_DRIVER_H
