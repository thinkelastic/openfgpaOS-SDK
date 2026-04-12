#!/bin/bash
#
# openfpgaOS SDK — Copy to Pocket SD Card
#
# Copies build/sdk/ (the complete SD card image) to the mounted SD card.
# Run 'make' from src/apps/ first to assemble build/sdk/.
#
# Usage:
#   ./scripts/copy.sh                       Auto-detect Pocket SD card
#   ./scripts/copy.sh /mnt/sdcard           Copy to specific path
#

set -e

GREEN='\033[92m'
RED='\033[91m'
RESET='\033[0m'

SDK_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$SDK_DIR/build/sdk"

# ── Check build exists ───────────────────────────────────────────────
if [ ! -d "$BUILD_DIR/Cores" ]; then
    echo "Error: build/sdk/ not found. Run 'make' from src/apps/ first."
    exit 1
fi

# ── Find / mount SD card ─────────────────────────────────────────────
SDCARD="$1"
source "$(dirname "$0")/sdcard.sh"

echo "Deploying build/sdk/ to $SDCARD"

# ── Copy ─────────────────────────────────────────────────────────────
cp -r "$BUILD_DIR"/Cores/* "$SDCARD/Cores/" 2>/dev/null
echo -e "  ${GREEN}+${RESET} Cores"

cp -r "$BUILD_DIR"/Assets/* "$SDCARD/Assets/" 2>/dev/null
echo -e "  ${GREEN}+${RESET} Assets"

cp -r "$BUILD_DIR"/Platforms/* "$SDCARD/Platforms/" 2>/dev/null
echo -e "  ${GREEN}+${RESET} Platforms"

sync 2>/dev/null || true
echo -e "\n${GREEN}Deployed!${RESET} Eject SD card and boot your Pocket."
