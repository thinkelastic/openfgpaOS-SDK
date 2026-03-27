# RayTracing InOneWeekend — VexRiscV / openfpgaOS port
#
# Target: VexRiscV RV32IMF core (rv32imafc, ABI ilp32f)
# SDK:    https://github.com/RndMnkIII/openfgpaOS-SDK
#
# ── Quick start ───────────────────────────────────────────────────────────
#
#   # Step 1: build for VexRiscV (from the SDK root):
#   make APP=InOneWeekend_vexriscv
#
#   # Step 2: build native PC version for correctness testing
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

# Path to the openfpgaOS SDK — relative to this directory inside the repo
SDK_DIR = ../sdk

# C++ source files (only one translation unit; all code is in headers)
SRCS_CXX := main.cpp

# Extra flags for the compat/ headers path
CXXFLAGS += -I.

include $(SDK_DIR)/sdk.mk

# Explicit CRT build rule (pattern rule does not cross directories)
$(CRT_DIR)/start.o: $(CRT_DIR)/start.S
	$(AS) $(ASFLAGS) -c -o $@ $<

all: app.elf
	$(SIZE) $<

# ── PC / host build ───────────────────────────────────────────────────────
# Uses the system compiler; OF_PC makes each SDK compat header include_next
# to the real system header.  No SDL2 needed — output is plain-text PPM.
PC_CC  ?= c++
PC_OUT  = app_pc

app_pc: main.cpp
	$(PC_CC) -DOF_PC -I. -O2 -Wall -Wextra -std=c++14 \
	    $(CXXFLAGS) main.cpp -lm -o $(PC_OUT)

.PHONY: all app_pc clean-pc clean

clean-pc:
	rm -f $(PC_OUT)

sdk-clean: clean-pc

clean: sdk-clean
