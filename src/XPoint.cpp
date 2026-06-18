// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "XPoint.h"

#if defined(ARDUINO)
#include <Arduino.h>
static inline uint32_t _millis()
{
    return (uint32_t)millis();
}
#else
extern uint32_t millis();
static inline uint32_t _millis()
{
    return millis();
}
#endif

/* --------------------------------------------------------------------------
 * Construction / destruction
 * -------------------------------------------------------------------------- */

XPoint::XPoint(uint8_t rows, uint8_t cols, RelayType type, uint16_t pdur)
    : _rows(rows), _cols(cols), _type(type), _pdur(pdur), _drv(nullptr),
      _pool((uint32_t)rows * cols + (uint32_t)rows * rows + cols)
{
    _init();
}

XPoint::XPoint(uint8_t rows, uint8_t cols, uint32_t *pool, RelayType type, uint16_t pdur)
    : _rows(rows), _cols(cols), _type(type), _pdur(pdur), _drv(nullptr),
      _pool(pool, (uint32_t)rows * cols + (uint32_t)rows * rows + cols)
{
    _init();
}

XPoint::~XPoint()
{
}

void XPoint::_init()
{
    _pool.clear();
    for (uint8_t i = 0; i < MAX_PULSES; ++i)
        _pulses[i] = {0, 0, 0u, false};
}

/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

bool XPoint::_pulsePending(uint8_t row, uint8_t col) const
{
    for (uint8_t i = 0; i < MAX_PULSES; ++i)
        if (_pulses[i].on && _pulses[i].r == row && _pulses[i].c == col)
            return true;
    return false;
}

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

void XPoint::setDriver(XPointDriver *drv)
{
    _drv = drv;
}

void XPoint::begin()
{
    if (_drv)
        _drv->begin();
}

/* --------------------------------------------------------------------------
 * Matrix operations
 * -------------------------------------------------------------------------- */

bool XPoint::connect(uint8_t row, uint8_t col)
{
    if (!_pool.valid() || row >= _rows || col >= _cols)
        return false;

    uint32_t ibase = _ilockOff();
    uint32_t ebase = _exclOff();

    for (uint8_t r = 0; r < _rows; ++r)
    {
        if (r == row)
            continue;
        if (_pool.get(ibase + (uint32_t)row * _rows + r) &&
            (_pool.get((uint32_t)r * _cols + col) || _pulsePending(r, col)))
            return false;
    }

    if (_pool.get(ebase + col))
    {
        for (uint8_t r = 0; r < _rows; ++r)
            if (r != row && (_pool.get((uint32_t)r * _cols + col) || _pulsePending(r, col)))
                return false;
    }

    if (_pool.get((uint32_t)row * _cols + col))
        return true; // already connected

    if (_type == RE_LATCHING_DUAL_COIL)
    {
        uint8_t freeSlot = MAX_PULSES;
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
        {
            if (_pulses[i].on)
            {
                if (_pulses[i].r == row && _pulses[i].c == col)
                    return false;
            }
            else if (freeSlot == MAX_PULSES)
            {
                freeSlot = i;
            }
        }
        if (freeSlot == MAX_PULSES)
            return false;

        _pool.set((uint32_t)row * _cols + col, true);
        if (_drv)
        {
            _drv->setNodeHardware(row, col, true);
            _drv->commitPhysicalUpdates();
        }
        _pulses[freeSlot] = {row, col, _millis(), true};
    }
    else
    {
        _pool.set((uint32_t)row * _cols + col, true);
        if (_drv)
        {
            _drv->setNodeHardware(row, col, true);
            _drv->commitPhysicalUpdates();
        }
    }
    return true;
}

bool XPoint::disconnect(uint8_t row, uint8_t col)
{
    if (!_pool.valid() || row >= _rows || col >= _cols)
        return false;

    uint32_t bit = (uint32_t)row * _cols + col;

    if (!_pool.get(bit))
        return true; // already disconnected

    if (_type == RE_LATCHING_DUAL_COIL)
    {
        uint8_t freeSlot = MAX_PULSES;
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
        {
            if (_pulses[i].on)
            {
                if (_pulses[i].r == row && _pulses[i].c == col)
                    return false;
            }
            else if (freeSlot == MAX_PULSES)
            {
                freeSlot = i;
            }
        }
        if (freeSlot == MAX_PULSES)
            return false;

        _pool.set(bit, false);
        if (_drv)
        {
            _drv->setNodeHardware(row, col, false);
            _drv->commitPhysicalUpdates();
        }
        _pulses[freeSlot] = {row, col, _millis(), true};
    }
    else
    {
        _pool.set(bit, false);
        if (_drv)
        {
            _drv->setNodeHardware(row, col, false);
            _drv->commitPhysicalUpdates();
        }
    }
    return true;
}

