// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/*
 * XPoint unit test suite — host-native C++11 build; no Arduino hardware required.
 *
 * Run:
 *   g++ -std=c++11 -Isrc -Itest \
 *       test/test_xpoint.cpp \
 *       src/XPoint.cpp src/drivers/ShiftRegisterDriver.cpp \
 *       src/drivers/DirectGPIODriver.cpp src/drivers/MCP23017Driver.cpp \
 *       src/drivers/TLC59711Driver.cpp \
 *       -o test_xpoint && ./test_xpoint
 *
 * millis() is stubbed as uint32_t to match the intended 32-bit Arduino target.
 * All timing tests are immune to rollover by construction.
 */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../src/XPoint.h"
#include "../src/drivers/DirectGPIODriver.h"
#include "../src/drivers/I2CInterface.h"
#include "../src/drivers/MCP23017Driver.h"
#include "../src/drivers/ShiftRegisterDriver.h"
#include "../src/drivers/TCA9548AInterface.h"
#include "../src/drivers/TLC59711Driver.h"
#include "../src/drivers/XPointDriver.h"
#include "Arduino.h"
#include "json_reader.h"

/* ---- Test categories (bitmask) -------------------------------------------
 * Pass --skip-<name> on the command line to disable a category.
 * --skip-range skips the slow 0×0–255×255 full-matrix sweep.
 * -------------------------------------------------------------------------*/
enum TestCat : uint8_t
{
    CAT_EXISTING = 1u << 0,
    CAT_LIMIT    = 1u << 1,
    CAT_RANGE    = 1u << 2,
    CAT_GAP      = 1u << 3,
    CAT_JSON     = 1u << 4,
    CAT_ALL      = 0x1Fu,
    CAT_SIZES    = 1u << 5  // opt-in with --sizes; not in CAT_ALL
};

static const char *cat_name(uint8_t c)
{
    switch (c)
    {
    case CAT_EXISTING:
        return "existing";
    case CAT_LIMIT:
        return "limit";
    case CAT_RANGE:
        return "range";
    case CAT_GAP:
        return "gap";
    case CAT_JSON:
        return "json";
    case CAT_SIZES:
        return "sizes";
    default:
        return "?";
    }
}

/* ---- Tee — writes to stdout and a file simultaneously --------------------*/
class Tee
{
    std::ofstream _f;

  public:
    explicit Tee(const std::string &path) : _f(path.c_str())
    {
    }

    template <typename T> Tee &operator<<(const T &v)
    {
        std::cout << v;
        if (_f.is_open())
            _f << v;
        return *this;
    }

    bool ok() const
    {
        return _f.is_open();
    }
};

/* ---- millis() shim --------------------------------------------------------
 * uint32_t — mirrors Arduino's 32-bit timer and enforces rollover semantics
 * even when built on a 64-bit host.
 * -------------------------------------------------------------------------*/
static uint32_t g_millis = 0;

uint32_t millis()
{
    return g_millis;
}

void delay(uint32_t ms)
{
    g_millis += ms;
}

static void advanceMillis(uint32_t ms)
{
    g_millis += ms;
}

/* ---- Test data path -------------------------------------------------------
 * Resolve "test/data/<file>" relative to this source file so tests work
 * regardless of the working directory the binary is invoked from.
 * -------------------------------------------------------------------------*/
static std::string data_path(const std::string &filename)
{
    std::string f(__FILE__);
    std::size_t pos = f.rfind('/');
    std::size_t posw = f.rfind('\\');
    if (posw != std::string::npos && (pos == std::string::npos || posw > pos))
        pos = posw;
    if (pos == std::string::npos)
        return "test/data/" + filename;
    return f.substr(0, pos + 1) + "data/" + filename;
}

/* ---- Mock I2C ------------------------------------------------------------- */

struct I2CWrite
{
    uint8_t addr;
    uint8_t reg;
    uint8_t val;
};

class MockI2C : public I2CInterface
{
  public:
    std::vector<I2CWrite> writes;

    void writeRegister(uint8_t addr, uint8_t reg, uint8_t val) override
    {
        writes.push_back(I2CWrite{addr, reg, val});
    }
};

/* ---- Mock driver ---------------------------------------------------------- */

struct Call
{
    uint8_t r;
    uint8_t c;
    bool state;
};

class MockDriver : public XPointDriver
{
  public:
    std::vector<Call> calls;
    std::vector<std::pair<uint8_t, uint8_t>> releases;
    int commits = 0;

    void begin() override
    {
    }

    void setNodeHardware(uint8_t r, uint8_t c, bool state) override
    {
        calls.push_back(Call{r, c, state});
    }

    void releaseNode(uint8_t r, uint8_t c) override
    {
        releases.push_back({r, c});
    }

    void commitPhysicalUpdates() override
    {
        ++commits;
    }
};

/* ==========================================================================
 * EXISTING TESTS (17)
 * ========================================================================*/

/* XPointStatic<R,C>: identical public API to XPoint; zero heap allocation. */
static bool test_xpointstatic_basic()
{
    MockDriver drv;
    XPointStatic<3, 3> m(RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    m.lockRows(0, 1);
    bool ok1 = m.connect(0, 0);
    bool ok2 = m.connect(1, 0); // blocked by interlock

    if (!ok1 || ok2)
        return false;

    m.disconnect(0, 0);
    if (drv.calls.empty() || drv.calls.back().state != false)
        return false;

    return true;
}

/* Pool constructor: caller supplies the bit-pool buffer; XPoint owns no heap. */
static bool test_user_buffer_constructor()
{
    // 4×4 matrix: poolWords(4,4) = ceil((16+16+4)/32) = ceil(36/32) = 2
    uint32_t pool[2] = {};

    MockDriver drv;
    XPoint m(4, 4, pool, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    if (!m.connect(1, 2))
        return false;
    if (drv.calls.size() != 1 || !drv.calls[0].state)
        return false;

    m.disconnect(1, 2);
    if (drv.calls.size() != 2 || drv.calls[1].state != false)
        return false;

    // Verify the state bit is clear in the pool.
    // state bit for (1,2) = 1*4+2 = 6; word 0, bit 6.
    if ((pool[0] >> 6) & 1u)
        return false;

    return true;
}

/* I2CInterface::begin() must be callable through a base pointer (default no-op). */
static bool test_i2c_interface_begin()
{
    MockI2C i2c;
    I2CInterface *iface = &i2c;
    iface->begin();
    return true;
}

/* Latching relay: connect() pulses SET coil; update() calls releaseNode() after _pdur. */
static bool test_latching_connect_pulse()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 10);
    m.setDriver(&drv);
    m.begin();

    if (!m.connect(0, 1))
        return false;

    if (drv.calls.size() != 1 || !drv.calls[0].state)
        return false;
    if (!drv.releases.empty())
        return false;

    advanceMillis(20);
    m.update();

    if (drv.releases.size() != 1)
        return false;
    if (drv.releases[0].first != 0 || drv.releases[0].second != 1)
        return false;
    if (drv.calls.size() != 1)
        return false;

    return true;
}

/* Latching relay: disconnect() pulses RESET coil (state=false); update() calls releaseNode(). */
static bool test_latching_disconnect_pulse()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 10);
    m.setDriver(&drv);
    m.begin();

    m.connect(0, 1);
    advanceMillis(20);
    m.update();

    drv.calls.clear();
    drv.releases.clear();
    g_millis = 100;

    if (!m.disconnect(0, 1))
        return false;

    if (drv.calls.size() != 1 || drv.calls[0].state != false)
        return false;

    advanceMillis(20);
    m.update();

    if (drv.releases.size() != 1)
        return false;
    if (drv.calls.size() != 1)
        return false;

    return true;
}

/* Latching relay: disconnect() while SET pulse still in-flight must return false.
 * After update() expires the slot, disconnect() must succeed. */
static bool test_latching_rapid_connect_disconnect()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 20);
    m.setDriver(&drv);
    m.begin();

    g_millis = 0;
    m.connect(0, 0);

    advanceMillis(5);
    if (m.disconnect(0, 0)) // must return false — coil still pulsing
        return false;

    advanceMillis(20);
    m.update();
    if (drv.releases.size() != 1)
        return false;

    if (!m.disconnect(0, 0))
        return false;

    return true;
}

