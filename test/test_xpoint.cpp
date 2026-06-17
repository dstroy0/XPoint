// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/*
 * XPoint unit test suite — host-native C++11 build; no Arduino hardware required.
 *
 * Run:
 *   g++ -std=c++11 -Itest test/test_xpoint.cpp -o test_xpoint && ./test_xpoint
 *
 * Custom parameterised run (see TESTS.md for all flags):
 *   ./test_xpoint --custom [--rows=N] [--cols=M] [--latching] [--pulse=N]
 */

#include <iostream>
#include <utility>
#include <vector>

#include "../src/I2CInterface.h"
#include "../src/XPoint.h"
#include "../src/XPointDriver.h"
#include "../src/drivers/DirectGPIODriver.h"
#include "../src/drivers/MCP23017Driver.h"
#include "../src/drivers/ShiftRegisterDriver.h"
#include "../src/drivers/TLC59711Driver.h"
#include "Arduino.h"

/* millis() shim — advanced by advanceMillis() and delay() in tests. */
static unsigned long g_millis = 0;
unsigned long millis()
{
    return g_millis;
}
void delay(unsigned long ms)
{
    g_millis += ms;
}
static void advanceMillis(unsigned long ms)
{
    g_millis += ms;
}

/* ---- Mock I2C -------------------------------------------------------------- */

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

/* ---- Mock driver ----------------------------------------------------------- */

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
    }
};

/* ---- Tests ----------------------------------------------------------------- */

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

