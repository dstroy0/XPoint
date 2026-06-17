// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "XPoint.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
extern unsigned long millis();
#endif

/* --------------------------------------------------------------------------
 * Construction / destruction
 * -------------------------------------------------------------------------- */

XPoint::XPoint(uint8_t rows, uint8_t cols, RelayType type, uint16_t pdur)
    : _rows(rows), _cols(cols), _type(type), _pdur(pdur), _drv(nullptr), _owns(true), _state(nullptr), _ilock(nullptr),
      _excl(nullptr)
{
    if (rows == 0 || cols == 0)
        return;
    _state = new bool[(size_t)rows * cols];
    _ilock = new bool[(size_t)rows * rows];
    _excl = new bool[cols];
    _init();
}

XPoint::XPoint(uint8_t rows, uint8_t cols, bool *state, bool *ilock, bool *excl, RelayType type, uint16_t pdur)
    : _rows(rows), _cols(cols), _type(type), _pdur(pdur), _drv(nullptr), _owns(false), _state(state), _ilock(ilock),
      _excl(excl)
{
    _init();
}

XPoint::~XPoint()
{
    if (_owns)
    {
        delete[] _state;
        delete[] _ilock;
        delete[] _excl;
    }
}

/* Zero all buffers and clear the pulse table.
 * Called by both constructors after pointers are set. */
void XPoint::_init()
{
    if (_state)
        memset(_state, 0, (size_t)_rows * _cols * sizeof(bool));
    if (_ilock)
        memset(_ilock, 0, (size_t)_rows * _rows * sizeof(bool));
    if (_excl)
        memset(_excl, 0, (size_t)_cols * sizeof(bool));

    for (uint8_t i = 0; i < MAX_PULSES; ++i)
        _pulses[i] = {0, 0, 0UL, false};
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
    if (!_state || row >= _rows || col >= _cols)
        return false;

    /* Interlock: refuse if any peer locked against this row already holds col. */
    for (uint8_t r = 0; r < _rows; ++r)
    {
        if (r == row)
            continue;
        if (_ilock[row * _rows + r] && _state[r * _cols + col])
            return false;
    }

    /* Exclusive column: refuse if any other row already holds this column. */
    if (_excl[col])
    {
        for (uint8_t r = 0; r < _rows; ++r)
            if (r != row && _state[r * _cols + col])
                return false;
    }

    if (_state[row * _cols + col])
        return true; // already connected - idempotent

    /* Latching: refuse if a coil pulse is still in-flight for this node.
     * Caller must wait until update() fires releaseNode() before re-trying. */
    if (_type == RE_LATCHING_DUAL_COIL)
    {
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
            if (_pulses[i].on && _pulses[i].r == row && _pulses[i].c == col)
                return false;
    }

    _state[row * _cols + col] = true;

    if (_drv)
    {
        _drv->setNodeHardware(row, col, true); // true = energise / SET coil
        _drv->commitPhysicalUpdates();
    }

    if (_type == RE_LATCHING_DUAL_COIL)
    {
        unsigned long now = millis();
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
        {
            if (!_pulses[i].on)
            {
                _pulses[i] = {row, col, now, true};
                break;
            }
        }
    }
    return true;
}

bool XPoint::disconnect(uint8_t row, uint8_t col)
{
    if (!_state || row >= _rows || col >= _cols)
        return false;
    if (!_state[row * _cols + col])
        return true; // already disconnected - idempotent

    /* Latching: refuse if a coil pulse is still in-flight for this node.
     * Caller must wait until update() fires releaseNode() before re-trying. */
    if (_type == RE_LATCHING_DUAL_COIL)
    {
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
            if (_pulses[i].on && _pulses[i].r == row && _pulses[i].c == col)
                return false;
    }

    _state[row * _cols + col] = false;

    if (_type == RE_LATCHING_DUAL_COIL)
    {
        if (_drv)
        {
            _drv->setNodeHardware(row, col, false); // false = energise RESET coil
            _drv->commitPhysicalUpdates();
        }
        unsigned long now = millis();
        for (uint8_t i = 0; i < MAX_PULSES; ++i)
        {
            if (!_pulses[i].on)
            {
                _pulses[i] = {row, col, now, true};
                break;
            }
        }
    }
    else
    {
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
    if (!_state || row >= _rows || col >= _cols)
        return false;

    bool on = (level > 0);

    if (on)
    {
        /* Apply protections on the connecting path only; disconnects always allowed. */
        for (uint8_t r = 0; r < _rows; ++r)
        {
            if (r == row)
                continue;
            if (_ilock[row * _rows + r] && _state[r * _cols + col])
                return false;
        }
        if (_excl[col])
        {
            for (uint8_t r = 0; r < _rows; ++r)
                if (r != row && _state[r * _cols + col])
                    return false;
        }
    }

    _state[row * _cols + col] = on;

    if (_drv)
    {
        _drv->setNodeLevel(row, col, level);
        _drv->commitPhysicalUpdates();
    }
    return true;
}

void XPoint::clearAll()
{
    if (!_state)
        return;

    unsigned long now = (_type == RE_LATCHING_DUAL_COIL) ? millis() : 0UL;

    for (uint8_t r = 0; r < _rows; ++r)
    {
        for (uint8_t c = 0; c < _cols; ++c)
        {
            if (!_state[r * _cols + c])
                continue; // skip already-off nodes

            _state[r * _cols + c] = false;

            if (_drv)
                _drv->setNodeHardware(r, c, false);

            if (_type == RE_LATCHING_DUAL_COIL)
            {
                /* Cancel any stale SET-coil pulse for this node so the slot
                 * is freed and the stale release cannot cut the RESET pulse short. */
                for (uint8_t i = 0; i < MAX_PULSES; ++i)
                    if (_pulses[i].on && _pulses[i].r == r && _pulses[i].c == c)
                        _pulses[i].on = false;

                /* Register RESET pulse so releaseNode() fires after _pdur ms.
                 * Nodes beyond MAX_PULSES=8 are silently skipped. */
                for (uint8_t i = 0; i < MAX_PULSES; ++i)
                {
                    if (!_pulses[i].on)
                    {
                        _pulses[i] = {r, c, now, true};
                        break;
                    }
                }
            }
        }
    }

    if (_drv)
        _drv->commitPhysicalUpdates();
}

void XPoint::lockRows(uint8_t rowA, uint8_t rowB)
{
    if (!_ilock || rowA >= _rows || rowB >= _rows)
        return;
    /* Symmetric: set both directions so connect() only needs to check one half. */
    _ilock[rowA * _rows + rowB] = true;
    _ilock[rowB * _rows + rowA] = true;
}

void XPoint::exclusiveInput(uint8_t col)
{
    if (!_excl || col >= _cols)
        return;
    _excl[col] = true;
}

/* --------------------------------------------------------------------------
 * Pulse management (RE_LATCHING_DUAL_COIL only)
 * -------------------------------------------------------------------------- */

void XPoint::update()
{
    if (_type != RE_LATCHING_DUAL_COIL)
        return;

    unsigned long now = millis();
    bool any = false;

    for (uint8_t i = 0; i < MAX_PULSES; ++i)
    {
        if (!_pulses[i].on)
            continue;
        /* Unsigned subtraction handles millis() 32-bit rollover correctly. */
        if ((now - _pulses[i].t0) >= _pdur)
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