/* Non-latching: disconnect() must produce exactly one setNodeHardware(false) call. */
static bool test_nonlatching_disconnect_no_spurious_call()
{
    MockDriver drv;
    XPoint m(2, 2, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    m.connect(0, 0);
    drv.calls.clear();

    m.disconnect(0, 0);
    if (drv.calls.size() != 1 || drv.calls[0].state != false)
        return false;

    return true;
}

/* lockRows() prevents two rows from connecting to the same column simultaneously. */
static bool test_interlock()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.lockRows(0, 1);

    bool ok1 = m.connect(0, 0);
    bool ok2 = m.connect(1, 0); // blocked

    return ok1 && !ok2;
}

/* exclusiveInput(): only one row may connect to a marked column at a time. */
static bool test_exclusive()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.exclusiveInput(2);

    bool ok1 = m.connect(0, 2);
    bool ok2 = m.connect(1, 2); // blocked

    return ok1 && !ok2;
}

/* MCP23017: begin() must configure IODIR before driving OLAT. */
static bool test_mcp23017_driver()
{
    MockI2C i2c;
    auto mapper = [](uint8_t r, uint8_t c) -> uint8_t { return (uint8_t)(r * 4 + c); };
    MCP23017Driver md(&i2c, 0x20, mapper);
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&md);
    m.begin();

    bool sawIODIRA = false, sawIODIRB = false;
    for (auto &w : i2c.writes)
    {
        if (w.reg == 0x00 && w.val == 0x00)
            sawIODIRA = true;
        if (w.reg == 0x01 && w.val == 0x00)
            sawIODIRB = true;
    }
    if (!sawIODIRA || !sawIODIRB)
        return false;

    m.connect(1, 2); // pin = 1*4+2 = 6 → OLATA bit 6
    bool sawGPIOWrite = false;
    for (auto &w : i2c.writes)
        if (w.reg == 0x14 && (w.val & (1 << 6)))
            sawGPIOWrite = true;

    return sawGPIOWrite;
}

/* DirectGPIODriver: basic pin-state tracking via the pinState() accessor. */
static bool test_direct_gpio_driver()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 4 + c); };
    DirectGPIODriver drv(16, mapper);
    drv.begin();

    drv.setNodeHardware(1, 2, true);
    if (!drv.pinState(6))
        return false; // 1*4+2 = 6

    drv.setNodeHardware(1, 2, false);
    if (drv.pinState(6))
        return false;

    return true;
}

/* ShiftRegisterDriver: byte packing and shadow buffer read-back via byteAt(). */
static bool test_shift_register_driver()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 4 + c); };
    ShiftRegisterDriver drv(16, mapper);

    drv.setNodeHardware(0, 7, true); // index 7 → byte 0, bit 7
    if (drv.byteAt(0) != 0x80)
        return false;

    drv.setNodeHardware(1, 0, true); // index 4 → byte 0, bit 4
    if (drv.byteAt(0) != 0x90)
        return false;

    drv.setNodeHardware(0, 7, false);
    if (drv.byteAt(0) != 0x10)
        return false;

    return true;
}

/* TLC59711: packet assembly — correct header, GS11-first channel order. */
static bool test_tlc59711_packet()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 4 + c); };
    TLC59711Driver tdrv(1, mapper);

    tdrv.setPWM(0, 0x1234); // GS0  → bytes 26-27
    tdrv.setPWM(1, 0xABCD); // GS1  → bytes 24-25
    tdrv.commitPhysicalUpdates();

    const uint8_t *pkt = tdrv.lastPacket();
    if (tdrv.lastPacketSize() != 28)
        return false;

    if (pkt[0] != 0x96 || pkt[1] != 0xDF || pkt[2] != 0xFF || pkt[3] != 0xFF)
        return false;

    for (uint8_t i = 4; i < 24; ++i)
        if (pkt[i] != 0x00)
            return false;

    if (pkt[24] != 0xAB || pkt[25] != 0xCD)
        return false;
    if (pkt[26] != 0x12 || pkt[27] != 0x34)
        return false;

    return true;
}

/* TLC59711: setNodeHardware() maps (r,c) → channel; full-on is 0xFFFF, off is 0x0000. */
static bool test_tlc59711_set_node()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 4 + c); };
    TLC59711Driver tdrv(1, mapper);

    // connect(0,2) → channel 2 = GS2 → bytes 4 + (11-2)*2 = 22-23
    tdrv.setNodeHardware(0, 2, true);
    tdrv.commitPhysicalUpdates();
    const uint8_t *pkt = tdrv.lastPacket();
    if (pkt[22] != 0xFF || pkt[23] != 0xFF)
        return false;

    tdrv.setNodeHardware(0, 2, false);
    tdrv.commitPhysicalUpdates();
    pkt = tdrv.lastPacket();
    if (pkt[22] != 0x00 || pkt[23] != 0x00)
        return false;

    return true;
}

/*
 * TCA9548AInterface: channel-select byte is sent before the first MCP23017
 * write and suppressed on subsequent writes to the same channel (cache hit).
 *
 * Mux: TCA9548A at 0x71, channel 3 → channelMask = 1<<3 = 0x08.
 * MockI2C::writeByte() uses the I2CInterface default which records
 * {addr=muxAddr, reg=channelMask, val=0x00} in the writes[] vector.
 */
static bool test_tca9548a_mux_transparent()
{
    MockI2C i2c;
    TCA9548AInterface mux(&i2c, 0x71, 3); // addr=0x71, channel 3
    auto mapper = [](uint8_t r, uint8_t c) -> uint8_t { return (uint8_t)(r * 4 + c); };
    MCP23017Driver md(&mux, 0x20, mapper);
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&md);

    mux.begin(); // initialise bus + invalidate channel cache for 0x71
    m.begin();   // → md.begin() → IODIRA, IODIRB, OLATA, OLATB via mux

    // begin() should produce exactly 5 I2C writes:
    // [0] channel-select {0x71, 0x08, 0x00}  — cache miss on first write
    // [1] IODIRA {0x20, 0x00, 0x00}
    // [2] IODIRB {0x20, 0x01, 0x00}
    // [3] OLATA  {0x20, 0x14, 0x00}          — cache hit, no re-select
    // [4] OLATB  {0x20, 0x15, 0x00}          — cache hit, no re-select
    if (i2c.writes.size() != 5)
        return false;
    if (i2c.writes[0].addr != 0x71 || i2c.writes[0].reg != 0x08)
        return false; // channel-select must be first

    i2c.writes.clear();

    // connect(1, 2) → pin 6 → OLATA bit 6 = 0x40.
    // Channel 3 is already selected (cache hot) → zero extra I2C writes.
    m.connect(1, 2);

    // Exactly 2 writes: OLATA and OLATB — no channel-select at all.
    if (i2c.writes.size() != 2)
        return false;
    bool sawOLATA = false;
    for (auto &w : i2c.writes)
        if (w.addr == 0x20 && w.reg == 0x14 && (w.val & 0x40))
            sawOLATA = true;
    if (!sawOLATA)
        return false;

    return true;
}

/* setLevel() on a binary driver: delegates to setNodeHardware(true/false). */
static bool test_set_level_binary_driver()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    if (!m.setLevel(0, 1, 0x8000))
        return false;
    if (drv.calls.size() != 1 || !drv.calls[0].state)
        return false;

    if (!m.setLevel(0, 1, 0))
        return false;
    if (drv.calls.size() != 2 || drv.calls[1].state != false)
        return false;

    return true;
}

/* setLevel() on TLC59711: routes through setNodeLevel() without clamping. */
static bool test_set_level_tlc59711()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 4 + c); };
    TLC59711Driver tdrv(1, mapper);
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&tdrv);
    m.begin();

    m.setLevel(0, 0, 0x8000); // half power; channel 0 = GS0 = bytes 26-27
    tdrv.commitPhysicalUpdates();
    const uint8_t *pkt = tdrv.lastPacket();
    if (pkt[26] != 0x80 || pkt[27] != 0x00)
        return false;

    m.setLevel(0, 0, 0);
    tdrv.commitPhysicalUpdates();
    pkt = tdrv.lastPacket();
    if (pkt[26] != 0x00 || pkt[27] != 0x00)
        return false;

    return true;
}

