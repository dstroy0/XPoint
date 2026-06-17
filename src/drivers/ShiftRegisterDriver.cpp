// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "ShiftRegisterDriver.h"
#include <string.h>

ShiftRegisterDriver::ShiftRegisterDriver(uint16_t nOut, MapFn map)
    : _nOut(nOut), _map(map), _nBytes((nOut + 7) / 8), _buf(new uint8_t[_nBytes])
{
    memset(_buf, 0, _nBytes);
}

ShiftRegisterDriver::~ShiftRegisterDriver()
{
    delete[] _buf;
}

void ShiftRegisterDriver::begin()
{
}

void ShiftRegisterDriver::setNodeHardware(uint8_t r, uint8_t c, bool state)
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

void ShiftRegisterDriver::commitPhysicalUpdates()
{
}

uint8_t ShiftRegisterDriver::byteAt(uint16_t idx) const
{
    if (idx >= _nBytes)
        return 0;
    return _buf[idx];
}
