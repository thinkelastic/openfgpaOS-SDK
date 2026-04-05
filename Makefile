# openfpgaOS SDK Makefile
#
# Quick start:
#   make setup        Install RISC-V toolchain
#   make core         Create your app
#
# Then from src/<app>/:
#   make              Build
#   make deploy       Deploy to Pocket SD card
#   make pc           Test on desktop (SDL2)

# ── Paths ────────────────────────────────────────────────────────────
CORE_ID      = ThinkElastic.openfpgaOS
PLATFORM     = openfpgaos
RUNTIME      = runtime

# ── Detect user apps (src/<name>/ excluding apps/, sdk/, tools/) ─────
USER_APPS := $(shell for d in src/*/; do \
	[ "$$d" = "src/apps/" ] || [ "$$d" = "src/sdk/" ] || [ "$$d" = "src/tools/" ] && continue; \
	[ -f "$$d/Makefile" ] && basename "$$d"; \
done)

# ── Default target ───────────────────────────────────────────────────
all: help

# ── Help ─────────────────────────────────────────────────────────────
help:
	@echo "         ___  ___  ___ ___"
	@echo "        / _ \\/ _ \\/ -_) _ \\"
	@echo "        \\___/ .__/\\__/_//_/"
	@echo "       ____/_/  ________"
	@echo "      / __/ _ \\/ ___/ _ |"
	@echo "     / _// ___/ (_ / __ |"
	@echo "    /_/_/_/___\\___/_/ |_|"
	@echo "   / __ \\/ __/"
	@echo "  / /_/ /\\ \\"
	@echo "  \\____/___/  SDK"
	@echo ""
	@echo "  Getting started:"
	@echo "    make setup            Install RISC-V toolchain"
	@echo "    make core             Create your app"
	@echo ""
	@echo "  Then work from your app directory:"
	@echo "    cd src/<app>"
	@echo "    make                  Build"
	@echo "    make exec             Build, push via UART, stream console"
	@echo "    make deploy           Deploy to Pocket SD card"
	@echo "    make package          Package core into a ZIP"
	@echo "    make pc               Test on desktop (SDL2)"
	@echo "    make clean            Remove build artifacts"
	@echo ""
	@echo "  To work with the demo apps:"
	@echo "    cd src/apps"
	@echo "    make                  Build all demos"
	@echo "    make new APP=demo     Create a new demo app"
	@echo "    make package          Package SDK core into a ZIP"
	@echo "    make deploy           Deploy SDK + demos to SD card"
	@echo "    make clean            Remove build artifacts"
	@echo ""
	@echo "  From the root:"
	@echo "    make build            Build everything"
	@echo "    make build APP=<app>  Build sdk or <app>"
	@echo "    make exec APP=<app>   Build, push via UART, stream console"
	@echo "    make deploy           Deploy everything to SD card"
	@echo "    make deploy APP=<app> Deploy sdk or <app> to SD card"
	@echo "    make tools            Build PHDP host tools"
	@echo "    make package          Package all cores into ZIPs"
	@echo "    make clean            Remove all build artifacts"

# ── Setup ────────────────────────────────────────────────────────────
setup:
	@./scripts/setup.sh

# ── Create your app ──────────────────────────────────────────────────
core:
	@./scripts/customize.sh

# ── Build ────────────────────────────────────────────────────────────
# make build            → build sdk demos + all user apps
# make build APP=sdk    → build sdk demos only
# make build APP=myapp  → build that user app only
build:
ifdef APP
ifeq ($(APP),sdk)
	$(MAKE) -C src/apps
else
	$(MAKE) -C src/$(APP)
endif
else
	$(MAKE) -C src/apps
	@for d in src/*/; do \
		[ "$$d" = "src/apps/" ] || [ "$$d" = "src/sdk/" ] || [ "$$d" = "src/tools/" ] && continue; \
		[ -f "$$d/Makefile" ] || continue; \
		name=$$(basename "$$d"); \
		echo "Building $$name..."; \
		$(MAKE) -C "$$d" || exit 1; \
	done
endif

# ── Exec (UART push + console) ───────────────────────────────────────
# make exec APP=myapp  → build, push via UART, stream console
exec:
	@test -n "$(APP)" || { echo "Usage: make exec APP=<app>"; exit 1; }
	$(MAKE) -C src/$(APP) exec

# ── Deploy ───────────────────────────────────────────────────────────
# make deploy            → deploy sdk + all user apps to SD card
# make deploy APP=sdk    → deploy sdk demos only
# make deploy APP=myapp  → deploy that user app only
deploy:
ifdef APP
ifeq ($(APP),sdk)
	$(MAKE) -C src/apps deploy
else
	$(MAKE) -C src/$(APP) deploy
endif
else
	$(MAKE) -C src/apps deploy
	@for d in src/*/; do \
		[ "$$d" = "src/apps/" ] || [ "$$d" = "src/sdk/" ] || [ "$$d" = "src/tools/" ] && continue; \
		[ -f "$$d/Makefile" ] || continue; \
		$(MAKE) -C "$$d" deploy || true; \
	done
endif

# ── Package ──────────────────────────────────────────────────────────
package:
ifdef APP
ifeq ($(APP),sdk)
	$(MAKE) -C src/apps package
else
	$(MAKE) -C src/$(APP) package
endif
else
	./scripts/package.sh
endif

# ── Build host tools ─────────────────────────────────────────────────
tools:
	$(MAKE) -C src/tools/phdp

# ── Clean ────────────────────────────────────────────────────────────
clean:
ifdef APP
ifeq ($(APP),sdk)
	$(MAKE) -C src/apps clean
else
	$(MAKE) -C src/$(APP) clean
endif
else
	$(MAKE) -C src/apps clean
	$(MAKE) -C src/tools/phdp clean
	@for d in src/*/; do \
		[ "$$d" = "src/apps/" ] || [ "$$d" = "src/sdk/" ] || [ "$$d" = "src/tools/" ] && continue; \
		[ -f "$$d/Makefile" ] && $(MAKE) -C "$$d" clean; \
	done
	rm -rf build .obj releases
endif

.PHONY: all help setup core build exec deploy package tools clean
