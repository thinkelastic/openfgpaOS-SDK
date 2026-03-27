#!/bin/bash
#
# openfpgaOS SDK — mi_app Packager
#
# Assembles and packages only mi_app into a distributable ZIP that can be
# extracted directly to an Analogue Pocket SD card.
#
# The ZIP contains the full structure required by the Pocket firmware:
#   Cores/ThinkElastic.openfpgaOS/   — bitstream, loader, JSON configs
#   Assets/openfpgaos/common/        — os.bin + mi_app.elf (+ any data files)
#   Assets/openfpgaos/ThinkElastic.openfpgaOS/ — mi_app instance JSON only
#   Platforms/                       — platform descriptor + image
#
# Usage:
#   ./package_mi_app.sh
#

set -e

GREEN='\033[92m'
CYAN='\033[96m'
RESET='\033[0m'

SDK_DIR="$(cd "$(dirname "$0")" && pwd)"
RUNTIME="$SDK_DIR/runtime"
DIST="$SDK_DIR/dist/sdk"
SRC_APP="$SDK_DIR/src/mi_app"

CORE_ID="ThinkElastic.openfpgaOS"
PLATFORM="openfpgaos"

BUILD="$SDK_DIR/build/mi_app"
RELEASES="$SDK_DIR/releases"

APP_ELF="$SRC_APP/app.elf"
INSTANCE_JSON="$DIST/instances/mi_app.json"

# ── Read version from core.json ───────────────────────────────────
CORE_VERSION=$(python3 -c "
import json
with open('$DIST/core/core.json') as f:
    d = json.load(f)
print(d['core']['metadata']['version'])
" 2>/dev/null || echo "1.0.0")

OUTPUT_ZIP="$RELEASES/mi_app-v${CORE_VERSION}.zip"

echo -e "${CYAN}=== mi_app Packager ===${RESET}"
echo "  Version : $CORE_VERSION"
echo "  Output  : $OUTPUT_ZIP"
echo

# ── Check required source files ───────────────────────────────────
check_file() {
    if [ -f "$1" ]; then
        echo "  ✓ $1"
    else
        echo "  ✗ $1 — not found"
        echo
        echo "Error: missing required file. Run 'make' first."
        exit 1
    fi
}

check_file "$APP_ELF"
check_file "$INSTANCE_JSON"
check_file "$RUNTIME/bitstream.rbf_r"
check_file "$RUNTIME/loader.bin"
check_file "$RUNTIME/os.bin"
echo

# ── Assemble build/mi_app/ directory structure ────────────────────
REL_CORE="$BUILD/Cores/$CORE_ID"
REL_ASSETS="$BUILD/Assets/$PLATFORM/common"
REL_INSTANCE="$BUILD/Assets/$PLATFORM/$CORE_ID"
REL_PLATFORM="$BUILD/Platforms"

rm -rf "$BUILD"
mkdir -p "$REL_CORE" "$REL_ASSETS" "$REL_INSTANCE" "$REL_PLATFORM/_images"

# Bitstream + loader
cp "$RUNTIME/bitstream.rbf_r" "$REL_CORE/"
cp "$RUNTIME/loader.bin"       "$REL_CORE/"

# Core JSON configs + icon
[ -d "$DIST/core" ] && cp "$DIST/core/"*.json "$DIST/core/"*.bin "$REL_CORE/" 2>/dev/null || true

# Platform files
[ -d "$DIST/platform" ] && cp "$DIST/platform/"*.json "$REL_PLATFORM/" 2>/dev/null || true
[ -d "$DIST/platform/_images" ] && cp "$DIST/platform/_images/"*.bin "$REL_PLATFORM/_images/" 2>/dev/null || true

# OS binary
cp "$RUNTIME/os.bin" "$REL_ASSETS/"

# mi_app ELF
cp "$APP_ELF" "$REL_ASSETS/mi_app.elf"

# Optional data files bundled with mi_app (.mid, .wav, .dat, .png)
find "$SRC_APP" -maxdepth 1 \
    \( -name "*.mid" -o -name "*.wav" -o -name "*.dat" -o -name "*.png" \) \
    -exec cp {} "$REL_ASSETS/" \; 2>/dev/null || true

# Instance JSON — mi_app only
cp "$INSTANCE_JSON" "$REL_INSTANCE/"

# ── Generate INSTALL.txt ──────────────────────────────────────────
GAME_NAME="mi_app"
cat > "$BUILD/INSTALL.txt" << EOF
$GAME_NAME
$(printf '=%.0s' $(seq 1 ${#GAME_NAME}))

Version: $CORE_VERSION

Installation:
1. Extract this ZIP to your Analogue Pocket SD card root
2. Merge with existing folders if prompted
3. The app will appear in the Pocket menu

Save files are created automatically on first use.
EOF

# ── Create ZIP ────────────────────────────────────────────────────
mkdir -p "$RELEASES"
rm -f "$OUTPUT_ZIP" 2>/dev/null || true

cd "$BUILD"
zip -r "$OUTPUT_ZIP" \
    Cores/ Assets/ Platforms/ INSTALL.txt \
    -x "*.DS_Store" "Thumbs.db" 2>/dev/null
cd "$SDK_DIR"

echo -e "${GREEN}Package created: $OUTPUT_ZIP${RESET}"
echo "  Size: $(du -h "$OUTPUT_ZIP" | cut -f1)"
