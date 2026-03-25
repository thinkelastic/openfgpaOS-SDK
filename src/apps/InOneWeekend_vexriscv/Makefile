# RayTracing InOneWeekend — VexRiscV / openfpgaOS port
#
# Target: VexRiscV RV32IMF core (rv32imafc, ABI ilp32f)
# SDK:    https://github.com/RndMnkIII/openfgpaOS-SDK
#
# ── Quick start ───────────────────────────────────────────────────────────
#
#   # Step 1: set SDK_DIR to where openfgpaOS-SDK is checked out
#   export SDK_DIR=/path/to/openfgpaOS-SDK/src/sdk
#
#   # Step 2: build for VexRiscV
#   make
#
#   # Step 3: build native PC version for correctness testing
#   make app_pc
#   ./app_pc > out.ppm
#   # View out.ppm with any PPM viewer (e.g. GIMP, feh, display)
#
# ── Full random-sphere scene ──────────────────────────────────────────────
#   make CXXFLAGS=-DFULL_SCENE     # embedded (very slow)
#   make app_pc CXXFLAGS=-DFULL_SCENE
#
# ── Custom resolution ────────────────────────────────────────────────────
#   The camera defaults to 160×90 with 4 samples.  Edit main.cpp to change.
# ─────────────────────────────────────────────────────────────────────────

# Path to the openfpgaOS SDK (override on command line or via $SDK_DIR)
# Default assumes the two repos are siblings:
#   <parent>/openfgpaOS-SDK/src/sdk
#   <parent>/raytracing.github.io/src/InOneWeekend_vexriscv
SDK_DIR ?= ../../../../openfgpaOS-SDK/src/sdk

# C++ source files (only one translation unit; all code is in headers)
SRCS_CXX := main.cpp

# Extra flags for the compat/ headers path
CXXFLAGS += -I.

include $(SDK_DIR)/sdk.mk

# ── PC / host build ───────────────────────────────────────────────────────
# Uses the system compiler; OF_PC makes each SDK compat header include_next
# to the real system header.  No SDL2 needed — output is plain-text PPM.
PC_CC  ?= c++
PC_OUT  = app_pc

app_pc: main.cpp
	$(PC_CC) -DOF_PC -I. -O2 -Wall -Wextra -std=c++14 \
	    $(CXXFLAGS) main.cpp -lm -o $(PC_OUT)

.PHONY: app_pc clean-pc

clean-pc:
	rm -f $(PC_OUT)

sdk-clean: clean-pc
