#!/bin/bash
#
# openfpgaOS SDK Setup
# Detects the RISC-V toolchain and validates the build environment.
#

set -e

echo "openfpgaOS SDK Setup"
echo "=============="
echo

# Detect toolchain
if command -v riscv64-unknown-elf-gcc &>/dev/null; then
    CROSS=riscv64-unknown-elf-
elif command -v riscv64-elf-gcc &>/dev/null; then
    CROSS=riscv64-elf-
else
    echo "ERROR: No RISC-V toolchain found!"
    echo
    echo "Install one of:"
    echo "  Arch:   pacman -S riscv64-elf-gcc"
    echo "  macOS:  brew install riscv64-elf-gcc"
    echo "  Ubuntu: apt install gcc-riscv64-unknown-elf"
    echo "  Manual: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack"
    exit 1
fi

CC="${CROSS}gcc"
CXX="${CROSS}g++"
echo "Toolchain:  ${CROSS}"
echo "C compiler: $($CC --version | head -1)"

# Check rv32imafc support
if ! $CC -march=rv32imafc -mabi=ilp32f -x c -c /dev/null -o /dev/null 2>/dev/null; then
    echo "ERROR: Toolchain does not support rv32imafc / ilp32f"
    exit 1
fi
echo "Target:     rv32imafc (ilp32f)"

# Check C++ compiler
if command -v "$CXX" &>/dev/null; then
    echo "C++ compiler: $($CXX --version | head -1)"
else
    echo "WARNING:    ${CXX} not found -- C++ apps will not build"
fi

# Check for os.bin
if [ -f os.bin ]; then
    echo "OS binary:  os.bin ($(wc -c < os.bin) bytes)"
else
    echo "WARNING:    os.bin not found -- download from releases"
fi

echo
echo "Ready! Run 'make' to build your app."
