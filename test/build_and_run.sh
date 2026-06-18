#!/usr/bin/env bash
set -euo pipefail

# Build, run tests, write TEST_REPORT.md, and open it (Linux/macOS)
ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOTDIR/test_xpoint"
OUT_CUSTOM="$ROOTDIR/test_custom"
REPORT="$ROOTDIR/test/TEST_REPORT.md"

SRCS="$ROOTDIR/src/XPoint.cpp
$ROOTDIR/src/drivers/BitPool.cpp
$ROOTDIR/src/drivers/ShiftRegisterDriver.cpp
$ROOTDIR/src/drivers/DirectGPIODriver.cpp
$ROOTDIR/src/drivers/MCP23017Driver.cpp
$ROOTDIR/src/drivers/TLC59711Driver.cpp"

echo "Building host tests..."
g++ -std=c++11 -Wall -Wextra -Wpedantic \
    -I"$ROOTDIR/src" -I"$ROOTDIR/test" \
    "$ROOTDIR/test/test_xpoint.cpp" \
    $SRCS \
    -o "$OUT"

echo "Building custom test binary..."
g++ -std=c++11 -Wall -Wextra -Wpedantic \
    -I"$ROOTDIR/src" -I"$ROOTDIR/test" \
    "$ROOTDIR/test/test_custom.cpp" \
    $SRCS \
    -o "$OUT_CUSTOM"

echo "Running unit tests..."
"$OUT" "$@"          # forward any --skip-* flags
UNIT_EXIT=$?

if [ $UNIT_EXIT -ne 0 ]; then
    echo "Unit tests failed (exit $UNIT_EXIT)"
    exit $UNIT_EXIT
fi

echo "Running custom tests..."
"$OUT_CUSTOM" --rows=4  --cols=4
"$OUT_CUSTOM" --rows=8  --cols=8  --latching --pulse=20
"$OUT_CUSTOM" --rows=1  --cols=16 --latching --pulse=50

# Open TEST_REPORT.md in the default viewer
if command -v xdg-open &>/dev/null; then
    xdg-open "$REPORT"
elif command -v open &>/dev/null; then
    open "$REPORT"
else
    echo "Report written to: $REPORT"
fi