bool XPoint::setLevel(uint8_t row, uint8_t col, uint16_t level)
{
    if (!_pool.valid() || row >= _rows || col >= _cols)
        return false;

    bool on = (level > 0);

    if (on)
    {
        uint32_t ibase = _ilockOff();
        uint32_t ebase = _exclOff();

        for (uint8_t r = 0; r < _rows; ++r)
        {
            if (r == row)
                continue;
            if (_pool.get(ibase + (uint32_t)row * _rows + r) &&
                (_pool.get((uint32_t)r * _cols + col) || _pulsePending(r, col)))
                return false;
        }
        if (_pool.get(ebase + col))
        {
            for (uint8_t r = 0; r < _rows; ++r)
                if (r != row && (_pool.get((uint32_t)r * _cols + col) || _pulsePending(r, col)))
                    return false;
        }
    }

    uint32_t bit = (uint32_t)row * _cols + col;

    if (_type == RE_LATCHING_DUAL_COIL)
    {
        if (_pool.get(bit) == on)
            return true; // idempotent

        uint8_t freeSlot = MAX_PULSES;
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
        {
            if (_pulses[i].on)
            {
                if (_pulses[i].r == row && _pulses[i].c == col)
                    return false;
            }
            else if (freeSlot == MAX_PULSES)
            {
                freeSlot = i;
            }
        }
        if (freeSlot == MAX_PULSES)
            return false;

        _pool.set(bit, on);
        if (_drv)
        {
            _drv->setNodeHardware(row, col, on);
            _drv->commitPhysicalUpdates();
        }
        _pulses[freeSlot] = {row, col, _millis(), true};
    }
    else
    {
        _pool.set(bit, on);
        if (_drv)
        {
            _drv->setNodeLevel(row, col, level);
            _drv->commitPhysicalUpdates();
        }
    }
    return true;
}

void XPoint::clearAll()
{
    if (!_pool.valid())
        return;

    uint32_t now = (_type == RE_LATCHING_DUAL_COIL) ? _millis() : 0u;

    for (uint8_t r = 0; r < _rows; ++r)
    {
        for (uint8_t c = 0; c < _cols; ++c)
        {
            uint32_t bit = (uint32_t)r * _cols + c;
            if (!_pool.get(bit))
                continue;

            _pool.set(bit, false);

            if (_type == RE_LATCHING_DUAL_COIL)
            {
                uint8_t freeSlot = MAX_PULSES;
                for (uint8_t i = 0; i < MAX_PULSES; ++i)
                {
                    if (_pulses[i].on && _pulses[i].r == r && _pulses[i].c == c)
                    {
                        _pulses[i].on = false;
                        freeSlot = i;
                    }
                    else if (!_pulses[i].on && freeSlot == MAX_PULSES)
                    {
                        freeSlot = i;
                    }
                }
                if (freeSlot < MAX_PULSES)
                {
                    if (_drv)
                        _drv->setNodeHardware(r, c, false);
                    _pulses[freeSlot] = {r, c, now, true};
                }
            }
            else
            {
                if (_drv)
                    _drv->setNodeHardware(r, c, false);
            }
        }
    }

    if (_drv)
        _drv->commitPhysicalUpdates();
}

void XPoint::lockRows(uint8_t rowA, uint8_t rowB)
{
    if (!_pool.valid() || rowA >= _rows || rowB >= _rows)
        return;
    uint32_t base = _ilockOff();
    _pool.set(base + (uint32_t)rowA * _rows + rowB, true);
    _pool.set(base + (uint32_t)rowB * _rows + rowA, true);
}

void XPoint::exclusiveInput(uint8_t col)
{
    if (!_pool.valid() || col >= _cols)
        return;
    _pool.set(_exclOff() + col, true);
}

/* --------------------------------------------------------------------------
 * Pulse management (RE_LATCHING_DUAL_COIL only)
 * -------------------------------------------------------------------------- */

void XPoint::update()
{
    if (_type != RE_LATCHING_DUAL_COIL)
        return;

    uint32_t now = _millis();
    bool any = false;

    for (uint8_t i = 0; i < MAX_PULSES; ++i)
    {
        if (!_pulses[i].on)
            continue;
        if ((now - _pulses[i].t0) >= (uint32_t)_pdur)
        {
            if (_drv)
                _drv->releaseNode(_pulses[i].r, _pulses[i].c);
            _pulses[i].on = false;
            any = true;
        }
    }

    if (any && _drv)
        _drv->commitPhysicalUpdates();
}
