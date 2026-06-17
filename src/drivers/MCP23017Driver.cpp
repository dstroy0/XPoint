// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "MCP23017Driver.h"

/* MCP23017 register addresses (Bank 0, the power-on default). */
static const uint8_t REG_IODIRA = 0x00; // I/O direction port A (1=input, 0=output)
static const uint8_t REG_IODIRB = 0x01; // I/O direction port B
static const uint8_t REG_OLATA = 0x14;  // output latch port A
static const uint8_t REG_OLATB = 0x15;  // output latch port B

MCP23017Driver::MCP23017Driver(I2CInterface *i2c, uint8_t addr, MapFn map)
    : _i2c(i2c), _addr(addr), _map(map), _ga(0), _gb(0)
{
}

void MCP23017Driver::begin()
{
    if (!_i2c)
        return;
    /* MCP23017 powers up with all pins as inputs; set all to outputs first. */
    _i2c->writeRegister(_addr, REG_IODIRA, 0x00);
    _i2c->writeRegister(_addr, REG_IODIRB, 0x00);
    _commit(); // drive all outputs low (shadows already 0)
}

void MCP23017Driver::setNodeHardware(uint8_t r, uint8_t c, bool state)
{
    uint8_t pin = _map(r, c);
    if (pin < 8)
    {
        if (state)
            _ga |= (uint8_t)(1 << pin);
        else
            _ga &= (uint8_t)~(1 << pin);
    }
    else
    {
        uint8_t p = pin - 8;
        if (state)
            _gb |= (uint8_t)(1 << p);
        else
            _gb &= (uint8_t)~(1 << p);
    }
    _commit();
}

/* Write shadow values to OLATA and OLATB in one pass. */
void MCP23017Driver::_commit()
{
    if (!_i2c)
        return;
    _i2c->writeRegister(_addr, REG_OLATA, _ga);
    _i2c->writeRegister(_addr, REG_OLATB, _gb);
}
