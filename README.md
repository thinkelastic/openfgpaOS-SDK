# openfpgaOS SDK

Build games for the [Analogue Pocket](https://www.analogue.co/pocket) in C.

## Getting Started

### 1. Fork and clone

```bash
git clone https://github.com/YOUR_USERNAME/openfpgaOS-SDK.git
cd openfpgaOS-SDK
```

### 2. Check your toolchain

```bash
./setup.sh
```

You need a RISC-V GCC:
- **Arch:** `pacman -S riscv64-elf-gcc`
- **macOS:** `brew install riscv64-elf-gcc`
- **Ubuntu:** `apt install gcc-riscv64-unknown-elf`

### 3. Create your game

```bash
./customize.sh
```

Follow the prompts. This creates:
- `src/<gamename>/main.c` — a hello world stub to start from
- `dist/<gamename>/` — the core packaging config

### 4. Build and deploy

```bash
make               # builds all apps → build/sdk/
make deploy         # copies to Pocket SD card
```

## Writing Your Game

Edit `src/<gamename>/main.c`:

```c
#include "of.h"
#include <stdio.h>

int main(void) {
    of_video_init();

    uint8_t *fb = of_video_surface();
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 320; x++)
            fb[y * 320 + x] = x ^ y;

    of_video_flip();
    printf("Hello world!\n");

    while (1) {
        of_input_poll();
        if (of_btn_pressed(OF_BTN_A)) {
            // handle input
        }
        of_delay_ms(16);
    }
}
```

### Standard C library

The SDK provides standard headers through the OS kernel's jump table:

```c
#include <stdio.h>    // printf, fprintf, sprintf, sscanf,
                      // fopen, fclose, fread, fwrite, fseek, ftell
#include <stdlib.h>   // malloc, free, calloc, realloc, atoi, atof,
                      // strtol, strtod, qsort, bsearch, rand
#include <string.h>   // memcpy, memset, strlen, strcmp, strdup,
                      // strcat, strtok, memchr, ...
#include <math.h>     // sinf, cosf, sqrtf, powf, logf, fabsf, ...
#include <ctype.h>    // toupper, tolower, isalpha, isdigit, isspace, ...
```

No musl or newlib needed — everything runs through the OS.

### API modules

Include `"of.h"` for the full API:

| Module | Description |
|--------|-------------|
| **Video** | 320×240 indexed framebuffer, 256-color palette, double buffering |
| **Audio** | 48 kHz stereo PCM, sample streaming, YM2151 FM synthesis |
| **Input** | D-pad, face buttons, shoulders, triggers, joystick (2 players) |
| **Timer** | Microsecond/millisecond timing, delays |
| **Save** | Up to 10 persistent save slots, up to 256KB each |
| **File** | Read data files from SD card (up to 4 data slots) |

### Save system

```c
/* Preferred: standard C file I/O */
FILE *f = fopen("save_0", "wb");
fwrite(data, sizeof(data), 1, f);
fclose(f);  /* auto-flushes to SD */

FILE *f = fopen("save_0", "rb");
fread(data, sizeof(data), 1, f);
fclose(f);
```

### Data Slot Layout

The APF framework uses data slots to map files to bridge memory. The `data.json` defines the slot layout:

| Slot ID | Name | Purpose |
|---------|------|---------|
| 9 | Game | Instance selector (must be first in data.json) |
| 1 | OS Binary | `os.bin` — loaded by bootloader via DMA |
| 2 | Application | App ELF — loaded by OS kernel |
| 3-6 | Data 1-4 | App data files (WAD, GRP, etc.) |
| 7 | OS Data | CRAM1 region for OS file table (FTAB) |
| 10-19 | Save 0-9 | Nonvolatile CRAM1 save slots (256KB each) |

**Important:**
- Slot 9 (Game selector) **must be the first entry** in `data.json` — the APF framework requires it
- Slot 0 is reserved by APF — do not use
- Slot 7 (OS Data) maps the CRAM1 region where Chip32 writes the filename-to-slot table (FTAB). This enables `fopen("filename.ext")` to resolve filenames to data slots automatically
- Save slot addresses use bridge addressing (`0x30000000` = CRAM1), with 256KB stride

### PC build

Test on your computer with SDL2:

```bash
make pc
./app_pc
```

## Scripts

| Script | What it does |
|--------|-------------|
| `setup.sh` | Checks RISC-V toolchain is installed |
| `customize.sh` | Creates a new game: stub source + core config |
| `deploy.sh` | Copies `build/sdk/` to the Pocket SD card |
| `package.sh` | ZIPs a game core for distribution |

### Packaging a standalone game

After `customize.sh`, your game can be packaged as its own Pocket menu entry:

```bash
./package.sh GameName      # creates releases/GameName.zip
```

Users extract the ZIP to their SD card root.

## Creating Your Own Game Repo

When your game is ready for its own repository:

### 1. Create a new repo from the SDK

```bash
git clone https://github.com/ThinkElastic/openfpgaOS-SDK.git MyGame
cd MyGame
./customize.sh    # creates game config + renames origin to sdk-upstream
git remote add origin git@github.com:YOU/MyGame.git
```

`customize.sh` automatically renames `origin` to `sdk-upstream` so you can track SDK updates while using `origin` for your own repo.

### 2. Develop your game

Edit `src/<gamename>/main.c`, add source files, build with `make`.

Your game source lives in `src/<gamename>/` — SDK files live in `src/sdk/`, `src/apps/`, `dist/sdk/`. They don't overlap.

### 3. Pull SDK updates

When the SDK is updated with new features or bug fixes:

```bash
git fetch sdk-upstream
git merge sdk-upstream/main
make clean && make
```

Only SDK-owned files change. Your game files (`Makefile`, `src/<gamename>/`, `dist/<GameName>/`) won't conflict.

### 4. Package and distribute

```bash
make                          # build everything
./package.sh                  # ZIPs SDK core + your standalone core
./package.sh <GameName>       # ZIP just your standalone core
```

### What you change vs. what the SDK owns

| Yours (edit freely) | SDK-owned (updated via merge) |
|---------------------|-------------------------------|
| `Makefile` | `src/sdk/` |
| `src/<gamename>/` | `src/apps/` |
| `dist/<GameName>/` | `dist/sdk/` |
| `.gitignore` | `runtime/` |
| | `customize.sh`, `deploy.sh`, `package.sh`, `setup.sh` |

## Project Structure

```
openfpgaOS-SDK/
├── Makefile              ← YOUR file: build config
├── src/
│   ├── <gamename>/       ← YOUR game source (created by customize.sh)
│   ├── apps/             ← bundled example apps (SDK-owned)
│   └── sdk/              ← headers, libc, CRT, build rules (SDK-owned)
│       ├── include/      ← openfpgaOS API (of.h, of_video.h, ...)
│       ├── libc/         ← C standard library wrappers
│       ├── crt/          ← startup code + linker script
│       └── pc/           ← SDL2 shim for desktop builds
├── dist/
│   ├── sdk/              ← shared openfpgaOS core configs (SDK-owned)
│   └── <GameName>/       ← YOUR standalone core (from customize.sh)
├── runtime/              ← FPGA bitstream, OS binary, loader (SDK-owned)
├── build/                ← build output (gitignored)
├── customize.sh          ← create new game core (SDK-owned)
├── deploy.sh             ← deploy to SD card (SDK-owned)
├── package.sh            ← create release ZIPs (SDK-owned)
└── setup.sh              ← toolchain check (SDK-owned)
```

## Reference

This SDK builds apps for [openfpgaOS](https://github.com/ThinkElastic/openfpgaOS) — a RISC-V operating system (VexRiscv rv32imafc, 100 MHz) running on the Analogue Pocket's Cyclone V FPGA. See the openfpgaOS repo for architecture details, FPGA design, and OS internals.