/* setLevel() must respect interlocks (same rules as connect()). */
static bool test_set_level_interlock()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.lockRows(0, 1);

    bool ok1 = m.setLevel(0, 0, 0x8000);
    bool ok2 = m.setLevel(1, 0, 0x4000); // blocked

    return ok1 && !ok2;
}

/*
 * connect() with all 8 pulse slots occupied by other nodes: a 9th connect()
 * must return false WITHOUT energizing the SET coil.
 */
static bool test_connect_slot_full_returns_false()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Fill all 8 pulse slots (MAX_PULSES).
    uint8_t fill_r[] = {0, 0, 0, 0, 1, 1, 1, 1};
    uint8_t fill_c[] = {0, 1, 2, 3, 0, 1, 2, 3};
    for (int i = 0; i < 8; ++i)
        if (!m.connect(fill_r[i], fill_c[i]))
            return false;

    std::size_t callsBefore = drv.calls.size();

    // 9th connect with all slots occupied: must fail without a hardware call.
    bool result = m.connect(2, 0);
    if (result)
        return false; // must return false
    if (drv.calls.size() != callsBefore)
        return false; // must not emit a hardware call

    return true;
}

/*
 * disconnect() with all 8 pulse slots occupied by other nodes: must return
 * false without energizing the RESET coil.
 */
static bool test_disconnect_slot_full_returns_false()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Connect (0,0), wait for its SET pulse to expire → node stays connected, slot freed.
    m.connect(0, 0);
    advanceMillis(60);
    m.update();

    // Now connect 8 other nodes to fill all 8 slots again.
    uint8_t fill_r[] = {0, 0, 0, 1, 1, 1, 1, 2};
    uint8_t fill_c[] = {1, 2, 3, 0, 1, 2, 3, 0};
    for (int i = 0; i < 8; ++i)
        if (!m.connect(fill_r[i], fill_c[i]))
            return false;

    std::size_t callsBefore = drv.calls.size();

    // Disconnect (0,0): no in-flight pulse for it, but table is full → must fail.
    bool result = m.disconnect(0, 0);
    if (result)
        return false;
    if (drv.calls.size() != callsBefore)
        return false; // no RESET coil call emitted

    return true;
}

/*
 * clearAll() with more connected nodes than MAX_PULSES (8): nodes beyond the
 * slot limit must NOT have setNodeHardware called; state is cleared to false
 * for all nodes.
 */
static bool test_clearall_skips_hardware_when_slot_full()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Connect 9 nodes in two passes so we never exceed 8 in-flight slots.
    uint8_t all_r[] = {0, 0, 0, 0, 1, 1, 1, 1, 2};
    uint8_t all_c[] = {0, 1, 2, 3, 0, 1, 2, 3, 0};

    // Pass 1: 8 nodes — fills all slots.
    for (int i = 0; i < 8; ++i)
        if (!m.connect(all_r[i], all_c[i]))
            return false;
    advanceMillis(60);
    m.update(); // releases all 8 slots; 8 nodes remain connected
    drv.calls.clear();
    drv.releases.clear();

    // Pass 2: 9th node.
    if (!m.connect(2, 0))
        return false;
    advanceMillis(60);
    m.update(); // 9th slot freed; all 9 nodes connected, 0 in-flight
    drv.calls.clear();
    drv.releases.clear();

    // clearAll on 9 nodes: first 8 get RESET coil + slot; 9th (2,0) gets skipped.
    g_millis = 200;
    m.clearAll();

    // Count how many false-state hardware calls were emitted.
    int reset_calls = 0;
    bool ninth_called = false;
    for (auto &c : drv.calls)
    {
        if (!c.state)
        {
            ++reset_calls;
            if (c.r == 2 && c.c == 0)
                ninth_called = true;
        }
    }
    if (reset_calls != 8)
        return false; // exactly 8 RESET coil calls expected
    if (ninth_called)
        return false; // 9th node must NOT have been energized

    // After pulse expiry, update → 8 releases.
    advanceMillis(60);
    m.update();
    if (drv.releases.size() != 8)
        return false;

    // State for (2,0) was logically cleared; connect should succeed.
    drv.calls.clear();
    if (!m.connect(2, 0))
        return false;
    if (drv.calls.empty() || !drv.calls.back().state)
        return false;

    return true;
}

/*
 * Interlock during a RESET pulse in-flight window: after disconnect() sets the
 * logical state to false but before update() fires releaseNode(), the relay is
 * physically still closed.  An interlocked row must be blocked.
 */
static bool test_interlock_desync_blocked()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    m.lockRows(0, 1);
    g_millis = 0;

    // Connect row 0 to col 0; wait for SET pulse to expire.
    m.connect(0, 0);
    advanceMillis(60);
    m.update();

    // Disconnect row 0: RESET coil fires, RESET pulse in-flight.
    m.disconnect(0, 0);

    // During the RESET pulse window, row 1 (interlocked with 0) must be blocked.
    bool blocked = !m.connect(1, 0);
    if (!blocked)
        return false;

    // After RESET pulse expires, row 1 must be allowed.
    advanceMillis(60);
    m.update();
    if (!m.connect(1, 0))
        return false;

    return true;
}

/*
 * Exclusive-input during a RESET pulse in-flight window: same scenario as the
 * interlock test but for exclusive columns instead of interlocked rows.
 */
static bool test_exclusive_desync_blocked()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    m.exclusiveInput(0);
    g_millis = 0;

    m.connect(0, 0);
    advanceMillis(60);
    m.update();

    m.disconnect(0, 0); // RESET pulse in-flight

    bool blocked = !m.connect(1, 0); // exclusive col 0 — must be blocked
    if (!blocked)
        return false;

    advanceMillis(60);
    m.update();
    if (!m.connect(1, 0))
        return false;

    return true;
}

/*
 * setLevel() in RE_LATCHING_DUAL_COIL mode: the connecting path must register
 * the pulse so update() calls releaseNode() after _pdur ms.
 */
static bool test_setlevel_latching_registers_pulse()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 20);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    if (!m.setLevel(0, 0, 0x8000)) // level > 0 → connecting path
        return false;
    if (drv.calls.empty() || !drv.calls.back().state)
        return false;

    // Before expiry: no release.
    advanceMillis(10);
    m.update();
    if (!drv.releases.empty())
        return false;

    // After expiry: exactly one release for (0,0).
    advanceMillis(15);
    m.update();
    if (drv.releases.size() != 1)
        return false;
    if (drv.releases[0].first != 0 || drv.releases[0].second != 0)
        return false;

    return true;
}

/*
 * setLevel() in RE_LATCHING_DUAL_COIL with a full pulse table: must return
 * false without energizing hardware.
 */
static bool test_setlevel_latching_slot_full_returns_false()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Fill all 8 slots.
    uint8_t fill_r[] = {0, 0, 0, 0, 1, 1, 1, 1};
    uint8_t fill_c[] = {0, 1, 2, 3, 0, 1, 2, 3};
    for (int i = 0; i < 8; ++i)
        if (!m.connect(fill_r[i], fill_c[i]))
            return false;

    std::size_t callsBefore = drv.calls.size();
    bool result = m.setLevel(2, 0, 0x8000);
    if (result)
        return false;
    if (drv.calls.size() != callsBefore)
        return false;

    return true;
}

/* ==========================================================================
 * BOUNDARY / LIMIT TESTS
 * ========================================================================*/

/* Degenerate matrices (rows=0 or cols=0): no crash; all operations return false. */
static bool test_bounds_degenerate()
{
    // rows=0, cols=0
    {
        XPoint m(0, 0, RE_NON_LATCHING, 0);
        if (m.connect(0, 0))
            return false;
        if (m.disconnect(0, 0))
            return false;
        m.clearAll(); // must not crash
        m.update();   // must not crash
    }
    // rows=0, cols=4
    {
        XPoint m(0, 4, RE_NON_LATCHING, 0);
        if (m.connect(0, 0))
            return false;
    }
    // rows=4, cols=0
    {
        XPoint m(4, 0, RE_NON_LATCHING, 0);
        if (m.connect(0, 0))
            return false;
    }
    return true;
}

