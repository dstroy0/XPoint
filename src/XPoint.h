// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file XPoint.h
 * @brief XPoint crosspoint matrix connection manager.
 *
 * Maintains a logical R×C connection state table and forwards every change to
 * an XPointDriver backend.  Supports:
 * - Non-latching relays: energize to connect, de-energize to disconnect
 * - Dual-coil latching relays: non-blocking SET/RESET coil pulse management
 * - Row interlock: prevent two rows from connecting to the same column
 * - Exclusive-input columns: only one row permitted at a time
 * - Analog level control via setLevel() for PWM-capable drivers
 *
 * **Bit-pool layout** — three sections packed into one flat uint32_t array
 * managed by a BitPool instance:
 *
 * | Section | Bit offset              | Bit count   |
 * |---------|-------------------------|-------------|
 * | state   | 0                       | rows × cols |
 * | ilock   | rows × cols             | rows × rows |
 * | excl    | rows × cols + rows²     | cols        |
 *
 * Total words = ceil((rows×cols + rows² + cols) / 32).
 * For an 8×8 matrix: 64 + 64 + 8 = 136 bits → 5 uint32_t (vs 136 bool bytes).
 *
 * **Construction options:**
 * | Constructor        | Heap use | Typical target               |
 * |--------------------|----------|------------------------------|
 * | Heap constructor   | Yes      | Desktop / large MCU          |
 * | Pool constructor   | No       | Caller manages pool buffer   |
 * | XPointStatic<R,C>  | No       | AVR global / BSS             |
 *
 * **Latching relay pulse table:**
 * - `MAX_PULSES = 8` simultaneous in-flight coil pulses.
 * - connect() / disconnect() return `false` while a pulse is in-flight for
 *   the target node; call update() in each loop() iteration to clear slots.
 */

#ifndef XPOINT_H
#define XPOINT_H

#include "drivers/BitPool.h"
#include "drivers/XPointDriver.h"
#include <stdint.h>

/**
 * @brief Relay operating mode.
 */
enum RelayType
{
    RE_NON_LATCHING = 0,  ///< Energize to connect; de-energize to disconnect.
    RE_LATCHING_DUAL_COIL ///< SET coil to connect; RESET coil to disconnect.
};

/**
 * @brief Internal pulse-event slot for non-blocking latching-relay coil timing.
 *
 * @note t0 is uint32_t to enforce 32-bit timer semantics and correct rollover
 *       on all targets including 64-bit hosts.
 */
struct XPOINT_PACKED PulseEvent
{
    uint8_t r;   ///< Matrix row.
    uint8_t c;   ///< Matrix column.
    uint32_t t0; ///< millis() at coil energize; wraps at ~49 days.
    bool on;     ///< true = slot occupied.
};

/**
 * @brief Hardware-agnostic crosspoint matrix connection manager.
 *
 * @see XPointStatic for the zero-heap template variant.
 */
class XPoint
{
  public:
    /**
     * @brief Heap-allocating constructor.
     *
     * Allocates one BitPool large enough for state, ilock, and excl sections;
     * freed in the destructor.
     *
     * @param[in] rows Number of matrix rows (1–255).
     * @param[in] cols Number of matrix columns (1–255).
     * @param[in] type Relay operating mode.
     * @param[in] pdur Coil pulse duration in ms (RE_LATCHING_DUAL_COIL only).
     */
    XPoint(uint8_t rows, uint8_t cols, RelayType type = RE_NON_LATCHING, uint16_t pdur = 0);

    /**
     * @brief Zero-heap pool constructor.
     *
     * @p pool must point to at least poolWords(rows, cols) uint32_t elements
     * and must outlive this object.  The destructor does not free it.
     *
     * @param[in] rows Number of matrix rows.
     * @param[in] cols Number of matrix columns.
     * @param[in] pool Caller-owned word array; use poolWords() to size it.
     * @param[in] type Relay operating mode.
     * @param[in] pdur Coil pulse duration in ms.
     */
    XPoint(uint8_t rows, uint8_t cols, uint32_t *pool, RelayType type = RE_NON_LATCHING, uint16_t pdur = 0);

    /** @brief Destructor. Frees pool only when built with the heap constructor. */
    ~XPoint();

    XPoint(const XPoint &) = delete;
    XPoint &operator=(const XPoint &) = delete;

