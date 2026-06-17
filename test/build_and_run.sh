#!/usr/bin/env bash
set -euo pipefail

# Build, run tests, and write a report (Linux/macOS)
ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOTDIR/test_xpoint"
REPORT="$ROOTDIR/report.txt"

echo "Building host tests..." | tee "$REPORT"
g++ -std=c++11 -Isrc -Itest \
    test/test_xpoint.cpp \
    src/XPoint.cpp \
    src/drivers/ShiftRegisterDriver.cpp \
    src/drivers/DirectGPIODriver.cpp \
    src/drivers/MCP23017Driver.cpp \
    src/drivers/TLC59711Driver.cpp \
    -o "$OUT" 2>&1 | tee -a "$REPORT"
BUILD_EXIT=${PIPESTATUS[0]}
if [ $BUILD_EXIT -ne 0 ]; then
    echo "Build failed with exit $BUILD_EXIT" | tee -a "$REPORT"
    exit $BUILD_EXIT
fi

echo "Running tests..." | tee -a "$REPORT"
"$OUT" 2>&1 | tee -a "$REPORT"
TEST_EXIT=${PIPESTATUS[0]}
echo "TEST EXIT CODE: $TEST_EXIT" | tee -a "$REPORT"
exit $TEST_EXIT