/* Out-of-range indices must return false without touching state. */
static bool test_bounds_out_of_range()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);

    if (m.connect(4, 0)) // row out of range
        return false;
    if (m.connect(0, 4)) // col out of range
        return false;
    if (m.connect(255, 0)) // extreme row
        return false;
    if (m.connect(0, 255)) // extreme col
        return false;
    if (m.disconnect(4, 0))
        return false;
    if (m.setLevel(4, 0, 0x8000))
        return false;
    if (!drv.calls.empty())
        return false; // no hardware calls for invalid indices

    return true;
}

/* lockRows(r, r) is a self-interlock: silently stored but never checked.
 * connect() must still succeed for that row. */
static bool test_lockrows_self_noop()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.lockRows(0, 0); // self-lock — no effect
    if (!m.connect(0, 0))
        return false;
    return true;
}

/* RE_LATCHING_DUAL_COIL with pdur=0: every pulse expires on the very next
 * update() call (now - t0 >= 0 is always true). */
static bool test_pdur_zero_fires_immediately()
{
    MockDriver drv;
    XPoint m(2, 2, RE_LATCHING_DUAL_COIL, 0);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    if (!m.connect(0, 0))
        return false;
    if (drv.releases.size() != 0)
        return false;

    m.update(); // elapsed = 0 >= 0 → fires immediately
    if (drv.releases.size() != 1)
        return false;

    return true;
}

/* 32-bit millis() rollover: pulse registered just before the uint32_t boundary
 * must fire correctly after the counter wraps. */
static bool test_millis_rollover_uint32()
{
    MockDriver drv;
    XPoint m(2, 2, RE_LATCHING_DUAL_COIL, 10);
    m.setDriver(&drv);
    m.begin();

    // Set timer 5 ms before the 32-bit boundary (0xFFFFFFFF).
    g_millis = (uint32_t)(-1) - 5u; // = 0xFFFFFFFA

    m.connect(0, 0); // pulse t0 = 0xFFFFFFFA

    // Advance 6 ms — wraps to 0.  Elapsed = 6 < 10 → must NOT fire.
    advanceMillis(6u);
    m.update();
    if (!drv.releases.empty())
        return false;

    // Advance 5 more ms — now at 5.  Elapsed = (uint32_t)(5 - 0xFFFFFFFA) = 11 >= 10.
    advanceMillis(5u);
    m.update();
    if (drv.releases.size() != 1)
        return false;
    if (drv.releases[0].first != 0 || drv.releases[0].second != 0)
        return false;

    return true;
}

/* connect() / disconnect() with no driver attached must not crash and must
 * still update logical state correctly. */
static bool test_no_driver_no_crash()
{
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    // No setDriver() call.
    m.begin(); // no-op without driver
    if (!m.connect(0, 0))
        return false; // state update still works
    if (!m.disconnect(0, 0))
        return false;
    m.clearAll();
    m.update();
    return true;
}

/* ==========================================================================
 * FULL RANGE TEST — 0×0 through 255×255
 *
 * Greedily constructs every distinct matrix size, exercises connect /
 * disconnect on the first and last valid nodes, and verifies idempotence and
 * bounds.  Runs on a modern PC where heap and time are not scarce.
 * ========================================================================*/
static bool test_all_matrix_sizes()
{
    for (int rows = 0; rows <= 255; ++rows)
    {
        for (int cols = 0; cols <= 255; ++cols)
        {
            XPoint m((uint8_t)rows, (uint8_t)cols, RE_NON_LATCHING, 0);

            if (rows == 0 || cols == 0)
            {
                // Degenerate: any connect must fail.
                if (m.connect(0, 0))
                    return false;
                if (m.disconnect(0, 0))
                    return false;
                continue;
            }

            // First node (0,0).
            if (!m.connect(0, 0))
                return false;
            if (!m.connect(0, 0)) // idempotent — state already true
                return false;
            if (!m.disconnect(0, 0))
                return false;
            if (!m.disconnect(0, 0)) // idempotent — state already false
                return false;

            // Last node (rows-1, cols-1).
            uint8_t lr = (uint8_t)(rows - 1);
            uint8_t lc = (uint8_t)(cols - 1);
            if (!m.connect(lr, lc))
                return false;
            if (!m.disconnect(lr, lc))
                return false;

            // Out-of-range checks (wraps safely inside uint8_t arithmetic).
            if (rows < 255 && m.connect((uint8_t)rows, 0))
                return false;
            if (cols < 255 && m.connect(0, (uint8_t)cols))
                return false;
        }
    }
    return true;
}

/* ==========================================================================
 * GAP COVERAGE TESTS
 * ========================================================================*/

/*
 * clearAll() while SET pulses are still in-flight: the SET-pulse cancel path.
 *
 * The loop inside clearAll() cancels any active SET-pulse entry for each node
 * and reuses that same slot for the RESET pulse, so no extra slot is consumed.
 * This path was previously untested — every prior clearAll test called it only
 * after all pulses had already expired.
 */
static bool test_clearall_set_pulse_cancel_and_reuse()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 50);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Connect 2 nodes at t=0; SET pulses occupy slots 0 and 1.
    m.connect(0, 0);
    m.connect(0, 1);

    // At t=10 the SET pulses are still in-flight (pdur=50).
    // clearAll() must cancel those SET entries and register RESET entries in
    // the same slots.
    g_millis = 10;
    drv.calls.clear();
    drv.commits = 0;
    m.clearAll();

    // Exactly 2 false-state calls (RESET coil) — no stale SET calls.
    int hw_false = 0;
    for (auto &c : drv.calls)
        if (!c.state)
            ++hw_false;
    if (hw_false != 2)
        return false;

    // commitPhysicalUpdates called exactly once (batched at end of clearAll).
    if (drv.commits != 1)
        return false;

    // No releases yet — RESET pulses have just been registered.
    if (!drv.releases.empty())
        return false;

    // After pdur ms the RESET pulses expire; exactly 2 releases.
    advanceMillis(55u);
    m.update();
    if (drv.releases.size() != 2)
        return false;

    // Both nodes released, not the original SET nodes.
    bool got00 = false, got01 = false;
    for (auto &r : drv.releases)
    {
        if (r.first == 0 && r.second == 0)
            got00 = true;
        if (r.first == 0 && r.second == 1)
            got01 = true;
    }
    return got00 && got01;
}

/*
 * setLevel(r, c, 0) in RE_LATCHING_DUAL_COIL: the disconnecting path.
 * Must call setNodeHardware(false) (RESET coil) and register a pulse so
 * releaseNode() fires after _pdur ms.  Before fix #4, this path never
 * registered a pulse.
 */
static bool test_setlevel_latching_disconnect_path()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 20);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Connect then let the SET pulse expire so the node is cleanly connected.
    m.connect(0, 0);
    advanceMillis(25u);
    m.update();
    drv.calls.clear();
    drv.releases.clear();

    // setLevel(level=0) → disconnecting path → RESET coil.
    if (!m.setLevel(0, 0, 0))
        return false;
    if (drv.calls.size() != 1 || drv.calls[0].state != false)
        return false;

    // Before expiry: no release.
    advanceMillis(10u);
    m.update();
    if (!drv.releases.empty())
        return false;

    // After expiry: exactly one release for (0,0).
    advanceMillis(15u);
    m.update();
    if (drv.releases.size() != 1)
        return false;
    if (drv.releases[0].first != 0 || drv.releases[0].second != 0)
        return false;

    return true;
}

/*
 * setLevel() idempotent in RE_LATCHING_DUAL_COIL: if the node is already in
 * the desired state, no coil is energized and no driver call is emitted.
 */
