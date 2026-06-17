#!/usr/bin/env bash
set -euo pipefail

ROOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOTDIR/test_xpoint"
REPORT="$ROOTDIR/report.txt"

echo "Removing binary and report..."
rm -f "$OUT" "$REPORT"
echo "Done."
