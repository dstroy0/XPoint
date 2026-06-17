// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file XPoint.h
 * @brief XPoint crosspoint matrix connection manager.
 *
 * Maintains a logical R×C connection state table and forwards every change to
 * an XPointDriver backend.  Supports:
 * - Non-latching relays: energise to connect, de-energise to disconnect
 * - Dual-coil latching relays: non-blocking SET/RESET coil pulse management
 * - Row interlock: prevent two rows from connecting to the same column
 * - Exclusive-input columns: only one row permitted at a time
 * - Analogue level control via setLevel() for PWM-capable drivers
 *
 * **Flat array layout** (all indices zero-based):
 * | Array    | Size             | Meaning                             |
 * |----------|------------------|-------------------------------------|
 * | `_state` | rows × cols      | Connection flag per node            |
 * | `_ilock` | rows × rows      | Symmetric row-pair interlock flags  |
 * | `_excl`  | cols             | Exclusive-input column flags        |
 *
 * **Construction options:**
 * | Constructor        | Heap use | Typical target               |
 * |--------------------|----------|------------------------------|
 * | Heap constructor   | Yes      | Desktop / large MCU          |
 * | Buffer constructor | No       | Caller manages static arrays |
 * | XPointStatic<R,C>  | No       | AVR global / BSS             |
 *
 * **RAM cost per instance:**
 * - Object overhead: ~71 B on AVR (8-bit, 2-byte ptrs), ~152 B on 64-bit.
 * - State buffers: `rows*cols + rows² + cols` bytes.
 *
 * **Latching relay pulse table:**
 * - `MAX_PULSES = 8` simultaneous in-flight coil pulses.
 * - Operations beyond this limit silently drop the pulse event.
 * - Do not issue more than 8 connect/disconnect calls before calling update().
 * - connect() / disconnect() return `false` while a pulse is in-flight for
 *   the target node; call update() in each loop() iteration to clear slots.
 */

#ifndef XPOINT_H
#define XPOINT_H

#include "XPointDriver.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief Relay operating mode.
 *
 * Passed to the XPoint / XPointStatic constructor to select coil behaviour.
 */
enum RelayType
{
    RE_NON_LATCHING      = 0, ///< Energise to connect; de-energise to disconnect.
    RE_LATCHING_DUAL_COIL    ///< SET coil to connect; RESET coil to disconnect.
};

/**
 * @brief Internal pulse-event slot for non-blocking latching-relay coil timing.
 *
 * One slot is occupied from the moment a coil is energised until update()
 * detects that @p pulseDuration ms have elapsed and calls releaseNode().
 * Managed exclusively by XPoint; not part of the public API.
 */
struct PulseEvent
{
    uint8_t r;        ///< Matrix row.
    uint8_t c;        ///< Matrix column.
    unsigned long t0; ///< millis() at coil energise.
    bool on;          ///< `true` = slot occupied.
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
     * Allocates `rows*cols + rows² + cols` bytes via `new[]`; freed in the
     * destructor.  Avoid on AVR with limited SRAM; prefer XPointStatic or the
     * buffer constructor to eliminate heap use and fragmentation risk.
     *
     * @param[in] rows Number of matrix rows (1–255).
     * @param[in] cols Number of matrix columns (1–255).
     * @param[in] type Relay operating mode (default RE_NON_LATCHING).
     * @param[in] pdur Coil pulse duration in ms for RE_LATCHING_DUAL_COIL (default 0).
     */
    XPoint(uint8_t rows, uint8_t cols, RelayType type = RE_NON_LATCHING, uint16_t pdur = 0);

    /**
     * @brief Zero-heap buffer constructor.
     *
     * All three arrays must be sized as specified and must outlive this object.
     * The destructor does **not** free them.
     *
     * @param[in] rows  Number of matrix rows.
     * @param[in] cols  Number of matrix columns.
     * @param[in] state Caller-owned array of `rows * cols` bools.
     * @param[in] ilock Caller-owned array of `rows * rows` bools (symmetric interlock map).
     * @param[in] excl  Caller-owned array of `cols` bools (exclusive-input flags).
     * @param[in] type  Relay operating mode.
     * @param[in] pdur  Coil pulse duration in ms.
     */
    XPoint(uint8_t rows, uint8_t cols, bool *state, bool *ilock, bool *excl,
           RelayType type = RE_NON_LATCHING, uint16_t pdur = 0);

    /** @brief Destructor. Frees heap buffers only when constructed with the heap constructor. */
    ~XPoint();

    XPoint(const XPoint &) = delete;
    XPoint &operator=(const XPoint &) = delete;

    /**
     * @brief Attach a driver backend.
     *
     * Must be called before begin().  XPoint does **not** take ownership of @p drv.
     *
     * @param[in] drv Pointer to a concrete XPointDriver implementation.
     */
    void setDriver(XPointDriver *drv);

    /**
     * @brief Initialise hardware by calling `drv->begin()`.
     *
     * Call once after setDriver() and before any connect / disconnect calls.
     */
    void begin();