static bool test_setlevel_latching_idempotent()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 20);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Connect and wait for SET pulse to expire.
    m.connect(0, 0);
    advanceMillis(25u);
    m.update();

    std::size_t calls_before = drv.calls.size();

    // setLevel with level>0 while state is already true: idempotent, no call.
    if (!m.setLevel(0, 0, 0x8000))
        return false;
    if (drv.calls.size() != calls_before)
        return false; // no hardware call emitted

    // Disconnect, let RESET pulse expire.
    m.disconnect(0, 0);
    advanceMillis(25u);
    m.update();
    calls_before = drv.calls.size();

    // setLevel with level=0 while state is already false: idempotent, no call.
    if (!m.setLevel(0, 0, 0))
        return false;
    if (drv.calls.size() != calls_before)
        return false;

    return true;
}

/*
 * clearAll() in RE_NON_LATCHING: must call setNodeHardware(false) for every
 * connected node and issue exactly one commitPhysicalUpdates() — not one per
 * node.
 */
static bool test_clearall_nonlatching()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    m.connect(0, 0);
    m.connect(1, 1);
    m.connect(2, 2);
    drv.calls.clear();
    drv.commits = 0;

    m.clearAll();

    // Exactly 3 false-state hardware calls.
    if (drv.calls.size() != 3)
        return false;
    for (auto &c : drv.calls)
        if (c.state != false)
            return false;

    // One batch commit, not three.
    if (drv.commits != 1)
        return false;

    // Non-latching: no releases.
    if (!drv.releases.empty())
        return false;

    return true;
}

/*
 * Multiple interlocks on the same row (fan-out): lockRows(0,1) and
 * lockRows(0,2) block both rows 1 and 2 when row 0 holds a column.
 * Rows 1 and 2 are NOT interlocked with each other — each can connect
 * to columns not held by row 0.
 */
static bool test_interlock_fanout()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.lockRows(0, 1); // row 0 ↔ row 1
    m.lockRows(0, 2); // row 0 ↔ row 2

    // Row 0 takes col 0.
    if (!m.connect(0, 0))
        return false;

    // Rows 1 and 2 are both blocked from col 0 (each locked with row 0).
    if (m.connect(1, 0))
        return false;
    if (m.connect(2, 0))
        return false;

    // Row 1 can still take col 1 (row 0 doesn't hold col 1).
    if (!m.connect(1, 1))
        return false;

    // Row 2 can take col 2 (not blocked by row 0).
    if (!m.connect(2, 2))
        return false;

    // Rows 1 and 2 share no direct interlock; row 1 holding col 1 does not
    // block row 2 from col 1.
    if (!m.connect(2, 1))
        return false;

    return true;
}

/*
 * setLevel() must respect exclusiveInput() the same way connect() does.
 * Also verifies the "disconnects always allowed" rule: setLevel(level=0)
 * bypasses the exclusive check so the holding row can release.
 */
static bool test_setlevel_exclusive()
{
    MockDriver drv;
    XPoint m(3, 3, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.exclusiveInput(0);

    // Row 0 takes exclusive col 0 via setLevel.
    if (!m.setLevel(0, 0, 0x8000))
        return false;

    // Row 1 blocked — col 0 is exclusive and row 0 holds it.
    if (m.setLevel(1, 0, 0x4000))
        return false;

    // Row 0 releases via setLevel(0) — disconnect path bypasses exclusive check.
    if (!m.setLevel(0, 0, 0))
        return false;

    // Row 1 can now take col 0.
    if (!m.setLevel(1, 0, 0x4000))
        return false;

    return true;
}

/*
 * update() is a no-op for RE_NON_LATCHING: calling it repeatedly must not
 * emit any driver calls or releases.
 */
static bool test_update_noop_nonlatching()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    m.connect(0, 0);
    drv.calls.clear();
    drv.commits = 0;

    for (int i = 0; i < 10; ++i)
    {
        advanceMillis(100u);
        m.update();
    }

    if (!drv.releases.empty())
        return false;
    if (!drv.calls.empty())
        return false;
    if (drv.commits != 0)
        return false;

    return true;
}

/*
 * connect() idempotent with driver: a second connect() on an already-connected
 * node must return true but emit no additional hardware call.
 */
static bool test_connect_idempotent_with_driver()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    if (!m.connect(0, 0))
        return false;
    std::size_t n = drv.calls.size();

    if (!m.connect(0, 0))
        return false; // must succeed
    if (drv.calls.size() != n)
        return false; // must not emit a new driver call

    return true;
}

/*
 * disconnect() idempotent with driver: a second disconnect() on an already-
 * disconnected node must return true but emit no additional hardware call.
 */
static bool test_disconnect_idempotent_with_driver()
{
    MockDriver drv;
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    m.connect(0, 0);
    m.disconnect(0, 0);
    std::size_t n = drv.calls.size();

    if (!m.disconnect(0, 0))
        return false; // must succeed
    if (drv.calls.size() != n)
        return false; // must not emit a new driver call

    return true;
}

/*
 * MCP23017: nodes that map to pins 8–15 write to port B (OLATB = 0x15).
 * With mapper r*4+c, node (2,0) → pin 8 → OLATB bit 0.
 */
static bool test_mcp23017_port_b()
{
    MockI2C i2c;
    auto mapper = [](uint8_t r, uint8_t c) -> uint8_t { return (uint8_t)(r * 4 + c); };
    MCP23017Driver md(&i2c, 0x20, mapper);
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&md);
    m.begin();
    i2c.writes.clear();

    // (2,0) → pin 8 → port B bit 0.
    m.connect(2, 0);

    bool sawOLATB = false;
    for (auto &w : i2c.writes)
        if (w.reg == 0x15 && (w.val & 0x01))
            sawOLATB = true;

    if (!sawOLATB)
        return false;

    // (3,3) → pin 15 → port B bit 7; both pins now set → OLATB = 0x81.
    m.connect(3, 3);
    bool sawBoth = false;
    for (auto &w : i2c.writes)
        if (w.reg == 0x15 && w.val == 0x81)
            sawBoth = true;

    return sawBoth;
}

/*
 * MCP23017 shadow accumulation: connecting two nodes on port A must set both
 * bits in a single OLATA write, not overwrite the first bit with the second.
 * gpioA() must reflect the combined shadow value.
 */
static bool test_mcp23017_shadow_accumulation()
{
    MockI2C i2c;
    auto mapper = [](uint8_t r, uint8_t c) -> uint8_t { return (uint8_t)(r * 4 + c); };
    MCP23017Driver md(&i2c, 0x20, mapper);
    XPoint m(4, 4, RE_NON_LATCHING, 0);
    m.setDriver(&md);
    m.begin();
    i2c.writes.clear();

    // (1,2) → pin 6 → bit 6.
    m.connect(1, 2);
    if (md.gpioA() != 0x40)
        return false;

    // (1,3) → pin 7 → bit 7; shadow should now have both.
    m.connect(1, 3);
    if (md.gpioA() != 0xC0)
        return false;

    // Verify the combined value was written to OLATA.
    bool sawCombined = false;
    for (auto &w : i2c.writes)
        if (w.reg == 0x14 && w.val == 0xC0)
            sawCombined = true;
    if (!sawCombined)
        return false;

    // Disconnect (1,2): bit 6 cleared; only bit 7 remains.
    m.disconnect(1, 2);
    if (md.gpioA() != 0x80)
        return false;

    return true;
}

/*
 * ShiftRegisterDriver bit-packing for byte index > 0: bit index 8 maps to
 * byte 1 bit 0, and bit index 15 maps to byte 1 bit 7.
 */
static bool test_shift_register_byte1()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 8 + c); };
    ShiftRegisterDriver drv(16, mapper);

    // (1,0) → index 8 → byte 1, bit 0.
    drv.setNodeHardware(1, 0, true);
    if (drv.byteAt(1) != 0x01)
        return false;

    // (1,7) → index 15 → byte 1, bit 7.
    drv.setNodeHardware(1, 7, true);
    if (drv.byteAt(1) != 0x81)
        return false;

    // Byte 0 must be untouched.
    if (drv.byteAt(0) != 0x00)
        return false;

    // Clear (1,0).
    drv.setNodeHardware(1, 0, false);
    if (drv.byteAt(1) != 0x80)
        return false;

    return true;
}

