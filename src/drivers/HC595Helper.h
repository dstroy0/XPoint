// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file HC595Helper.h
 * @brief Utility for mapping matrix nodes to 74HC595 shift-register output bits.
 *
 * Because the driver `MapFn` signature is a plain function pointer (no closure),
 * column count must be captured as a compile-time constant in the mapper:
 *
 * @code
 * static const uint8_t COLS = 4;
 * static uint16_t myMapper(uint8_t r, uint8_t c) {
 *     return HC595Helper::rowMajorIndex(r, c, COLS);
 * }
 * @endcode
 */

#ifndef HC595_HELPER_H
#define HC595_HELPER_H

#include <stdint.h>

/**
 * @brief Utilities for 74HC595 daisy-chain index computation.
 */
namespace HC595Helper
{

/**
 * @brief Compute the row-major bit index for node (row, col).
 *
 * Maps a 2-D matrix position to a flat bit index for a 74HC595 chain wired
 * in row-major order (row 0 bits first, then row 1, etc.).
 *
 * @param[in] row  Row index (zero-based).
 * @param[in] col  Column index (zero-based).
 * @param[in] cols Total number of columns in the matrix.
 * @return Flat bit index: `row * cols + col`.
 */
inline uint16_t rowMajorIndex(uint8_t row, uint8_t col, uint8_t cols)
{
    return (uint16_t)(row * cols + col);
}

} // namespace HC595Helper

#endif // HC595_HELPER_H
