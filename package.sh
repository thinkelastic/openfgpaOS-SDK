#!/bin/bash
#
# openfpgaOS SDK — Release Packager
#
# Packages a custom game core or the SDK release into a distributable ZIP.
#
# Usage:
#   ./package.sh                    Package the SDK (build/sdk/)
#   ./package.sh <ShortName>        Package a custom core (dist/<ShortName>/)
#

set -e

GREEN='\033[92m'
CYAN='\033[96m'
RESET='\033[0m'

SDK_DIR="$(cd "$(dirname "$0")" && pwd)"
CUSTOM_NAME="$1"

if [ -n "$CUSTOM_NAME" ]; then
    # Package a custom core from dist/<ShortName>/
    INPUT="$SDK_DIR/dist/$CUSTOM_NAME"
    if [ ! -d "$INPUT/Cores" ]; then
        echo "Error: dist/$CUSTOM_NAME/Cores not found"
        echo "Run ./customize.sh first to create it."
        exit 1
    fi
    OUTPUT="$SDK_DIR/releases/${CUSTOM_NAME}.zip"
else
    # Package the SDK release
    INPUT="$SDK_DIR/build/sdk"
    if [ ! -d "$INPUT/Cores" ]; then
        echo "Error: build/sdk/ not found"
        echo "Run 'make' first to create it."
        exit 1
    fi
    OUTPUT="$SDK_DIR/releases/openfpgaOS-SDK.zip"
fi

# Detect core name
CORE_NAME=$(ls "$INPUT/Cores/" 2>/dev/null | head -1)
[[ -z "$CORE_NAME" ]] && { echo "Error: no core found in $INPUT/Cores/"; exit 1; }

# Read metadata for INSTALL.txt
GAME_NAME=$(python3 -c "
import json, sys
with open('$INPUT/Cores/$CORE_NAME/core.json') as f:
    d = json.load(f)
print(d['core']['metadata']['description'])
" 2>/dev/null || echo "$CORE_NAME")

GAME_VERSION=$(python3 -c "
import json, sys
with open('$INPUT/Cores/$CORE_NAME/core.json') as f:
    d = json.load(f)
print(d['core']['metadata']['version'])
" 2>/dev/null || echo "1.0.0")

# Generate INSTALL.txt
cat > "$INPUT/INSTALL.txt" << EOF
$GAME_NAME
$(printf '=%.0s' $(seq 1 ${#GAME_NAME}))

Version: $GAME_VERSION
Platform: openfpgaOS

Installation:
1. Extract this ZIP to your Analogue Pocket SD card root
2. Merge with existing folders if prompted
3. The game will appear in the Pocket menu

Save files are created automatically on first use.
EOF

# Create output directory
mkdir -p "$(dirname "$OUTPUT")"

# Create ZIP
cd "$INPUT"
rm -f "$OUTPUT" 2>/dev/null || true
zip -r "$OUTPUT" \
    Cores/ Assets/ Platforms/ INSTALL.txt \
    -x "*.DS_Store" "Thumbs.db" 2>/dev/null

cd "$SDK_DIR"

echo
echo -e "${GREEN}Package created: $OUTPUT${RESET}"
echo "  Size: $(du -h "$OUTPUT" | cut -f1)"
