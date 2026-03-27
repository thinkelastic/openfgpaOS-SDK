# openfpgaOS SDK Makefile
#
# Usage:
#   make                       Build app (default: mi_app), create release/
#   make APP=myapp             Build a different app from src/myapp/
#   make deploy                Copy release/ to Pocket SD card
#   make clean                 Remove all build artifacts
#   make core                  Build a standalone game core (interactive)
#   make package               Package full SDK release into a ZIP
#   make package-app           Package only the current APP into a ZIP
#   make package-app APP=myapp Package a specific app into a ZIP

# ── App name (override on command line: make APP=myapp) ──────────
APP ?= mi_app

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
all: app release

# ── Build app ────────────────────────────────────────────────────
app:
	$(MAKE) -C src/$(APP)

# ── Create release/ directory ────────────────────────────────────
release: app
	@echo "Creating release (APP=$(APP))..."
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
	@# App ELF + data files
	@[ -f src/$(APP)/app.elf ] && cp src/$(APP)/app.elf $(REL_ASSETS)/$(APP).elf || true
	@find src/$(APP) -maxdepth 1 \( -name "*.mid" -o -name "*.wav" -o -name "*.dat" -o -name "*.png" \) \
		-exec cp {} "$(REL_ASSETS)/" \; 2>/dev/null || true
	@# Instance JSON — only the named app
	@[ -f dist/sdk/instances/$(APP).json ] && cp dist/sdk/instances/$(APP).json $(REL_INSTANCE)/ || true
	@echo "Release ready: $(RELEASE)/"

# ── Deploy to SD card ────────────────────────────────────────────
deploy: release
	@./deploy.sh

# ── Clean ────────────────────────────────────────────────────────
clean:
	$(MAKE) -C src/$(APP) clean
	rm -rf build releases

# ── Core packaging ───────────────────────────────────────────────
core:
	./customize.sh

package:
	./package.sh

package-app:
	./package_app.sh $(APP)

.PHONY: all app release deploy clean core package package-app