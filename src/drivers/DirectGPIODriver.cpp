// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "DirectGPIODriver.h"
#include <string.h>

DirectGPIODriver::DirectGPIODriver(uint16_t nPins, MapFn map) : _nPins(nPins), _map(map), _pins(new bool[nPins])
{
    memset(_pins, 0, (size_t)nPins * sizeof(bool));
}

DirectGPIODriver::~DirectGPIODriver()
{
    delete[] _pins;
}

void DirectGPIODriver::begin()
{
}

void DirectGPIODriver::setNodeHardware(uint8_t r, uint8_t c, bool state)
{
    uint16_t idx = _map(r, c);
    if (idx >= _nPins)
        return;
    _pins[idx] = state;
}

void DirectGPIODriver::commitPhysicalUpdates()
{
}

bool DirectGPIODriver::pinState(uint16_t idx) const
{
    if (idx >= _nPins)
        return false;
    return _pins[idx];
}
