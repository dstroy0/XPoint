// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "WireI2C.h"

#if defined(ARDUINO)

void WireI2C::begin()
{
    Wire.begin();
}

void WireI2C::writeRegister(uint8_t addr, uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

void WireI2C::writeByte(uint8_t addr, uint8_t data)
{
    Wire.beginTransmission(addr);
    Wire.write(data);
    Wire.endTransmission();
}

#else // host build — no-ops (tests use MockI2C, not WireI2C)

void WireI2C::begin()
{
}

void WireI2C::writeRegister(uint8_t /*addr*/, uint8_t /*reg*/, uint8_t /*val*/)
{
}

void WireI2C::writeByte(uint8_t /*addr*/, uint8_t /*data*/)
{
}

#endif
