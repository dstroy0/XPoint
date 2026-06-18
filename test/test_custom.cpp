// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/*
 * XPoint parametric test — exercises core logic for a user-specified matrix
 * configuration.  Build and run separately from the main test suite.
 *
 * Build:
 *   g++ -std=c++11 -Isrc -Itest \
 *       test/test_custom.cpp \
 *       src/XPoint.cpp src/drivers/ShiftRegisterDriver.cpp \
 *       src/drivers/DirectGPIODriver.cpp src/drivers/MCP23017Driver.cpp \
 *       src/drivers/TLC59711Driver.cpp \
 *       -o test_custom
 *
 * Usage:
 *   ./test_custom [--rows=N] [--cols=M] [--latching] [--pulse=N]
 *
 * Defaults: 4×4, non-latching, pulse=20 ms.
 */

#include <iostream>
#include <utility>
#include <vector>

#include "../src/XPoint.h"
#include "../src/drivers/DirectGPIODriver.h"
#include "../src/drivers/I2CInterface.h"
#include "../src/drivers/MCP23017Driver.h"
#include "../src/drivers/ShiftRegisterDriver.h"
#include "../src/drivers/TLC59711Driver.h"
#include "../src/drivers/XPointDriver.h"
#include "Arduino.h"

/* ---- millis() shim -------------------------------------------------------- */
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

/* ---- CLI helpers ---------------------------------------------------------- */

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

static void report(bool pass, const char *name)
{
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  " << name << "\n";
}

/* ---- Parametric sub-tests ------------------------------------------------- */

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

    std::size_t n = drv.calls.size();
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

/* lockRows() blocks a second row from the same column while the first holds it. */
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

/* Latching: connect() energizes SET coil; disconnect() energizes RESET coil;
 * update() calls releaseNode() after pulseDuration elapses. */
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

    advanceMillis((uint32_t)pdur + 1u);
    m.update();
    if (drv.releases.empty())
        return false;

    drv.calls.clear();
    drv.releases.clear();

    if (!m.disconnect(0, 0))
        return false;
    if (drv.calls.empty() || drv.calls.back().state != false)
        return false;

    advanceMillis((uint32_t)pdur + 1u);
    m.update();
    if (drv.releases.empty())
        return false;

    return true;
}

/* Latching: pulse-table full returns false without energizing a coil. */
static bool ctest_slot_full(uint8_t rows, uint8_t cols, uint16_t pdur)
{
    if ((int)rows * (int)cols < 9)
        return true; // not enough nodes to fill 8 slots and have a 9th

    MockDriver drv;
    XPoint m(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
    m.setDriver(&drv);
    m.begin();
    g_millis = 0;

    // Fill 8 slots.
    uint8_t nr = 0, nc = 0;
    int filled = 0;
    for (uint8_t r = 0; r < rows && filled < 8; ++r)
        for (uint8_t c = 0; c < cols && filled < 8; ++c)
        {
            m.connect(r, c);
            nr = r;
            nc = c;
            ++filled;
        }

    // Find a 9th node not yet connected.
    bool found9 = false;
    uint8_t r9 = 0, c9 = 0;
    for (uint8_t r = 0; r < rows && !found9; ++r)
        for (uint8_t c = 0; c < cols && !found9; ++c)
        {
            bool isFilledNode = false;
            int idx = 0;
            for (uint8_t fr = 0; fr < rows && idx < 8; ++fr)
                for (uint8_t fc = 0; fc < cols && idx < 8; ++fc, ++idx)
                    if (fr == r && fc == c)
                        isFilledNode = true;
            if (!isFilledNode && !(r == nr && c == nc))
            {
                r9 = r;
                c9 = c;
                found9 = true;
            }
        }

    // Simpler: just try the 9th in row-major order after the first 8.
    // Reset and redo with a direct approach.
    (void)nr;
    (void)nc;
    (void)r9;
    (void)c9;
    (void)found9;

    MockDriver drv2;
    XPoint m2(rows, cols, RE_LATCHING_DUAL_COIL, pdur);
    m2.setDriver(&drv2);
    m2.begin();
    g_millis = 0;

    int count = 0;
    uint8_t last_r = 0, last_c = 0;
    for (uint8_t r = 0; r < rows; ++r)
    {
        for (uint8_t c = 0; c < cols; ++c)
        {
            if (count < 8)
            {
                m2.connect(r, c);
                ++count;
            }
            else
            {
                last_r = r;
                last_c = c;
                goto done_fill;
            }
        }
    }
done_fill:

    std::size_t before = drv2.calls.size();
    bool result = m2.connect(last_r, last_c);
    if (result)
        return false; // must be false
    if (drv2.calls.size() != before)
        return false; // no hardware call

    return true;
}

/* ---- Runner --------------------------------------------------------------- */

int main(int argc, char **argv)
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
    report(r1, "connect / disconnect all nodes (idempotent)");

    bool r2 = ctest_setlevel(rows, cols);
    report(r2, "setLevel connect + disconnect");

    bool r3 = true;
    if (rows >= 2)
    {
        r3 = ctest_interlock(rows, cols);
        report(r3, "interlock lockRows(0,1)");
    }
    else
    {
        std::cout << "  SKIP  interlock (rows < 2)\n";
    }

    bool r4 = true;
    if (rows >= 2)
    {
        r4 = ctest_exclusive(rows, cols);
        report(r4, "exclusive input col 0");
    }
    else
    {
        std::cout << "  SKIP  exclusive (rows < 2)\n";
    }

    bool r5 = true;
    if (rtype == RE_LATCHING_DUAL_COIL)
    {
        r5 = ctest_latching(rows, cols, pdur);
        report(r5, "latching relay SET + RESET pulse");

        bool r5b = ctest_slot_full(rows, cols, pdur);
        report(r5b, "latching slot-full returns false");
        r5 = r5 && r5b;
    }

    bool all = r1 && r2 && r3 && r4 && r5;
    std::cout << "\nCustom test " << (all ? "PASSED" : "FAILED") << "\n\n";
    return all ? 0 : 1;
}