/* Buffer constructor: user-supplied stack arrays; XPoint owns no heap. */
static bool test_user_buffer_constructor()
{
    bool stateBuf[4 * 4] = {};
    bool ilockBuf[4 * 4] = {};
    bool exclBuf[4] = {};

    MockDriver drv;
    XPoint m(4, 4, stateBuf, ilockBuf, exclBuf, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    if (!m.connect(1, 2))
        return false;
    if (drv.calls.size() != 1 || !drv.calls[0].state)
        return false;

    m.disconnect(1, 2);
    if (drv.calls.size() != 2 || drv.calls[1].state != false)
        return false;

    if (stateBuf[1 * 4 + 2] != false)
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

/*
 * Latching relay: disconnect() called while the SET pulse is still in-flight
 * must return false (coil busy).  Once update() fires releaseNode() and clears
 * the slot, disconnect() must succeed on the next attempt.
 */
static bool test_latching_rapid_connect_disconnect()
{
    MockDriver drv;
    XPoint m(4, 4, RE_LATCHING_DUAL_COIL, 20);
    m.setDriver(&drv);
    m.begin();

    g_millis = 0;
    m.connect(0, 0); // SET coil energised; pulse registered at t=0

    /* disconnect() while SET pulse is in-flight must be rejected. */
    advanceMillis(5);
    if (m.disconnect(0, 0)) // must return false — coil still pulsing
        return false;

    /* Pulse expires; update() calls releaseNode(). */
    advanceMillis(20);
    m.update();
    if (drv.releases.size() != 1)
        return false;

    /* Now disconnect() must succeed (slot is free). */
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

/* TLC59711: setNodeHardware() maps (r,c) → channel; full-on is 0xFFFF, full-off is 0x0000. */
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
 * setLevel() on a binary driver: default setNodeLevel() delegates to
 * setNodeHardware(true) for level>0 and setNodeHardware(false) for level==0.
 */
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

/* ---- Custom parameterised test --------------------------------------------- */

/*
 * Exercises XPoint logic for a user-specified matrix configuration with MockDriver.
 *
 * Invoked by passing --custom to the test binary:
 *   test_xpoint --custom [--rows=N] [--cols=M] [--latching] [--pulse=N]
 */

static const char *parse_arg(const char *arg, const char *key)
{
    if (arg[0] != '-' || arg[1] != '-')
        return nullptr;
    const char *a = arg + 2, *k = key;
    while (*k && *a == *k)
    {
        ++a;
        ++k;
    }
    if (*k != '\0')
        return nullptr;
    if (*a == '\0')
        return a;
    if (*a == '=')
        return a + 1;
    return nullptr;
}

static int parse_int(const char *s, int def)
{
    if (!s || *s == '\0')
        return def;
    int v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v;
}

static void ctest_report(bool pass, const char *name)
{
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  " << name << "\n";
}

/* Every node connects and disconnects; idempotent calls fire no extra driver call. */
static bool ctest_connect_all(uint8_t rows, uint8_t cols)
{
    MockDriver drv;
    XPoint m(rows, cols, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.begin();

    for (uint8_t r = 0; r < rows; ++r)
        for (uint8_t c = 0; c < cols; ++c)
            if (!m.connect(r, c))
                return false;

    size_t n = drv.calls.size();
    if (!m.connect(0, 0) || drv.calls.size() != n) // idempotent: no new call
        return false;

    for (uint8_t r = 0; r < rows; ++r)
        for (uint8_t c = 0; c < cols; ++c)
            if (!m.disconnect(r, c))
                return false;

    n = drv.calls.size();
    if (!m.disconnect(0, 0) || drv.calls.size() != n) // idempotent
        return false;

    return true;
}

/* setLevel routes through the driver; level>0 = on, level==0 = off. */
static bool ctest_setlevel(uint8_t rows, uint8_t cols)
{
    MockDriver drv;
    XPoint m(rows, cols, RE_NON_LATCHING, 0);
    m.setDriver(&drv);

    if (!m.setLevel(0, 0, 0x8000))
        return false;
    if (drv.calls.empty() || !drv.calls.back().state)
        return false;

    if (!m.setLevel(0, 0, 0))
        return false;
    if (drv.calls.empty() || drv.calls.back().state)
        return false;

    return true;
}

/* lockRows() blocks a second row from the same column while the first is connected. */
static bool ctest_interlock(uint8_t rows, uint8_t cols)
{
    if (rows < 2)
        return true;

    MockDriver drv;
    XPoint m(rows, cols, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.lockRows(0, 1);

    if (!m.connect(0, 0))
        return false;
    if (m.connect(1, 0))
        return false;
    m.disconnect(0, 0);
    if (!m.connect(1, 0))
        return false;

    return true;
}

/* exclusiveInput() restricts a column to exactly one row at a time. */
static bool ctest_exclusive(uint8_t rows, uint8_t cols)
{
    if (rows < 2)
        return true;

    MockDriver drv;
    XPoint m(rows, cols, RE_NON_LATCHING, 0);
    m.setDriver(&drv);
    m.exclusiveInput(0);

    if (!m.connect(0, 0))
        return false;
    if (m.connect(1, 0))
        return false;
    m.disconnect(0, 0);
    if (!m.connect(1, 0))
        return false;

    return true;
}

/*
 * connect() energises SET coil (state=true); disconnect() energises RESET coil
 * (state=false); update() calls releaseNode() after pulseDuration elapses.
 */
static bool ctest_latching(uint8_t rows, uint8_t cols, uint16_t pdur)
{
    MockDriver drv;
    XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
    m.setDriver(&drv);
    m.begin();

    g_millis = 0;

    if (!m.connect(0, 0))
        return false;
    if (drv.calls.empty() || !drv.calls.back().state)
        return false;

    advanceMillis((unsigned long)pdur + 1);
    m.update();
    if (drv.releases.empty())
        return false;

    drv.calls.clear();
    drv.releases.clear();

    if (!m.disconnect(0, 0))
        return false;
    if (drv.calls.empty() || drv.calls.back().state != false)
        return false;

    advanceMillis((unsigned long)pdur + 1);
    m.update();
    if (drv.releases.empty())
        return false;

    return true;
}

static int run_custom_test(int argc, char **argv)
{
    uint8_t rows = 4;
    uint8_t cols = 4;
    RelayType rtype = RE_NON_LATCHING;
    uint16_t pdur = 20;

    for (int i = 1; i < argc; ++i)
    {
        const char *v;
        if ((v = parse_arg(argv[i], "rows")))
            rows = (uint8_t)parse_int(v, 4);
        if ((v = parse_arg(argv[i], "cols")))
            cols = (uint8_t)parse_int(v, 4);
        if ((v = parse_arg(argv[i], "pulse")))
            pdur = (uint16_t)parse_int(v, 20);
        if (parse_arg(argv[i], "latching") != nullptr)
            rtype = RE_LATCHING_DUAL_COIL;
    }

    std::cout << "\nCustom test  " << (int)rows << " x " << (int)cols << "  "
              << (rtype == RE_LATCHING_DUAL_COIL ? "latching" : "non-latching");
    if (rtype == RE_LATCHING_DUAL_COIL)
        std::cout << "  pulse=" << (int)pdur << "ms";
    std::cout << "\n\n";

    bool r1 = ctest_connect_all(rows, cols);
    ctest_report(r1, "connect / disconnect all nodes (idempotent)");

    bool r2 = ctest_setlevel(rows, cols);
    ctest_report(r2, "setLevel connect + disconnect");

    bool r3 = ctest_interlock(rows, cols);
    if (rows >= 2)
        ctest_report(r3, "interlock lockRows(0,1)");
    else
        std::cout << "  SKIP  interlock (rows < 2)\n";

    bool r4 = ctest_exclusive(rows, cols);
    if (rows >= 2)
        ctest_report(r4, "exclusive input col 0");
    else
        std::cout << "  SKIP  exclusive (rows < 2)\n";

    bool r5 = true;
    if (rtype == RE_LATCHING_DUAL_COIL)
    {
        r5 = ctest_latching(rows, cols, pdur);
        ctest_report(r5, "latching relay SET + RESET pulse");
    }

    bool all = r1 && r2 && r3 && r4 && r5;
    std::cout << "\nCustom test " << (all ? "PASSED" : "FAILED") << "\n\n";
    return all ? 0 : 1;
}

/* ---- Runner ---------------------------------------------------------------- */

struct Test
{
    const char *name;
    bool (*fn)();
};

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (parse_arg(argv[i], "custom") != nullptr)
            return run_custom_test(argc, argv);

    Test tests[] = {
        {"XPointStatic basic", test_xpointstatic_basic},
        {"user buffer constructor", test_user_buffer_constructor},
        {"I2CInterface begin() through ptr", test_i2c_interface_begin},
        {"latching connect pulse", test_latching_connect_pulse},
        {"latching disconnect pulse", test_latching_disconnect_pulse},
        {"latching rapid connect+disconnect", test_latching_rapid_connect_disconnect},
        {"nonlatching disconnect no spurious", test_nonlatching_disconnect_no_spurious_call},
        {"interlock", test_interlock},
        {"exclusive input", test_exclusive},
        {"MCP23017 driver", test_mcp23017_driver},
        {"DirectGPIO driver", test_direct_gpio_driver},
        {"ShiftRegister driver", test_shift_register_driver},
        {"TLC59711 packet", test_tlc59711_packet},
        {"TLC59711 setNodeHardware", test_tlc59711_set_node},
        {"setLevel binary driver", test_set_level_binary_driver},
        {"setLevel TLC59711 PWM", test_set_level_tlc59711},
        {"setLevel interlock", test_set_level_interlock},
    };

    int failures = 0;
    for (auto &t : tests)
    {
        g_millis = 0;
        bool passed = t.fn();
        std::cout << (passed ? "PASS" : "FAIL") << "  " << t.name << "\n";
        if (!passed)
            ++failures;
    }

    std::cout << "\n" << (sizeof(tests) / sizeof(tests[0])) << " tests, " << failures << " failure(s)\n";
    return failures;
}