/*
 * TLC59711 two-chip daisy-chain byte ordering.
 *
 * commitPhysicalUpdates() sends chip N-1 data first so that software channel 0
 * always lands on the physically-nearest chip (nearest MOSI).
 *
 * For N=2, packet layout (56 bytes):
 *   Bytes  0- 3: chip 1 control word
 *   Bytes  4-27: chip 1 GS11 … GS0  (channels 23 … 12)
 *   Bytes 28-31: chip 0 control word
 *   Bytes 32-55: chip 0 GS11 … GS0  (channels 11 … 0)
 *
 * Channel 0  (GS0  chip 0) → bytes 54-55
 * Channel 12 (GS0  chip 1) → bytes 26-27
 */
static bool test_tlc59711_multichip_ordering()
{
    auto mapper = [](uint8_t r, uint8_t c) -> uint16_t { return (uint16_t)(r * 12 + c); };
    TLC59711Driver tdrv(2, mapper);

    if (tdrv.lastPacketSize() != 56)
        return false;

    tdrv.setPWM(0, 0xABCD);  // channel 0  → GS0 chip 0 → bytes 54-55
    tdrv.setPWM(12, 0x1234); // channel 12 → GS0 chip 1 → bytes 26-27
    tdrv.commitPhysicalUpdates();
    const uint8_t *pkt = tdrv.lastPacket();

    // Both control words.
    if (pkt[0] != 0x96 || pkt[1] != 0xDF || pkt[2] != 0xFF || pkt[3] != 0xFF)
        return false;
    if (pkt[28] != 0x96 || pkt[29] != 0xDF || pkt[30] != 0xFF || pkt[31] != 0xFF)
        return false;

    // Channel 12 (GS0 chip 1) at bytes 26-27.
    if (pkt[26] != 0x12 || pkt[27] != 0x34)
        return false;

    // Channel 0 (GS0 chip 0) at bytes 54-55.
    if (pkt[54] != 0xAB || pkt[55] != 0xCD)
        return false;

    // All other GS bytes must be 0x00.
    for (int i = 4; i < 26; ++i)
        if (pkt[i] != 0x00)
            return false;
    for (int i = 32; i < 54; ++i)
        if (pkt[i] != 0x00)
            return false;

    return true;
}

/* ==========================================================================
 * JSON-DRIVEN TESTS
 * ========================================================================*/

static bool test_slot_full_scenarios_json()
{
    std::string path = data_path("slot_full_scenarios.json");
    std::vector<std::string> objs = json::load(path);
    if (objs.empty())
    {
        std::cerr << "  [SKIP] slot_full_scenarios.json not found\n";
        return true; // skip rather than fail when running outside project root
    }

    for (std::size_t s = 0; s < objs.size(); ++s)
    {
        const std::string &o = objs[s];
        std::string test = json::get_str(o, "test");
        uint8_t rows = (uint8_t)json::get_int(o, "rows", 4);
        uint8_t cols = (uint8_t)json::get_int(o, "cols", 4);
        uint16_t pdur = (uint16_t)json::get_int(o, "pulse_ms", 50);

        if (test == "connect_overflow")
        {
            std::vector<int> fr = json::get_int_array(o, "fill_r");
            std::vector<int> fc = json::get_int_array(o, "fill_c");
            uint8_t ovr = (uint8_t)json::get_int(o, "overflow_r", 2);
            uint8_t ovc = (uint8_t)json::get_int(o, "overflow_c", 0);

            MockDriver drv;
            XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
            m.setDriver(&drv);
            m.begin();
            g_millis = 0;

            for (std::size_t i = 0; i < fr.size(); ++i)
                if (!m.connect((uint8_t)fr[i], (uint8_t)fc[i]))
                    return false;

            std::size_t before = drv.calls.size();
            if (m.connect(ovr, ovc))
            {
                std::cerr << "  scenario " << s << " connect_overflow: 9th connect returned true\n";
                return false;
            }
            if (drv.calls.size() != before)
            {
                std::cerr << "  scenario " << s << " connect_overflow: hardware call emitted for slot-full connect\n";
                return false;
            }
        }
        else if (test == "disconnect_overflow")
        {
            std::vector<int> fr = json::get_int_array(o, "fill_r");
            std::vector<int> fc = json::get_int_array(o, "fill_c");
            uint8_t hr = (uint8_t)json::get_int(o, "hold_r", 0);
            uint8_t hc = (uint8_t)json::get_int(o, "hold_c", 0);

            MockDriver drv;
            XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
            m.setDriver(&drv);
            m.begin();
            g_millis = 0;

            // Connect hold node, wait for its pulse to expire.
            if (!m.connect(hr, hc))
                return false;
            advanceMillis((uint32_t)pdur + 10u);
            m.update();

            // Fill all 8 slots with other nodes.
            for (std::size_t i = 0; i < fr.size(); ++i)
                if (!m.connect((uint8_t)fr[i], (uint8_t)fc[i]))
                    return false;

            std::size_t before = drv.calls.size();
            if (m.disconnect(hr, hc))
            {
                std::cerr << "  scenario " << s << " disconnect_overflow: disconnect returned true with full table\n";
                return false;
            }
            if (drv.calls.size() != before)
            {
                std::cerr << "  scenario " << s
                          << " disconnect_overflow: hardware call emitted for slot-full disconnect\n";
                return false;
            }
        }
        else if (test == "clearall_overflow")
        {
            std::vector<int> cr = json::get_int_array(o, "connect_r");
            std::vector<int> cc = json::get_int_array(o, "connect_c");
            uint8_t sr = (uint8_t)json::get_int(o, "skipped_r", 2);
            uint8_t sc = (uint8_t)json::get_int(o, "skipped_c", 0);

            MockDriver drv;
            XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
            m.setDriver(&drv);
            m.begin();
            g_millis = 0;

            // Connect all nodes in batches of 8 to avoid slot pressure.
            for (std::size_t i = 0; i < cr.size(); i += 8)
            {
                std::size_t end = i + 8 < cr.size() ? i + 8 : cr.size();
                for (std::size_t j = i; j < end; ++j)
                    if (!m.connect((uint8_t)cr[j], (uint8_t)cc[j]))
                        return false;
                advanceMillis((uint32_t)pdur + 10u);
                m.update();
            }
            drv.calls.clear();
            drv.releases.clear();
            g_millis = 1000;

            m.clearAll();

            // Exactly (N-1) false-state hardware calls (skipped node omitted).
            int reset_calls = 0;
            bool skipped_called = false;
            for (auto &c : drv.calls)
            {
                if (!c.state)
                {
                    ++reset_calls;
                    if (c.r == sr && c.c == sc)
                        skipped_called = true;
                }
            }
            if (reset_calls != (int)(cr.size() - 1))
            {
                std::cerr << "  scenario " << s << " clearall_overflow: expected " << (cr.size() - 1)
                          << " RESET calls, got " << reset_calls << "\n";
                return false;
            }
            if (skipped_called)
            {
                std::cerr << "  scenario " << s << " clearall_overflow: skipped node (" << (int)sr << "," << (int)sc
                          << ") was energized\n";
                return false;
            }

            // After pulse expiry the skipped node state is false; connect must succeed.
            advanceMillis((uint32_t)pdur + 10u);
            m.update();
            drv.calls.clear();
            if (!m.connect(sr, sc))
            {
                std::cerr << "  scenario " << s << " clearall_overflow: reconnect of skipped node failed\n";
                return false;
            }
        }
    }
    return true;
}

