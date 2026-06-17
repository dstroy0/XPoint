#!/usr/bin/env bash
# XPoint custom parameterised test runner.
#
# Builds test_xpoint and invokes it with --custom and the chosen matrix
# configuration. If arguments are omitted the script prompts interactively.
#
# Usage:
#   ./test/run_custom_test.sh [--driver=<profile>] [--rows=N] [--cols=M]
#                             [--latching] [--pulse=N]
#
# Profiles:
#   gpio            Arduino Direct GPIO       4 x 4  non-latching
#   shift_register  74HC595 Shift Register    8 x 8  non-latching
#   mcp23017        MCP23017 I2C Expander     2 x 8  non-latching
#   tlc59711        TLC59711 PWM Expander     1 x 12 non-latching
#   latching        Dual-coil latching relay  4 x 4  latching (20 ms)
#   custom          Specify every value manually
#
# Examples:
#   ./test/run_custom_test.sh --driver=shift_register
#   ./test/run_custom_test.sh --driver=custom --rows=6 --cols=6
#   ./test/run_custom_test.sh --driver=latching --rows=8 --cols=8 --pulse=50

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"

DRIVER=""
ROWS=0
COLS=0
LATCHING=0
PULSE=20

for arg in "$@"; do
    case "$arg" in
        --driver=*)  DRIVER="${arg#*=}" ;;
        --rows=*)    ROWS="${arg#*=}" ;;
        --cols=*)    COLS="${arg#*=}" ;;
        --latching)  LATCHING=1 ;;
        --pulse=*)   PULSE="${arg#*=}" ;;
        --help|-h)
            sed -n '2,/^[^#]/{ /^#/{ s/^# \?//; p }; /^[^#]/q }' "$0"
            exit 0 ;;
    esac
done

# ---------------------------------------------------------------------------
# Profile selection
# ---------------------------------------------------------------------------
apply_profile() {
    case "$1" in
        gpio)           [ "$ROWS" -eq 0 ] && ROWS=4;  [ "$COLS" -eq 0 ] && COLS=4  ;;
        shift_register) [ "$ROWS" -eq 0 ] && ROWS=8;  [ "$COLS" -eq 0 ] && COLS=8  ;;
        mcp23017)       [ "$ROWS" -eq 0 ] && ROWS=2;  [ "$COLS" -eq 0 ] && COLS=8  ;;
        tlc59711)       [ "$ROWS" -eq 0 ] && ROWS=1;  [ "$COLS" -eq 0 ] && COLS=12 ;;
        latching)       [ "$ROWS" -eq 0 ] && ROWS=4;  [ "$COLS" -eq 0 ] && COLS=4; LATCHING=1 ;;
        custom)         ;;
        *) echo "Unknown profile: $1"; echo "Valid: gpio shift_register mcp23017 tlc59711 latching custom"; exit 1 ;;
    esac
}

if [ -z "$DRIVER" ]; then
    echo ""
    echo "XPoint custom test — select a hardware profile"
    echo "----------------------------------------------"
    echo "  1)  gpio             Arduino Direct GPIO       4 x 4  non-latching"
    echo "  2)  shift_register   74HC595 Shift Register    8 x 8  non-latching"
    echo "  3)  mcp23017         MCP23017 I2C Expander     2 x 8  non-latching"
    echo "  4)  tlc59711         TLC59711 PWM Expander     1 x 12 non-latching"
    echo "  5)  latching         Dual-coil latching relay  4 x 4  latching (20 ms)"
    echo "  6)  custom           Specify every value manually"
    echo ""
    read -rp "Profile (name or 1-6): " CHOICE
    case "$CHOICE" in
        1) DRIVER=gpio ;;
        2) DRIVER=shift_register ;;
        3) DRIVER=mcp23017 ;;
        4) DRIVER=tlc59711 ;;
        5) DRIVER=latching ;;
        6) DRIVER=custom ;;
        *) DRIVER="$CHOICE" ;;
    esac
fi

apply_profile "$DRIVER"

# Prompt for any values still at zero
if [ "$ROWS" -eq 0 ]; then
    read -rp "Rows: " ROWS
fi
if [ "$COLS" -eq 0 ]; then
    read -rp "Cols: " COLS
fi
if [ "$LATCHING" -eq 0 ]; then
    read -rp "Latching relay mode? [y/N]: " LATCH_ANS
    [[ "${LATCH_ANS:-N}" =~ ^[Yy]$ ]] && LATCHING=1 || true
fi
if [ "$LATCHING" -eq 1 ] && [ "$PULSE" -eq 20 ]; then
    read -rp "Pulse duration ms [20]: " PD_ANS
    [ -n "${PD_ANS:-}" ] && PULSE="$PD_ANS" || true
fi

MODE_STR="non-latching"
[ "$LATCHING" -eq 1 ] && MODE_STR="latching  pulse=${PULSE}ms"

echo ""
echo "Profile   : $DRIVER"
echo "Matrix    : $ROWS x $COLS"
echo "Mode      : $MODE_STR"
echo ""

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
EXE="$ROOT/test_xpoint_custom"

echo "Building..."
cd "$ROOT"
g++ -std=c++11 -Isrc -Itest \
    test/test_xpoint.cpp \
    src/XPoint.cpp \
    src/drivers/DirectGPIODriver.cpp \
    src/drivers/ShiftRegisterDriver.cpp \
    src/drivers/MCP23017Driver.cpp \
    src/drivers/TLC59711Driver.cpp \
    -o "$EXE"

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
EXTRA_ARGS=""
if [ "$LATCHING" -eq 1 ]; then
    EXTRA_ARGS="--latching --pulse=$PULSE"
fi

# shellcheck disable=SC2086
"$EXE" --custom --rows="$ROWS" --cols="$COLS" $EXTRA_ARGS
RC=$?

rm -f "$EXE"
exit $RC
