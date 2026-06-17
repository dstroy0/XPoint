// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "ArduinoDirectGPIODriver.h"

#if defined(ARDUINO)

ArduinoDirectGPIODriver::ArduinoDirectGPIODriver(uint8_t rows, uint8_t cols, MapFn map, uint8_t maxPin)
    : _rows(rows), _cols(cols), _map(map), _maxPin(maxPin)
{
}

void ArduinoDirectGPIODriver::begin()
{
    /* Configure only the pins the mapper actually uses; avoids clobbering
     * special-purpose pins (RX/TX, SPI, etc.) that a blanket pass would hit. */
    for (uint8_t r = 0; r < _rows; ++r)
    {
        for (uint8_t c = 0; c < _cols; ++c)
        {
            uint8_t pin = _map(r, c);
            if (pin <= _maxPin)
            {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
            }
        }
    }
}

void ArduinoDirectGPIODriver::setNodeHardware(uint8_t r, uint8_t c, bool state)
{
    uint8_t pin = _map(r, c);
    if (pin > _maxPin)
        return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);
}

#else // host build — no-ops

ArduinoDirectGPIODriver::ArduinoDirectGPIODriver(uint8_t, uint8_t, MapFn, uint8_t)
    : _rows(0), _cols(0), _map(nullptr), _maxPin(0)
{
}

void ArduinoDirectGPIODriver::begin()
{
}

void ArduinoDirectGPIODriver::setNodeHardware(uint8_t, uint8_t, bool)
{
}

#endif
