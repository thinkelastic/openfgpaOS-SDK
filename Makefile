# openfpgaOS SDK Makefile
#
# Usage:
#   make                  Build mi_app, create release/
#   make deploy           Copy release/ to Pocket SD card
#   make clean            Remove all build artifacts
#   make core             Build a standalone game core (interactive)
#   make package          Package full SDK release into a ZIP
#   make package-mi_app   Package only mi_app into a ZIP

# ── Paths ────────────────────────────────────────────────────────
CORE_ID      = ThinkElastic.openfpgaOS
PLATFORM     = openfpgaos
RELEASE      = build/sdk
REL_CORE     = $(RELEASE)/Cores/$(CORE_ID)
REL_ASSETS   = $(RELEASE)/Assets/$(PLATFORM)/common
REL_INSTANCE = $(RELEASE)/Assets/$(PLATFORM)/$(CORE_ID)
REL_PLATFORM = $(RELEASE)/Platforms
RUNTIME      = runtime

# ── Default target ───────────────────────────────────────────────
all: mi_app release

# ── Build mi_app ─────────────────────────────────────────────────
mi_app:
	$(MAKE) -C src/mi_app

# ── Create release/ directory ────────────────────────────────────
release: mi_app
	@echo "Creating release/..."
	@mkdir -p $(REL_CORE) $(REL_ASSETS) $(REL_INSTANCE) $(REL_PLATFORM)/_images
	@# Core: bitstream + loader
	@cp $(RUNTIME)/bitstream.rbf_r $(REL_CORE)/
	@cp $(RUNTIME)/loader.bin $(REL_CORE)/
	@# Core: JSON configs + icon (from dist/sdk/ if present)
	@[ -d dist/sdk/core ] && cp dist/sdk/core/*.json dist/sdk/core/*.bin $(REL_CORE)/ 2>/dev/null || true
	@# Platform
	@[ -d dist/sdk/platform ] && cp dist/sdk/platform/*.json $(REL_PLATFORM)/ 2>/dev/null || true
	@[ -d dist/sdk/platform/_images ] && cp dist/sdk/platform/_images/*.bin $(REL_PLATFORM)/_images/ 2>/dev/null || true
	@# OS binary
	@cp $(RUNTIME)/os.bin $(REL_ASSETS)/
	@# mi_app ELF + data files
	@[ -f src/mi_app/app.elf ] && cp src/mi_app/app.elf $(REL_ASSETS)/mi_app.elf || true
	@find src/mi_app -maxdepth 1 \( -name "*.mid" -o -name "*.wav" -o -name "*.dat" -o -name "*.png" \) \
		-exec cp {} "$(REL_ASSETS)/" \; 2>/dev/null || true
	@# Instance JSON — only mi_app
	@[ -f dist/sdk/instances/mi_app.json ] && cp dist/sdk/instances/mi_app.json $(REL_INSTANCE)/ || true
	@echo "Release ready: $(RELEASE)/"

# ── Deploy to SD card ────────────────────────────────────────────
deploy: release
	@./deploy.sh

# ── Clean ────────────────────────────────────────────────────────
clean:
	$(MAKE) -C src/mi_app clean
	rm -rf build releases

# ── Core packaging ───────────────────────────────────────────────
core:
	./customize.sh

package:
	./package.sh

package-mi_app:
	./package_mi_app.sh

.PHONY: all mi_app release deploy clean core package package-mi_app