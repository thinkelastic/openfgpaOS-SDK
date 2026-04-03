# MOD/XM/S3M/IT player demo for openfpgaOS Pocket
#
# Compiles libxmp-lite from the main libxmp repo.
# The lite sources live in src/lite/ as wrapper files (lite-*.c)
# that #define LIBXMP_CORE_PLAYER and #include the real src/*.c.
#
# Setup:
#   cd src/apps/moddemo
#   git clone https://github.com/libxmp/libxmp.git libxmp
#
# Build:  make

SDK_DIR = ../../sdk

# ── App sources ──────────────────────────────────────────────────
SRCS = main.c

# ── libxmp-lite from main repo ───────────────────────────────────
# The repo structure is:
#   libxmp/
#     include/xmp.h            <- public API header
#     src/                     <- main sources (*.c)
#     src/loaders/             <- loader sources
#     src/lite/                <- lite wrappers (lite-*.c)
#     src/lite/Makefile         <- defines LITE object list
#
LIBXMP_DIR   = $(CURDIR)/libxmp
LIBXMP_LITE  = $(LIBXMP_DIR)/src/lite

# This list comes directly from libxmp/src/lite/Makefile.
# Each lite-*.c is a thin wrapper that does:
#   #define LIBXMP_CORE_PLAYER
#   #include "../original.c"
# So we only compile these — NOT the files in src/ directly.
LIBXMP_LITE_SRCS = \
	$(LIBXMP_LITE)/lite-virtual.c \
	$(LIBXMP_LITE)/lite-format.c \
	$(LIBXMP_LITE)/lite-period.c \
	$(LIBXMP_LITE)/lite-player.c \
	$(LIBXMP_LITE)/lite-read_event.c \
	$(LIBXMP_LITE)/lite-misc.c \
	$(LIBXMP_LITE)/lite-dataio.c \
	$(LIBXMP_LITE)/lite-lfo.c \
	$(LIBXMP_LITE)/lite-scan.c \
	$(LIBXMP_LITE)/lite-control.c \
	$(LIBXMP_LITE)/lite-filter.c \
	$(LIBXMP_LITE)/lite-effects.c \
	$(LIBXMP_LITE)/lite-mixer.c \
	$(LIBXMP_LITE)/lite-mix_all.c \
	$(LIBXMP_LITE)/lite-load_helpers.c \
	$(LIBXMP_LITE)/lite-load.c \
	$(LIBXMP_LITE)/lite-filetype.c \
	$(LIBXMP_LITE)/lite-hio.c \
	$(LIBXMP_LITE)/lite-smix.c \
	$(LIBXMP_LITE)/lite-memio.c \
	$(LIBXMP_LITE)/lite-rng.c \
	$(LIBXMP_LITE)/lite-win32.c \
	$(LIBXMP_LITE)/lite-flow.c \
	$(LIBXMP_LITE)/lite-common.c \
	$(LIBXMP_LITE)/lite-itsex.c \
	$(LIBXMP_LITE)/lite-sample.c \
	$(LIBXMP_LITE)/lite-xm_load.c \
	$(LIBXMP_LITE)/lite-mod_load.c \
	$(LIBXMP_LITE)/lite-s3m_load.c \
	$(LIBXMP_LITE)/lite-it_load.c

LIBXMP_LITE_OBJS = $(LIBXMP_LITE_SRCS:.c=.o)

# Include paths:
#   libxmp/include       -> xmp.h (public API)
#   libxmp/src           -> internal headers (common.h, effects.h, etc.)
LIBXMP_INC = -I$(LIBXMP_DIR)/include -I$(LIBXMP_DIR)/src

# ── Pull in SDK build system ────────────────────────────────────
include $(SDK_DIR)/sdk.mk

# ── Append libxmp-lite flags AFTER sdk.mk ────────────────────────
# BUILDING_STATIC : makes LIBXMP_EXPORT empty (no dllexport)
# NDEBUG          : disable assert()
# We do NOT need -DLIBXMP_CORE_PLAYER here because each lite-*.c
# already defines it internally before including the real source.
ALL_CFLAGS += $(LIBXMP_INC) \
              -DBUILDING_STATIC -DNDEBUG \
              -include $(CURDIR)/libxmp_compat.h \
              -Wno-unused-parameter -Wno-sign-compare

# ── Add libxmp-lite objects to the link ──────────────────────────
OBJS += $(LIBXMP_LITE_OBJS)

# ── Dependency: app.elf needs libxmp objects ─────────────────────
app.elf: $(LIBXMP_LITE_OBJS)

# ── Compile rule for lite wrapper sources ────────────────────────
# Note: these files #include "../foo.c" so the -I path to src/ is
# critical for finding the internal headers.
$(LIBXMP_LITE)/%.o: $(LIBXMP_LITE)/%.c
	@echo "  CC  $<"
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

# ── CRT start object ────────────────────────────────────────────
$(CRT_DIR)/start.o: $(CRT_DIR)/start.S
	$(AS) $(ASFLAGS) -c -o $@ $<

# ── Targets ──────────────────────────────────────────────────────
all: app.elf
	$(SIZE) app.elf

clean: sdk-clean
	rm -f $(LIBXMP_LITE_OBJS)

.PHONY: all clean