static bool test_desync_scenarios_json()
{
    std::string path = data_path("desync_scenarios.json");
    std::vector<std::string> objs = json::load(path);
    if (objs.empty())
    {
        std::cerr << "  [SKIP] desync_scenarios.json not found\n";
        return true;
    }

    for (std::size_t s = 0; s < objs.size(); ++s)
    {
        const std::string &o = objs[s];
        std::string type = json::get_str(o, "type");
        uint8_t rows = (uint8_t)json::get_int(o, "rows", 4);
        uint8_t cols = (uint8_t)json::get_int(o, "cols", 4);
        uint16_t pdur = (uint16_t)json::get_int(o, "pulse_ms", 50);
        uint8_t hold_r = (uint8_t)json::get_int(o, "hold_r", 0);
        uint8_t hold_c = (uint8_t)json::get_int(o, "hold_c", 0);
        uint8_t blocked_r = (uint8_t)json::get_int(o, "blocked_r", 1);
        uint8_t blocked_c = (uint8_t)json::get_int(o, "blocked_c", 0);

        MockDriver drv;
        XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
        m.setDriver(&drv);
        m.begin();

        if (type == "interlock")
        {
            uint8_t rowA = (uint8_t)json::get_int(o, "lock_rowA", 0);
            uint8_t rowB = (uint8_t)json::get_int(o, "lock_rowB", 1);
            m.lockRows(rowA, rowB);
        }
        else if (type == "exclusive")
        {
            uint8_t excl = (uint8_t)json::get_int(o, "exclusive_col", 0);
            m.exclusiveInput(excl);
        }

        g_millis = 0;

        // Connect and let SET pulse expire.
        m.connect(hold_r, hold_c);
        advanceMillis((uint32_t)pdur + 10u);
        m.update();

        // Disconnect: RESET pulse now in-flight.
        m.disconnect(hold_r, hold_c);

        // During RESET pulse window, blocked node must be refused.
        if (m.connect(blocked_r, blocked_c))
        {
            std::cerr << "  desync scenario " << s << " (" << type
                      << "): blocked connect succeeded during RESET pulse window\n";
            return false;
        }

        // After RESET pulse expires, blocked node must be allowed.
        advanceMillis((uint32_t)pdur + 10u);
        m.update();
        if (!m.connect(blocked_r, blocked_c))
        {
            std::cerr << "  desync scenario " << s << " (" << type << "): connect failed after RESET pulse expired\n";
            return false;
        }
    }
    return true;
}

static bool test_interlock_patterns_json()
{
    std::string path = data_path("interlock_patterns.json");
    std::vector<std::string> objs = json::load(path);
    if (objs.empty())
    {
        std::cerr << "  [SKIP] interlock_patterns.json not found\n";
        return true;
    }

    for (std::size_t s = 0; s < objs.size(); ++s)
    {
        const std::string &o = objs[s];
        uint8_t rows = (uint8_t)json::get_int(o, "rows", 4);
        uint8_t cols = (uint8_t)json::get_int(o, "cols", 4);
        std::vector<int> pair_a = json::get_int_array(o, "pair_a");
        std::vector<int> pair_b = json::get_int_array(o, "pair_b");
        uint8_t hold_r = (uint8_t)json::get_int(o, "hold_r", 0);
        uint8_t hold_c = (uint8_t)json::get_int(o, "hold_c", 0);
        uint8_t blocked_r = (uint8_t)json::get_int(o, "blocked_r", 1);
        uint8_t blocked_c = (uint8_t)json::get_int(o, "blocked_c", 0);
        uint8_t allowed_r = (uint8_t)json::get_int(o, "allowed_r", 2);
        uint8_t allowed_c = (uint8_t)json::get_int(o, "allowed_c", 0);

        MockDriver drv;
        XPoint m(rows, cols, RE_NON_LATCHING, 0);
        m.setDriver(&drv);

        for (std::size_t i = 0; i < pair_a.size() && i < pair_b.size(); ++i)
            m.lockRows((uint8_t)pair_a[i], (uint8_t)pair_b[i]);

        // Hold row connects.
        if (!m.connect(hold_r, hold_c))
        {
            std::cerr << "  interlock pattern " << s << ": hold connect failed\n";
            return false;
        }

        // Blocked row must be refused while hold is active.
        if (m.connect(blocked_r, blocked_c))
        {
            std::cerr << "  interlock pattern " << s << ": blocked connect succeeded (hold active)\n";
            return false;
        }

        // Allowed row (not directly locked with hold_r) must succeed.
        if (allowed_r != blocked_r && allowed_r != hold_r)
        {
            if (!m.connect(allowed_r, allowed_c))
            {
                std::cerr << "  interlock pattern " << s << ": allowed connect failed\n";
                return false;
            }
            m.disconnect(allowed_r, allowed_c);
        }

        // Release hold; blocked row must now be permitted.
        m.disconnect(hold_r, hold_c);
        if (!m.connect(blocked_r, blocked_c))
        {
            std::cerr << "  interlock pattern " << s << ": blocked connect failed after hold released\n";
            return false;
        }
    }
    return true;
}

/* ==========================================================================
 * SIZES TEST — opt-in with --sizes
 * ========================================================================*/

/* Extra report content written here by test_pool_sizes(); flushed via Tee
 * in main() after the results table so it lands in TEST_REPORT.md. */
static std::string g_extra_report;

static bool test_pool_sizes()
{
    std::ostringstream ss;

    struct MatrixDim
    {
        uint8_t rows;
        uint8_t cols;
    };
    static const MatrixDim dims[] = {
        {1, 1}, {2, 8}, {4, 4}, {1, 12}, {4, 8}, {8, 8}, {8, 16}, {16, 16}};

    ss << "\n### Pool Sizes by Matrix Dimension (this platform)\n\n";
    ss << "| Matrix | Pool bits | Pool words | Pool bytes |\n";
    ss << "|--------|-----------|------------|------------|\n";
    for (std::size_t i = 0; i < sizeof(dims) / sizeof(dims[0]); ++i)
    {
        uint8_t r = dims[i].rows, c = dims[i].cols;
        uint32_t bits = (uint32_t)r * c + (uint32_t)r * r + c;
        uint16_t words = XPoint::poolWords(r, c);
        ss << "| " << (int)r << "x" << (int)c << " | " << bits << " | " << words << " | "
           << (uint32_t)(words * 4u) << " |\n";
    }

    ss << "\n### Object Sizes (this platform)\n\n";
    ss << "| Type | sizeof (bytes) |\n";
    ss << "|------|----------------|\n";
    ss << "| `XPoint` (base class) | " << sizeof(XPoint) << " |\n";
    ss << "| `BitPool` | " << sizeof(BitPool) << " |\n";
    ss << "| `PulseEvent` (packed) | " << sizeof(PulseEvent) << " |\n";
    ss << "| `XPointStatic<1,1>` | " << sizeof(XPointStatic<1, 1>) << " |\n";
    ss << "| `XPointStatic<2,8>` | " << sizeof(XPointStatic<2, 8>) << " |\n";
    ss << "| `XPointStatic<4,4>` | " << sizeof(XPointStatic<4, 4>) << " |\n";
    ss << "| `XPointStatic<8,8>` | " << sizeof(XPointStatic<8, 8>) << " |\n";
    ss << "| `XPointStatic<8,16>` | " << sizeof(XPointStatic<8, 16>) << " |\n";
    ss << "| `XPointStatic<16,16>` | " << sizeof(XPointStatic<16, 16>) << " |\n";

    g_extra_report = ss.str();
    return true;
}

/* ==========================================================================
 * RUNNER
 * ========================================================================*/

struct Test
{
    const char *name;
    bool (*fn)();
    uint8_t cat;
};

/* Derive the report path from this source file's directory. */
static std::string report_path()
{
    std::string f(__FILE__);
    std::size_t pos = f.rfind('/');
    std::size_t posw = f.rfind('\\');
    if (posw != std::string::npos && (pos == std::string::npos || posw > pos))
        pos = posw;
    if (pos == std::string::npos)
        return "test/TEST_REPORT.md";
    return f.substr(0, pos + 1) + "TEST_REPORT.md";
}

static bool has_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0)
            return true;
    return false;
}