    /**
     * @brief Number of uint32_t words needed for a pool of the given dimensions.
     *
     * Use this to size the array passed to the pool constructor or XPointStatic.
     *
     * @param[in] rows Row count.
     * @param[in] cols Column count.
     * @return Word count, or 0 for degenerate (0-dim) matrices.
     */
    static uint16_t poolWords(uint8_t rows, uint8_t cols)
    {
        return BitPool::wordsFor((uint32_t)rows * cols + (uint32_t)rows * rows + cols);
    }

    /** @brief Attach a driver backend. Must be called before begin(). */
    void setDriver(XPointDriver *drv);

    /** @brief Initialize hardware via drv->begin(). Call once after setDriver(). */
    void begin();

    /**
     * @brief Connect row to column.
     *
     * Applies interlock and exclusive-input rules.  For RE_LATCHING_DUAL_COIL
     * returns false while a coil pulse is in-flight for this node.
     *
     * @return true if connected (or was already connected), false if blocked.
     */
    bool connect(uint8_t row, uint8_t col);

    /**
     * @brief Disconnect row from column.
     *
     * For RE_LATCHING_DUAL_COIL returns false while a coil pulse is in-flight.
     *
     * @return true if disconnected (or was already disconnected), false if blocked.
     */
    bool disconnect(uint8_t row, uint8_t col);

    /**
     * @brief Analog-level connect / disconnect for PWM drivers.
     *
     * level > 0: connecting path — interlock and exclusive rules apply.
     * level == 0: disconnecting path — rules are bypassed.
     *
     * @return true on success, false if blocked or out of range.
     */
    bool setLevel(uint8_t row, uint8_t col, uint16_t level);

    /**
     * @brief Disconnect all nodes, zeroing the state section of the pool.
     *
     * For RE_LATCHING_DUAL_COIL pulses the RESET coil on each connected node.
     * In-flight SET pulses are cancelled before registering RESET pulses so
     * freed slots remain available. Nodes beyond MAX_PULSES are silently skipped.
     */
    void clearAll();

    /**
     * @brief Prevent rowA and rowB from sharing a column simultaneously.
     *
     * Symmetric; lockRows(0,1) and lockRows(1,0) are equivalent.
     */
    void lockRows(uint8_t rowA, uint8_t rowB);

    /** @brief Mark column col as exclusive: at most one row may connect at a time. */
    void exclusiveInput(uint8_t col);

    /**
     * @brief Expire latching-relay coil pulses and call releaseNode() as needed.
     *
     * Must be called every loop() iteration when using RE_LATCHING_DUAL_COIL.
     * No-op for RE_NON_LATCHING matrices.
     */
    void update();

  private:
    uint8_t _rows;
    uint8_t _cols;
    RelayType _type;
    uint16_t _pdur;
    XPointDriver *_drv;
    BitPool _pool; ///< Flat bit pool: [state | ilock | excl].

    static const uint8_t MAX_PULSES = 8;
    PulseEvent _pulses[MAX_PULSES];

    /* Section offsets — computed from _rows/_cols, never stored. */
    uint32_t _ilockOff() const
    {
        return (uint32_t)_rows * _cols;
    }
    uint32_t _exclOff() const
    {
        return (uint32_t)_rows * _cols + (uint32_t)_rows * _rows;
    }

    void _init();
    bool _pulsePending(uint8_t row, uint8_t col) const;
};

/**
 * @brief Zero-heap XPoint with a compile-time-sized embedded bit pool.
 *
 * Declare as a global or static local to keep everything in BSS/data.
 *
 * @code
 * XPointStatic<4, 4> matrix(RE_NON_LATCHING);
 * matrix.setDriver(&myDriver);
 * matrix.begin();
 * @endcode
 *
 * @tparam ROWS Row count (compile-time constant, 1–255).
 * @tparam COLS Column count (compile-time constant, 1–255).
 */
template <uint8_t ROWS, uint8_t COLS> class XPointStatic : public XPoint
{
    static const uint16_t WORDS = (uint16_t)(((uint32_t)ROWS * COLS + (uint32_t)ROWS * ROWS + COLS + 31u) / 32u);

    uint32_t _buf[WORDS > 0u ? WORDS : 1u];

  public:
    /**
     * @param[in] type Relay operating mode (default RE_NON_LATCHING).
     * @param[in] pdur Coil pulse duration in ms (default 0).
     */
    XPointStatic(RelayType type = RE_NON_LATCHING, uint16_t pdur = 0) : XPoint(ROWS, COLS, _buf, type, pdur)
    {
    }
};

#endif // XPOINT_H