    /**
     * @brief Connect row @p row to column @p col.
     *
     * Applies interlock and exclusive-input rules before connecting.
     * For RE_LATCHING_DUAL_COIL, returns `false` while a coil pulse is
     * in-flight for this node — call update() each loop() to free pulse slots.
     *
     * @param[in] row Row index (zero-based).
     * @param[in] col Column index (zero-based).
     * @return `true`  — node is now connected (or was already connected).
     * @return `false` — out of range, blocked by interlock / exclusive rule,
     *                   or a latching-relay pulse is still in-flight.
     */
    bool connect(uint8_t row, uint8_t col);

    /**
     * @brief Disconnect row @p row from column @p col.
     *
     * For RE_LATCHING_DUAL_COIL, returns `false` while a coil pulse is
     * in-flight for this node — call update() each loop() to free pulse slots.
     *
     * @param[in] row Row index.
     * @param[in] col Column index.
     * @return `true`  — node is now disconnected (or was already disconnected).
     * @return `false` — out of range, or a latching-relay pulse is still in-flight.
     */
    bool disconnect(uint8_t row, uint8_t col);

    /**
     * @brief Analogue-level connect / disconnect (PWM drivers).
     *
     * - `level > 0`: connecting path — interlock and exclusive-input rules apply.
     * - `level == 0`: disconnecting path — rules are not checked.
     *
     * The default setNodeLevel() delegates to setNodeHardware(), so binary
     * drivers treat any non-zero level as "on" and `0` as "off".
     * PWM-capable drivers (TLC59711) use the full 16-bit range.
     *
     * @param[in] row   Row index.
     * @param[in] col   Column index.
     * @param[in] level Drive level `0x0000` (off) to `0xFFFF` (full on).
     * @return `true` on success, `false` if blocked or out of range.
     */
    bool setLevel(uint8_t row, uint8_t col, uint16_t level);

    /**
     * @brief Disconnect all connected nodes and zero the state table.
     *
     * For RE_LATCHING_DUAL_COIL, pulses the RESET coil on each connected node
     * and registers pulse events (up to MAX_PULSES; excess nodes are silently
     * skipped — their hardware timeout must de-energise any excess coils).
     * Any stale in-flight SET-coil pulses are cancelled before registering the
     * RESET pulses so freed slots remain available.
     */
    void clearAll();

    /**
     * @brief Prevent rowA and rowB from connecting to the same column simultaneously.
     *
     * The interlock is symmetric; calling `lockRows(0,1)` and `lockRows(1,0)`
     * are equivalent.  Safe to call multiple times.
     *
     * @param[in] rowA First row index.
     * @param[in] rowB Second row index.
     */
    void lockRows(uint8_t rowA, uint8_t rowB);

    /**
     * @brief Mark column @p col as exclusive: at most one row may connect at a time.
     *
     * @param[in] col Column index.
     */
    void exclusiveInput(uint8_t col);

    /**
     * @brief Expire latching-relay coil pulses and call releaseNode() as needed.
     *
     * Must be called every `loop()` iteration when using RE_LATCHING_DUAL_COIL.
     * Returns immediately (no overhead) for RE_NON_LATCHING matrices.
     * Calls `commitPhysicalUpdates()` once if any pulses were released.
     */
    void update();

  private:
    uint8_t _rows;  ///< Row count.
    uint8_t _cols;  ///< Column count.
    RelayType _type; ///< Relay operating mode.
    uint16_t _pdur; ///< Coil pulse duration in ms (RE_LATCHING_DUAL_COIL only).
    XPointDriver *_drv; ///< Attached driver backend.
    bool _owns;     ///< `true` = destructor frees _state, _ilock, _excl.

    bool *_state; ///< [_rows * _cols]  Logical connection table.
    bool *_ilock; ///< [_rows * _rows]  Row-pair interlock flags (symmetric).
    bool *_excl;  ///< [_cols]          Exclusive-input column flags.

    static const uint8_t MAX_PULSES = 8; ///< Maximum simultaneous in-flight pulses.
    PulseEvent _pulses[MAX_PULSES]; ///< Pulse event table.

    /** @brief Zero buffers and pulse table; called by both constructors. */
    void _init();
};

/**
 * @brief Zero-heap variant of XPoint with embedded state arrays.
 *
 * All three state arrays are embedded directly inside the object so no heap
 * allocation ever occurs.  Declare as a global or `static` local on AVR to
 * keep everything in BSS / data segment.
 *
 * @code
 * XPointStatic<4, 4> matrix(RE_NON_LATCHING);
 * matrix.setDriver(&myDriver);
 * matrix.begin();
 * @endcode
 *
 * @tparam ROWS Number of matrix rows (compile-time constant).
 * @tparam COLS Number of matrix columns (compile-time constant).
 */
template <uint8_t ROWS, uint8_t COLS>
class XPointStatic : public XPoint
{
    bool _stateBuf[ROWS * COLS]; ///< Embedded connection state [ROWS * COLS].
    bool _ilockBuf[ROWS * ROWS]; ///< Embedded interlock map    [ROWS * ROWS].
    bool _exclBuf[COLS];         ///< Embedded exclusive flags  [COLS].

  public:
    /**
     * @param[in] type Relay operating mode (default RE_NON_LATCHING).
     * @param[in] pdur Coil pulse duration in ms (default 0).
     */
    XPointStatic(RelayType type = RE_NON_LATCHING, uint16_t pdur = 0)
        : XPoint(ROWS, COLS, _stateBuf, _ilockBuf, _exclBuf, type, pdur)
    {
    }
};

#endif // XPOINT_H
