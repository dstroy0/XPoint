// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "TLC59711Driver.h"
#include <string.h>

#if defined(ARDUINO)
#include <SPI.h>
/*
 * MSBFIRST portability:
 *   AVR / ESP8266      — MSBFIRST is a preprocessor macro (= 1).
 *   SAMD / RP2040      — MSBFIRST is a BitOrder enum (ArduinoCore-API);
 *                        detected via ARDUINO_API_VERSION.
 *   Due (SAM)          — MSBFIRST is a BitOrder enum in framework-arduino-sam;
 *                        no ARDUINO_API_VERSION — detected via ARDUINO_ARCH_SAM.
 *   ESP32 (old core)   — neither macro nor BitOrder; define int 1.
 * Only emit the fallback define when none of the enum-using architectures are
 * active AND the macro is not already provided.
 */
#if !defined(MSBFIRST) && !defined(ARDUINO_API_VERSION) && !defined(ARDUINO_ARCH_SAM)
#define MSBFIRST 1
#endif
#endif

/*
 * Each TLC59711 packet is 28 bytes:
 *   4 bytes control word + 24 bytes GS data (12 channels x 2 bytes, MSB first).
 *
 * Control word with full-scale brightness and OUTTMG/TMGRST/DSPRPT set:
 *   0x96, 0xDF, 0xFF, 0xFF
 */
static const uint8_t CTRL_WORD[4] = {0x96, 0xDF, 0xFF, 0xFF};
static const uint8_t BYTES_PER_CHIP = 28;

TLC59711Driver::TLC59711Driver(uint8_t nChips, MapFn map)
    : _nChips(nChips), _map(map), _nCh((uint16_t)nChips * 12U), _pktSz((uint16_t)nChips * BYTES_PER_CHIP),
      _pwms(new uint16_t[_nCh]), _pkt(new uint8_t[_pktSz])
{
    memset(_pwms, 0, sizeof(uint16_t) * _nCh);
    memset(_pkt, 0, _pktSz);
}

TLC59711Driver::~TLC59711Driver()
{
    delete[] _pwms;
    delete[] _pkt;
}

void TLC59711Driver::begin()
{
#if defined(ARDUINO)
    SPI.begin();
#endif
}

void TLC59711Driver::setNodeHardware(uint8_t r, uint8_t c, bool state)
{
    uint16_t ch = _map(r, c);
    if (ch < _nCh)
        _pwms[ch] = state ? 0xFFFFU : 0x0000U;
}

void TLC59711Driver::setNodeLevel(uint8_t r, uint8_t c, uint16_t level)
{
    uint16_t ch = _map(r, c);
    if (ch < _nCh)
        _pwms[ch] = level;
}

void TLC59711Driver::setPWM(uint16_t ch, uint16_t val)
{
    if (ch < _nCh)
        _pwms[ch] = val;
}

void TLC59711Driver::commitPhysicalUpdates()
{
    /*
     * In a daisy-chain the first bytes shifted out propagate to the last
     * physical chip.  Send chip N-1 data first so software chip 0 stays
     * at the nearest-MOSI device, matching the mapper's channel numbering.
     */
    uint16_t off = 0;
    for (int8_t chip = (int8_t)_nChips - 1; chip >= 0; --chip)
    {
        _pkt[off + 0] = CTRL_WORD[0];
        _pkt[off + 1] = CTRL_WORD[1];
        _pkt[off + 2] = CTRL_WORD[2];
        _pkt[off + 3] = CTRL_WORD[3];

        /* GS channels sent highest-index first (GS11 → GS0). */
        uint16_t base = (uint16_t)(uint8_t)chip * 12U;
        for (uint8_t i = 0; i < 12; ++i)
        {
            uint16_t idx = base + (11U - i);
            uint16_t v = (idx < _nCh) ? _pwms[idx] : 0U;
            _pkt[off + 4 + i * 2] = (uint8_t)(v >> 8);
            _pkt[off + 4 + i * 2 + 1] = (uint8_t)(v & 0xFF);
        }
        off += BYTES_PER_CHIP;
    }

#if defined(ARDUINO)
    SPI.beginTransaction(SPISettings(10000000UL, MSBFIRST, SPI_MODE0));
    for (uint16_t i = 0; i < _pktSz; ++i)
        SPI.transfer(_pkt[i]);
    SPI.endTransaction();
#endif
}
