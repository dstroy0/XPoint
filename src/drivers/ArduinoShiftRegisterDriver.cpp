// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "ArduinoShiftRegisterDriver.h"
#include <string.h>

#if defined(ARDUINO)

ArduinoShiftRegisterDriver::ArduinoShiftRegisterDriver(uint16_t nOut, MapFn map, int dPin, int ckPin, int ltPin)
    : _nOut(nOut), _map(map), _nBytes((nOut + 7) / 8), _buf(new uint8_t[_nBytes]), _dPin(dPin), _ckPin(ckPin),
      _ltPin(ltPin)
{
    memset(_buf, 0, _nBytes);
}

ArduinoShiftRegisterDriver::~ArduinoShiftRegisterDriver()
{
    delete[] _buf;
}

void ArduinoShiftRegisterDriver::begin()
{
    pinMode(_dPin, OUTPUT);
    pinMode(_ckPin, OUTPUT);
    pinMode(_ltPin, OUTPUT);
    digitalWrite(_dPin, LOW);
    digitalWrite(_ckPin, LOW);
    digitalWrite(_ltPin, LOW);
    commitPhysicalUpdates(); // drive all outputs LOW
}

void ArduinoShiftRegisterDriver::setNodeHardware(uint8_t r, uint8_t c, bool state)
{
    uint16_t idx = _map(r, c);
    if (idx >= _nOut)
        return;
    uint16_t bi = idx / 8;
    uint8_t b = idx % 8;
    if (state)
        _buf[bi] |= (uint8_t)(1 << b);
    else
        _buf[bi] &= (uint8_t)~(1 << b);
}

void ArduinoShiftRegisterDriver::commitPhysicalUpdates()
{
    /* Standard 74HC595 daisy-chain: clock MSB first, last byte first.
     * This loads the first byte into the register nearest the latch pin. */
    digitalWrite(_ltPin, LOW);
    for (int bi = (int)_nBytes - 1; bi >= 0; --bi)
    {
        uint8_t byte = _buf[bi];
        for (int b = 7; b >= 0; --b)
        {
            digitalWrite(_ckPin, LOW);
            digitalWrite(_dPin, (byte & (1 << b)) ? HIGH : LOW);
            digitalWrite(_ckPin, HIGH);
        }
    }
    digitalWrite(_ltPin, HIGH);
}

#else // host build — no-ops

ArduinoShiftRegisterDriver::ArduinoShiftRegisterDriver(uint16_t, MapFn, int, int, int)
    : _nOut(0), _map(nullptr), _nBytes(0), _buf(nullptr), _dPin(0), _ckPin(0), _ltPin(0)
{
}

ArduinoShiftRegisterDriver::~ArduinoShiftRegisterDriver()
{
    delete[] _buf;
}

void ArduinoShiftRegisterDriver::begin()
{
}
void ArduinoShiftRegisterDriver::setNodeHardware(uint8_t, uint8_t, bool)
{
}
void ArduinoShiftRegisterDriver::commitPhysicalUpdates()
{
}

#endif
