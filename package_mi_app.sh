#!/bin/bash
#
# openfpgaOS SDK — mi_app Packager
#
# Builds the release (make release → build/sdk/) and packages it into a
# distributable ZIP that can be extracted directly to an Analogue Pocket
# SD card.
#
# build/sdk/ already contains only the mi_app artefacts:
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
DIST="$SDK_DIR/dist/sdk"

BUILD="$SDK_DIR/build/sdk"
RELEASES="$SDK_DIR/releases"

# ── Build / refresh build/sdk/ ────────────────────────────────────
echo -e "${CYAN}=== mi_app Packager ===${RESET}"
echo "  Building release..."
make -C "$SDK_DIR" release

# ── Verify build/sdk/ is ready ───────────────────────────────────
if [ ! -d "$BUILD/Cores" ]; then
    echo "Error: build/sdk/ not found after make release."
    exit 1
fi

# ── Read metadata from the assembled core.json ───────────────────
CORE_NAME=$(ls "$BUILD/Cores/" 2>/dev/null | head -1)
if [ -z "$CORE_NAME" ]; then
    echo "Error: no core found in build/sdk/Cores/. Run 'make release' first."
    exit 1
fi

CORE_JSON="$BUILD/Cores/$CORE_NAME/core.json"
if [ ! -f "$CORE_JSON" ]; then
    echo "Error: $CORE_JSON not found."
    exit 1
fi

GAME_NAME=$(python3 -c "
import json, sys
with open('$CORE_JSON') as f:
    d = json.load(f)
print(d['core']['metadata']['description'])
" 2>/dev/null)
[ -z "$GAME_NAME" ] && { echo "Warning: could not read description from $CORE_JSON, using 'mi_app'"; GAME_NAME="mi_app"; }

CORE_VERSION=$(python3 -c "
import json, sys
with open('$CORE_JSON') as f:
    d = json.load(f)
print(d['core']['metadata']['version'])
" 2>/dev/null)
[ -z "$CORE_VERSION" ] && { echo "Warning: could not read version from $CORE_JSON, using '1.0.0'"; CORE_VERSION="1.0.0"; }

OUTPUT_ZIP="$RELEASES/mi_app-v${CORE_VERSION}.zip"

echo "  Version : $CORE_VERSION"
echo "  Output  : $OUTPUT_ZIP"
echo

# ── Generate INSTALL.txt inside build/sdk/ ───────────────────────
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

# ── Create ZIP from build/sdk/ ────────────────────────────────────
mkdir -p "$RELEASES"
rm -f "$OUTPUT_ZIP" 2>/dev/null || true

cd "$BUILD"
zip -r "$OUTPUT_ZIP" \
    Cores/ Assets/ Platforms/ INSTALL.txt \
    -x "*.DS_Store" "Thumbs.db" 2>/dev/null
cd "$SDK_DIR"

echo -e "${GREEN}Package created: $OUTPUT_ZIP${RESET}"
echo "  Size: $(du -h "$OUTPUT_ZIP" | cut -f1)"