int main(int argc, char **argv)
{
    /* --- category enable mask --- */
    uint8_t enabled = CAT_ALL;
    if (has_flag(argc, argv, "--skip-existing"))
        enabled &= (uint8_t)~CAT_EXISTING;
    if (has_flag(argc, argv, "--skip-limit"))
        enabled &= (uint8_t)~CAT_LIMIT;
    if (has_flag(argc, argv, "--skip-range") || has_flag(argc, argv, "--fast"))
        enabled &= (uint8_t)~CAT_RANGE;
    if (has_flag(argc, argv, "--skip-gap"))
        enabled &= (uint8_t)~CAT_GAP;
    if (has_flag(argc, argv, "--skip-json"))
        enabled &= (uint8_t)~CAT_JSON;
    if (has_flag(argc, argv, "--sizes"))
        enabled |= CAT_SIZES; // opt-in

    /* --- test table --- */
    Test tests[] = {
        // --- existing ---
        {"XPointStatic basic", test_xpointstatic_basic, CAT_EXISTING},
        {"user buffer constructor", test_user_buffer_constructor, CAT_EXISTING},
        {"I2CInterface begin() through ptr", test_i2c_interface_begin, CAT_EXISTING},
        {"latching connect pulse", test_latching_connect_pulse, CAT_EXISTING},
        {"latching disconnect pulse", test_latching_disconnect_pulse, CAT_EXISTING},
        {"latching rapid connect+disconnect", test_latching_rapid_connect_disconnect, CAT_EXISTING},
        {"nonlatching disconnect no spurious", test_nonlatching_disconnect_no_spurious_call, CAT_EXISTING},
        {"interlock", test_interlock, CAT_EXISTING},
        {"exclusive input", test_exclusive, CAT_EXISTING},
        {"MCP23017 driver", test_mcp23017_driver, CAT_EXISTING},
        {"TCA9548A transparent mux (MCP23017 via channel 3)", test_tca9548a_mux_transparent, CAT_EXISTING},
        {"DirectGPIO driver", test_direct_gpio_driver, CAT_EXISTING},
        {"ShiftRegister driver", test_shift_register_driver, CAT_EXISTING},
        {"TLC59711 packet", test_tlc59711_packet, CAT_EXISTING},
        {"TLC59711 setNodeHardware", test_tlc59711_set_node, CAT_EXISTING},
        {"setLevel binary driver", test_set_level_binary_driver, CAT_EXISTING},
        {"setLevel TLC59711 PWM", test_set_level_tlc59711, CAT_EXISTING},
        {"setLevel interlock", test_set_level_interlock, CAT_EXISTING},
        {"connect slot-full returns false", test_connect_slot_full_returns_false, CAT_EXISTING},
        {"disconnect slot-full returns false", test_disconnect_slot_full_returns_false, CAT_EXISTING},
        {"clearAll skips hardware when slot full", test_clearall_skips_hardware_when_slot_full, CAT_EXISTING},
        {"interlock desync blocked during RESET pulse", test_interlock_desync_blocked, CAT_EXISTING},
        {"exclusive desync blocked during RESET pulse", test_exclusive_desync_blocked, CAT_EXISTING},
        {"setLevel latching registers pulse", test_setlevel_latching_registers_pulse, CAT_EXISTING},
        {"setLevel latching slot-full returns false", test_setlevel_latching_slot_full_returns_false, CAT_EXISTING},
        // --- boundary / limit ---
        {"bounds: degenerate 0-dim matrices", test_bounds_degenerate, CAT_LIMIT},
        {"bounds: out-of-range indices return false", test_bounds_out_of_range, CAT_LIMIT},
        {"lockRows self-interlock noop", test_lockrows_self_noop, CAT_LIMIT},
        {"pdur=0 fires on first update()", test_pdur_zero_fires_immediately, CAT_LIMIT},
        {"millis() 32-bit rollover", test_millis_rollover_uint32, CAT_LIMIT},
        {"no driver attached — no crash", test_no_driver_no_crash, CAT_LIMIT},
        // --- full range ---
        {"all matrix sizes 0x0 to 255x255", test_all_matrix_sizes, CAT_RANGE},
        // --- gap coverage ---
        {"gap: clearAll SET-pulse cancel and reuse", test_clearall_set_pulse_cancel_and_reuse, CAT_GAP},
        {"gap: setLevel latching disconnect path", test_setlevel_latching_disconnect_path, CAT_GAP},
        {"gap: setLevel latching idempotent", test_setlevel_latching_idempotent, CAT_GAP},
        {"gap: clearAll non-latching batch commit", test_clearall_nonlatching, CAT_GAP},
        {"gap: interlock fanout (row 0 to rows 1&2)", test_interlock_fanout, CAT_GAP},
        {"gap: setLevel respects exclusiveInput", test_setlevel_exclusive, CAT_GAP},
        {"gap: update() noop in non-latching mode", test_update_noop_nonlatching, CAT_GAP},
        {"gap: connect idempotent with driver", test_connect_idempotent_with_driver, CAT_GAP},
        {"gap: disconnect idempotent with driver", test_disconnect_idempotent_with_driver, CAT_GAP},
        {"gap: MCP23017 port B (pins 8-15)", test_mcp23017_port_b, CAT_GAP},
        {"gap: MCP23017 shadow register accumulation", test_mcp23017_shadow_accumulation, CAT_GAP},
        {"gap: ShiftRegister byte 1 bit packing", test_shift_register_byte1, CAT_GAP},
        {"gap: TLC59711 2-chip daisy-chain ordering", test_tlc59711_multichip_ordering, CAT_GAP},
        // --- JSON-driven ---
        {"json: slot-full scenarios", test_slot_full_scenarios_json, CAT_JSON},
        {"json: desync scenarios", test_desync_scenarios_json, CAT_JSON},
        {"json: interlock patterns", test_interlock_patterns_json, CAT_JSON},
        // --- sizes (opt-in) ---
        {"sizes: pool/object sizes (this platform)", test_pool_sizes, CAT_SIZES},
    };
    const int total = (int)(sizeof(tests) / sizeof(tests[0]));

    /* --- count tests per category (for report header) --- */
    int cat_total[6] = {};
    int cat_skipped[6] = {};
    const uint8_t cats[] = {CAT_EXISTING, CAT_LIMIT, CAT_RANGE, CAT_GAP, CAT_JSON, CAT_SIZES};
    for (auto &t : tests)
        for (int k = 0; k < 6; ++k)
            if (t.cat == cats[k])
            {
                cat_total[k]++;
                if (!(enabled & cats[k]))
                    cat_skipped[k]++;
            }

    /* --- open tee (stdout + TEST_REPORT.md) --- */
    std::string rpath = report_path();
    Tee tee(rpath);
    if (!tee.ok())
        std::cerr << "warning: could not open " << rpath << " for writing\n";

    /* --- report header --- */
    std::time_t now = std::time(nullptr);
    char tbuf[32] = {};
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    tee << "# XPoint Test Report\n\n";
    tee << "**Generated:** " << tbuf << "\n\n";

    tee << "## Categories\n\n";
    tee << "| Category   | Tests | Status  |\n";
    tee << "|------------|-------|---------|\n";
    for (int k = 0; k < 6; ++k)
    {
        const char *status = (enabled & cats[k]) ? "enabled" : "SKIPPED";
        tee << "| " << cat_name(cats[k]);
        for (int p = (int)std::strlen(cat_name(cats[k])); p < 10; ++p)
            tee << " ";
        tee << " | " << cat_total[k];
        tee << "     | " << status << " |\n";
    }
    tee << "\n## Results\n\n";
    tee << "| # | Status | Test |\n";
    tee << "|---|--------|------|\n";

    /* --- run tests --- */
    int failures = 0, skipped = 0, row = 0;
    for (auto &t : tests)
    {
        ++row;
        g_millis = 0;
        if (!(enabled & t.cat))
        {
            tee << "| " << row << " | SKIP | " << t.name << " |\n";
            ++skipped;
            continue;
        }
        bool passed = t.fn();
        tee << "| " << row << " | " << (passed ? "PASS" : "FAIL") << " | " << t.name << " |\n";
        if (!passed)
            ++failures;
    }

    /* --- summary --- */
    tee << "\n## Summary\n\n";
    tee << (total - skipped) << " tests run, " << skipped << " skipped, **" << failures << " failure(s)**\n";

    /* --- extra sections injected by tests (e.g. size measurements) --- */
    if (!g_extra_report.empty())
        tee << "\n## Size Measurements\n" << g_extra_report;

    return failures;
}
