# openfpgaOS SDK

Build games for the [Analogue Pocket](https://www.analogue.co/pocket) in C or C++.

**Hardware:** VexiiRiscv rv32imafc @ 100 MHz, 8 KB I-cache + 32 KB D-cache, 64 MB SDRAM, 320x240 video, 48 kHz stereo audio, 18-channel OPL3 FM synthesis + MIDI playback library.

## Getting Started

### 1. Fork and clone

```bash
git clone https://github.com/YOUR_USERNAME/openfpgaOS-SDK.git
cd openfpgaOS-SDK
```

### 2. Check your toolchain

```bash
./scripts/setup.sh
```

You need a RISC-V GCC:
- **Arch:** `pacman -S riscv64-elf-gcc`
- **macOS:** `brew install riscv64-elf-gcc`
- **Ubuntu:** `apt install gcc-riscv64-unknown-elf`

### 3. Create your game

```bash
./scripts/customize.sh
```

Follow the prompts. This creates:
- `src/<gamename>/main.c` — a hello world stub to start from
- `dist/<GameName>/` — the core packaging config

### 4. Build and deploy

```bash
make               # builds all apps → build/sdk/
make deploy         # copies to Pocket SD card
```

### 5. Build PHDP tools (optional)

For UART-based development with a DevKey cartridge:

```bash
cd src/tools/phdp
make
sudo make install   # installs phdpd + phdp to /usr/local/bin
```

---

## Writing Your Game

Edit `src/<gamename>/main.c`:

```c
#include "of.h"
#include <stdio.h>

int main(void) {
    of_video_init();

    /* Set up a palette */
    for (int i = 0; i < 256; i++)
        of_video_palette(i, (i << 16) | ((255 - i) << 8) | 128);

    /* Draw to the framebuffer */
    uint8_t *fb = of_video_surface();
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 320; x++)
            fb[y * 320 + x] = x ^ y;

    of_video_flip();
    printf("Hello from openfpgaOS!\n");

    /* Game loop */
    while (1) {
        of_input_poll();
        if (of_btn_pressed(OF_BTN_A)) {
            /* handle button press */
        }
        of_delay_ms(16);  /* ~60 fps */
    }
}
```

### Standard C library

The OS kernel provides a full C standard library via a jump table. No musl or newlib needed in your build:

```c
#include <stdio.h>    // printf, snprintf, sscanf,
                      // fopen, fclose, fread, fwrite, fseek, ftell
#include <stdlib.h>   // malloc, free, calloc, realloc, atoi, atof,
                      // strtol, strtod, qsort, bsearch, rand, abs
#include <string.h>   // memcpy, memset, strlen, strcmp, strdup,
                      // strcat, strtok, memchr, strspn, strcspn, ...
#include <math.h>     // sinf, cosf, sqrtf, powf, logf, atan2f, fabsf, ...
#include <ctype.h>    // toupper, tolower, isalpha, isdigit, isspace, ...
```

### C++ support

The SDK supports C++ (freestanding, no exceptions, no RTTI). Place `.cpp` files alongside `.c` files and they are compiled automatically.

What works:
- Classes, inheritance, virtual methods
- `operator new` / `delete` (backed by the OS `malloc`/`free`)
- Templates
- Static constructors and destructors (`.init_array` / `.fini_array`)
- All SDK headers are `extern "C"` compatible
- `<iostream>` — `std::cout`, `std::cerr`, `std::cin` (lightweight, jump-table backed)

What is **not** available (freestanding environment):
- Exceptions (`-fno-exceptions`)
- RTTI / `dynamic_cast` (`-fno-rtti`)
- The rest of the C++ Standard Library (`<vector>`, `<string>`, `<algorithm>`, etc.)

#### `<iostream>` — cout / cerr / cin

```cpp
#include <iostream>

int main(void) {
    std::cout << "Hello from cout!\n";
    std::cout << "int=" << 42 << " float=" << 3.14f << std::endl;
    std::cerr << "error message\n";

    int n;
    std::cin >> n;                         // reads from fd 0 (stdin / serial)
    std::cout << "you entered: " << n << "\n";
}
```

`std::cout` and `std::cerr` write through `write(1, …)` / `write(2, …)` via the OS jump table — identical to calling `printf`. `std::cin` reads from fd 0 character-by-character; on the Analogue Pocket there is no keyboard, so `cin` is mainly useful when stdin is connected to a serial port or redirected by the host OS.

Supported `operator<<` types: `bool`, `char`, `unsigned char`, `const char*`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `unsigned long`, `long long`, `unsigned long long`, `float`, `double`, `void*`.

Supported `operator>>` types: `char`, `char*` (one word), `int`, `unsigned int`, `long`, `unsigned long`, `float`, `double`, `bool`.

Example (`main.cpp`):

```cpp
#include "of.h"
#include <stdio.h>

class Game {
    int score;
public:
    Game() : score(0) {}
    void tick() {
        of_input_poll();
        if (of_btn_pressed(OF_BTN_A)) score++;
    }
    void draw() {
        of_video_clear(0);
        printf("Score: %d\n", score);
        of_video_flip();
    }
};

int main(void) {
    of_video_init();
    Game game;
    while (1) {
        game.tick();
        game.draw();
        of_delay_ms(16);
    }
}
```

You can add custom `CXXFLAGS` in your Makefile before including `sdk.mk`:

```makefile
CXXFLAGS = -std=c++17
```

### PC build

Test on your computer with SDL2:

```bash
make pc
./app_pc
```

---

## API Reference

Include `"of.h"` for the entire API, or include individual headers.

### Video — `of_video.h`

320x240 framebuffer with double buffering. Default mode is 8-bit indexed (256-color palette).

```c
of_video_init();                              // Initialize video
uint8_t *fb = of_video_surface();             // Get back buffer (write here)
of_video_flip();                              // Swap front/back buffers
of_video_sync();                              // Wait until flip completes
of_video_clear(0);                            // Fill back buffer with palette index
of_video_pixel(x, y, color);                  // Set one pixel (bounds-checked)
of_video_flush();                             // Flush D-cache (advanced)
```

**Palette:**

```c
of_video_palette(index, 0x00RRGGBB);          // Set one palette entry (0-255)
of_video_palette_bulk(rgb_array, count);       // Set multiple entries at once
of_video_palette_vga6(vga_triplets, count);    // Convert 6-bit VGA palette (0-63 per channel)
```

**Color modes:**

Six video modes, switched at runtime. Indexed modes use the palette; direct modes encode color per pixel.

```c
of_video_set_color_mode(OF_VIDEO_MODE_8BIT);      // 256 colors, 1 byte/pixel (default)
of_video_set_color_mode(OF_VIDEO_MODE_4BIT);      // 16 colors,  2 pixels/byte
of_video_set_color_mode(OF_VIDEO_MODE_2BIT);      // 4 colors,   4 pixels/byte
of_video_set_color_mode(OF_VIDEO_MODE_RGB565);    // 16-bit direct, 2 bytes/pixel
of_video_set_color_mode(OF_VIDEO_MODE_RGB555);    // 15-bit direct, 2 bytes/pixel
of_video_set_color_mode(OF_VIDEO_MODE_RGBA5551);  // 15-bit + alpha, 2 bytes/pixel

uint16_t *fb16 = of_video_surface16();   // Use for 16-bit modes
```

| Mode | Framebuffer size | Pixels per byte |
|------|-----------------|-----------------|
| 8-bit indexed | 76,800 B | 1 |
| 4-bit indexed | 38,400 B | 2 (low nibble first) |
| 2-bit indexed | 19,200 B | 4 (LSB first) |
| RGB565 | 153,600 B | 0.5 (16-bit per pixel) |
| RGB555 | 153,600 B | 0.5 |
| RGBA5551 | 153,600 B | 0.5 (bit 0 = alpha) |

**Display mode:**

```c
of_video_set_display_mode(0);    // Terminal only (text console)
of_video_set_display_mode(1);    // Framebuffer only (default after of_video_init)
of_video_set_display_mode(2);    // Overlay: white terminal text over framebuffer
```

**Blitting helpers:**

```c
of_blit(dx, dy, w, h, src, src_stride);               // Blit with transparency (pixel 0 = skip)
of_blit_pal(dx, dy, w, h, src, src_stride, offset);   // Blit with palette offset
of_fill_rect(x, y, w, h, color);                       // Solid filled rectangle
of_video_blit_letterbox(src, src_w, src_h);             // Center vertically, black bars
```

### Audio — `of_audio.h`

48 kHz stereo PCM output with hardware OPL3 (YMF262) FM synthesis.

```c
of_audio_init();                                      // Initialize audio system
of_audio_enqueue(samples, count);                     // Queue stereo int16_t pairs
int free = of_audio_ring_free();                      // Samples free in ring buffer
```

**OPL3 FM synthesis (18 channels, both register banks):**

```c
of_audio_opl_write(reg, val);     // Write OPL3 register (uint16_t reg)
of_audio_opl_reset();             // Reset all OPL3 state
```

Registers `0x00`-`0xFF` target bank 0 (channels 0-8), `0x100`-`0x1FF` target bank 1 (channels 9-17). Enable full OPL3 mode: `of_audio_opl_write(0x105, 0x01)`.

### MIDI Playback — `of_midi.h`

Plays Standard MIDI Files (Format 0 and 1) through 18 OPL3 channels. Non-blocking, timer-driven. Includes a built-in General MIDI instrument bank.

```c
of_midi_init();                              // Init OPL3 in 18-channel mode
of_midi_play(midi_data, midi_len, 1);        // Play (1 = loop)
of_midi_pump();                              // Call each frame (non-blocking)
of_midi_stop();                              // Stop and silence all
of_midi_pause();  / of_midi_resume();        // Pause/resume
of_midi_set_volume(200);                     // Master volume 0-255
of_midi_load_bank(custom_bank);              // Custom GM bank (NULL = built-in)
int playing = of_midi_playing();             // Query state
```

**Features:** Format 0 + Format 1 (multi-track), velocity-scaled volume, channel volume (CC7), pan (CC10), pitch bend (±2 semitones), tempo changes, looping, custom instrument banks. Error codes: `OF_MIDI_OK`, `OF_MIDI_ERR_BAD_HDR`, `OF_MIDI_ERR_FORMAT`, `OF_MIDI_ERR_NO_TRACKS`, `OF_MIDI_ERR_PLAYING`.

**Example (mididemo):**

```c
#include "of.h"

static uint8_t midi_buf[256 * 1024] __attribute__((aligned(512)));

int main(void) {
    of_file_slot_register(3, "music.mid");
    FILE *f = fopen("music.mid", "rb");
    uint32_t n = fread(midi_buf, 1, sizeof(midi_buf), f);
    fclose(f);

    of_midi_init();
    of_midi_play(midi_buf, n, 1);     // loop

    while (1) {
        of_midi_pump();               // process MIDI events
        of_delay_ms(1);
    }
}
```

### Audio Mixer — `of_mixer.h`

Multi-voice PCM mixer with automatic resampling to 48 kHz. Input: unsigned 8-bit PCM.

```c
of_mixer_init(8, 48000);                             // 8 voices, 48 kHz output
int voice = of_mixer_play(pcm_u8, len, rate, pri, vol);  // Play sample → voice ID
of_mixer_set_volume(voice, 200);                     // Volume: 0-255
of_mixer_stop(voice);                                // Stop one voice
of_mixer_stop_all();                                 // Stop all voices
of_mixer_pump();                                     // Call each frame to process audio
int active = of_mixer_voice_active(voice);           // 1 if playing, 0 if done
```

### Input — `of_input.h`

Two controllers with d-pad, face buttons, shoulders, triggers, and analog sticks.

```c
of_input_poll();                          // Read hardware (call once per frame)

/* Player 1 */
if (of_btn(OF_BTN_A))          { ... }   // Held this frame
if (of_btn_pressed(OF_BTN_A))  { ... }   // Just pressed (edge)
if (of_btn_released(OF_BTN_A)) { ... }   // Just released (edge)

/* Player 2 */
if (of_btn_p2(OF_BTN_START))          { ... }
if (of_btn_pressed_p2(OF_BTN_START))  { ... }
```

**Button constants:** `OF_BTN_UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `X`, `Y`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `SELECT`, `START`

**Full state (sticks, triggers):**

```c
of_input_state_t state;
of_input_state(0, &state);            // Player 0
int16_t lx = state.joy_lx;           // Left stick X: -32768..32767
int16_t ly = state.joy_ly;           // Left stick Y
uint16_t lt = state.trigger_l;       // Left trigger: 0..65535

of_input_set_deadzone(4000);          // Stick deadzone (default: 0)
```

### Timer — `of_timer.h`

100 MHz hardware timer. Starts at 0 on boot, wraps at ~71 minutes (us) / ~49 days (ms).

```c
uint32_t us = of_time_us();           // Microseconds since boot
uint32_t ms = of_time_ms();           // Milliseconds since boot
of_delay_us(100);                     // Busy-wait 100 microseconds
of_delay_ms(16);                      // Busy-wait 16 milliseconds
```

### File I/O — `of_file.h`

Apps register filenames at startup, then use standard C file I/O:

```c
/* Register data files (call once at startup) */
of_file_slot_register(3, "game.dat");

/* Then use standard fopen */
FILE *f = fopen("game.dat", "rb");    // Resolves to slot 3
fread(buf, 1, size, f);
fclose(f);

/* Or access slots directly without registration */
FILE *f = fopen("slot:3", "rb");
```

**Low-level (bypasses stdio):**

```c
int n = of_file_read(slot_id, offset, dest, length);   // DMA read from data slot
long sz = of_file_size(slot_id);                         // File size in bytes
```

**Idle hook** — called during DMA waits for background work (e.g., audio pump):

```c
of_set_idle_hook(my_audio_pump);      // Called by OS during bridge waits
of_set_idle_hook(NULL);               // Disable
```

### Save System — `of_save.h`

10 persistent save slots (256 KB each), backed by CRAM1 nonvolatile memory.

**Preferred: standard C file I/O (auto-flushes on close):**

```c
FILE *f = fopen("save_0", "wb");
fwrite(data, sizeof(data), 1, f);
fclose(f);                            // Auto-flushes actual bytes written

FILE *f = fopen("save_0", "rb");
fread(data, sizeof(data), 1, f);
fclose(f);
```

Save names: `"save_0"` through `"save_9"`, or `"save:0"` through `"save:9"`.

**Low-level API:**

```c
of_save_read(slot, buf, offset, len);           // Read from save slot (0-9)
of_save_write(slot, buf, offset, len);          // Write to save slot
of_save_flush(slot);                            // Flush full 256KB to SD
of_save_flush_size(slot, actual_bytes);         // Flush only what you wrote
of_save_erase(slot);                            // Zero and flush
```

### Terminal — `of_terminal.h`

40x30 text console with CP437 character set. Useful for debug output.

```c
of_print("Hello\n");                  // Print string
of_print_char('X');                   // Print one character
of_print_clear();                     // Clear screen
of_print_at(col, row);               // Move cursor (0-indexed)
printf("Score: %d\n", score);         // Standard printf works too
```

**Box drawing (CP437, ncurses-compatible names):**

```c
of_print_char(ACS_ULCORNER);  of_print_char(ACS_HLINE);  of_print_char(ACS_URCORNER);
of_print_char(ACS_VLINE);     of_print(" text ");         of_print_char(ACS_VLINE);
of_print_char(ACS_LLCORNER);  of_print_char(ACS_HLINE);  of_print_char(ACS_LRCORNER);
```

Available: single-line (`ACS_VLINE`, `ACS_HLINE`, corners, tees, `ACS_PLUS`), double-line (`ACS_D_*`), block elements (`ACS_BLOCK`, `ACS_CKBOARD`), arrows (`ACS_UARROW`, etc.), symbols (`ACS_BULLET`, `ACS_DEGREE`).

### Tile Engine — `of_tile.h`

Hardware tile layer (64x32 tilemap of 8x8 tiles, 4bpp) plus 64 hardware sprites (8x8, 4bpp).

```c
/* Tile layer */
of_tile_enable(1);                                    // Enable tile layer
of_tile_scroll(scroll_x, scroll_y);                   // Pixel-level scrolling
of_tile_set(col, row, tile_index);                    // Set one tile
of_tile_load_map(map_data, count);                    // Load tilemap
of_tile_load_chr(chr_data, size);                     // Load tile graphics

/* Sprites */
of_sprite_enable(1);                                  // Enable sprite layer
of_sprite_set(id, tile, palette, flip_h, flip_v);     // Configure sprite
of_sprite_move(id, x, y);                             // Position sprite
of_sprite_load_chr(chr_data, size);                   // Load sprite graphics
of_sprite_hide(id);                                   // Hide one sprite
of_sprite_hide_all();                                 // Hide all sprites
```

### Link Cable — `of_link.h`

Inter-device communication for multiplayer:

```c
int ok = of_link_send(data_32bit);         // Send 32-bit word (0=success)
int ok = of_link_recv(&data_32bit);        // Receive 32-bit word (0=success)
uint32_t status = of_link_status();        // Connection status
```

### Interact — `of_interact.h`

Read Pocket menu options (defined in `interact.json`). Up to 64 variables.

```c
uint32_t val = of_interact_get(0);    // Read variable at index 0
```

Variable indices match `interact.json` order. The first 4 are reserved by the SDK (Analogizer, SNAC, video offsets). App-specific options start at index 4.

### Analogizer — `of_analogizer.h`

```c
int enabled = of_analogizer_enabled();    // 1 if Analogizer hardware present
uint32_t state = of_analogizer_state();   // SNAC type, video mode, offsets
```

### Audio Codec — `of_codec.h`

Parse VOC and WAV audio files into raw PCM:

```c
of_codec_result_t result;
of_codec_parse_wav(wav_data, wav_size, &result);
// result.pcm, result.pcm_len, result.sample_rate, result.bits_per_sample, result.channels
```

### LZW Compression — `of_lzw.h`

Build Engine compatible LZW compression:

```c
int32_t compressed_size = of_lzw_compress(in, in_len, out);
int32_t decompressed_size = of_lzw_uncompress(in, comp_len, out);
```

### Cache — `of_cache.h`

For advanced users. Most apps never need this.

```c
of_cache_flush_video();           // Flush D-cache for framebuffer
of_cache_invalidate_icache();     // Invalidate I-cache (after code loading)
```

### BRAM Hot Path — `of_bram.h`

Place performance-critical functions in on-chip BRAM for zero-wait-state execution (~55 KB available). Normal code runs from SDRAM with cache; BRAM code has guaranteed zero-cycle latency.

```c
#include "of.h"

OF_FASTTEXT void inner_loop(void) {
    /* Runs from BRAM — no cache misses */
}

OF_FASTDATA int lookup_table[256];       // Initialized data in BRAM
OF_FASTRODATA const int constants[16];   // Read-only data in BRAM

int main(void) {
    inner_loop();    // Direct call to BRAM address
}
```

The linker places `OF_FASTTEXT` code in BRAM (VMA 0x2000-0xFE00) with load data in SDRAM. The OS copies it to BRAM at app startup. No runtime API needed — just annotate functions.

### Version — `of_version.h`

```c
uint32_t v = of_get_version();     // Runtime API version from kernel
// OF_API_VERSION_MAJOR, OF_API_VERSION_MINOR, OF_API_VERSION_PATCH
```

---

## UART Development (PHDP)

The **Pocket-Host Debug Protocol** streams binaries over UART at 2 Mbaud, bypassing the SD card for rapid iteration. Requires a DevKey cartridge connected via USB-UART adapter.

### Architecture

Two host-side tools in `src/tools/phdp/`:

- **`phdpd`** — background daemon that owns the UART connection and manages protocol state
- **`phdp`** — CLI client that talks to the daemon via Unix socket

### Workflow

```bash
# Start the daemon (once)
phdpd                               # auto-detects /dev/ttyUSB0
phdpd -d /dev/ttyACM0               # or specify device

# Queue files for the next boot
phdp push --slot 1 build/Assets/openfpgaos/common/os.bin
phdp push --slot 2 build/Assets/openfpgaos/common/myapp.elf

# Reboot the core and stream
phdp reset
phdp wait                           # blocks until OS is running
phdp logs                           # tail console output
```

### Protocol phases

1. **Discovery** (250ms) — Pocket broadcasts `EVT_BOOT_ALIVE` over UART. If no host responds, boots from SD.
2. **Override** (200ms per slot) — before loading each data slot, Pocket asks the host. Host responds with `RES_STREAM` (send over UART) or `RES_USE_SD` (load from SD).
3. **Streaming** — host sends `DATA_CHUNK` packets (up to 512B), Pocket ACKs with `REPORT_PROGRESS`. CRC-16/CCITT on every packet.
4. **Monitoring** — after `EVT_EXEC_START`, terminal output is mirrored to UART as raw ASCII.

### CLI commands

| Command | Description |
|---------|-------------|
| `phdp status` | Connection state, queued slots, transfer progress |
| `phdp push --slot N file` | Queue binary for slot N |
| `phdp clear [--slot N]` | Clear queued overrides |
| `phdp reset` | Reboot the RISC-V core |
| `phdp wait` | Block until OS is running |
| `phdp logs [--last N]` | Tail or show last N lines of console output |

### Typical dev loop

```bash
make firmware                        # rebuild OS
./scripts/exec.sh build/Assets/openfpgaos/common/os.bin
```

`exec.sh` starts the daemon if needed, clears pending slots, pushes the file (auto-detects slot 1 for `os.bin`, slot 2 for app ELFs), resets the core, and streams console output until Ctrl+C.

---

## Data Slot Layout

The APF framework uses data slots to map SD card files to bridge memory regions. Your `data.json` defines the layout:

| Slot ID | Name | Purpose |
|---------|------|---------|
| 9 | Game | Instance selector (must be first in data.json) |
| 1 | OS Binary | `os.bin` — loaded by bootloader via DMA |
| 2 | Application | Your app ELF — loaded by OS kernel |
| 3-6 | Data 1-4 | App data files (WAD, GRP, images, audio, etc.) |
| 10-19 | Save 0-9 | Nonvolatile CRAM1 save slots (256 KB each) |

**Rules:**
- Slot 9 (Game selector) **must be the first entry** in `data.json`
- Slot 0 is reserved by APF — do not use
- Save slots use bridge address `0x30000000` (CRAM1) with 256 KB stride
- Each instance JSON maps which files go into which slots

### Instance JSON example

```json
{
    "instance": {
        "magic": "APF_VER_1",
        "variant_select": { "id": 666, "select": false },
        "data_slots": [
            { "id": 1, "filename": "os.bin" },
            { "id": 2, "filename": "mygame.elf" },
            { "id": 3, "filename": "assets.dat" }
        ]
    }
}
```

Place instance JSONs in `dist/<GameName>/instances/`. Files referenced by the instance go in `Assets/<platform>/common/` on the SD card.

---

## Memory Map

```
0x00000000 ┌──────────────────────┐
           │ BRAM (32 KB)         │
           │ 0x0000-0x1FFF: OS    │  Boot, trap handler
           │ 0x2000-0x7DFF: App   │  OF_FASTTEXT (~23 KB)
           │ 0x7E00-0x7FFF: Stack │  Trap frame
0x00008000 ├──────────────────────┤
           │                      │
0x10300000 ├──────────────────────┤
           │ OS Kernel (SDRAM)    │  ~128 KB
0x10400000 ├──────────────────────┤
           │ App Code + Data      │  Up to 48 MB
           │ (loaded from ELF)    │
0x13FFFFFF └──────────────────────┘

0x39000000   CRAM1: Save slots (10 x 256 KB)
```

---

## Scripts

| Script | What it does |
|--------|-------------|
| `scripts/setup.sh` | Checks RISC-V toolchain is installed |
| `scripts/customize.sh` | Creates a new game: stub source + core config |
| `scripts/deploy.sh` | Copies `build/sdk/` to the Pocket SD card |
| `scripts/package.sh` | ZIPs a game core for distribution |
| `scripts/exec.sh` | Push binary via UART, reset core, stream output |

**PHDP tools** (in `src/tools/phdp/`):

| Tool | What it does |
|------|-------------|
| `phdpd` | Background daemon — owns UART, manages PHDP protocol |
| `phdp` | CLI client — push binaries, reset, tail logs |

### Packaging a standalone game

After `customize.sh`, your game can be packaged as its own Pocket menu entry:

```bash
./scripts/package.sh GameName      # creates releases/GameName.zip
```

Users extract the ZIP to their SD card root.

---

## Creating Your Own Game Repo

When your game is ready for its own repository:

### 1. Create a new repo from the SDK

```bash
git clone https://github.com/ThinkElastic/openfpgaOS-SDK.git MyGame
cd MyGame
./scripts/customize.sh    # creates game config + renames origin to sdk-upstream
git remote add origin git@github.com:YOU/MyGame.git
```

`customize.sh` automatically renames `origin` to `sdk-upstream` so you can track SDK updates while using `origin` for your own repo.

### 2. Develop your game

Edit `src/<gamename>/main.c`, add source files, build with `make`.

Your game source lives in `src/<gamename>/` — SDK files live in `src/sdk/`, `src/apps/`, `dist/sdk/`. They don't overlap.

### 3. Pull SDK updates

```bash
git fetch sdk-upstream
git merge sdk-upstream/main
make clean && make
```

Only SDK-owned files change. Your game files won't conflict.

### 4. Package and distribute

```bash
make                          # build everything
./scripts/package.sh                  # ZIPs SDK core + your standalone core
./scripts/package.sh <GameName>       # ZIP just your standalone core
```

---

## Porting Existing Games

For larger ports (Duke Nukem, Doom, etc.), games carry their own copy of the SDK headers and link against the OS jump table:

1. Copy `src/sdk/include/` and `src/sdk/libc/` into your game repo as `sdk/`
2. Copy `src/sdk/crt/start.S` and `src/sdk/crt/app.ld`
3. Write a `posix_shim.c` with game-specific stubs
4. Use `sdk/of_posix.c` for POSIX I/O (`open`/`read`/`write`/`lseek`)
5. Register data files: `of_file_slot_register(3, "game.grp")`

**Important for POSIX I/O:** The kernel uses riscv32 `_llseek` (5-argument convention). Your `lseek()` wrapper must pass `(fd, off_hi, off_lo, &result, whence)` via syscall 62, not the traditional 3-argument form. See `src/sdk/include/of_posix.c` for the reference implementation.

---

## Project Structure

```
openfpgaOS-SDK/
├── Makefile              <- YOUR build config
├── src/
│   ├── <gamename>/       <- YOUR game source (created by customize.sh)
│   ├── apps/             <- Bundled example apps (SDK-owned)
│   │   ├── bramdemo/     <- BRAM hot-path benchmarking
│   │   ├── celeste/      <- Full game example
│   │   ├── colordemo/    <- Video color mode demo (all 6 modes)
│   │   ├── cray/         <- Real-time C raytracer
│   │   ├── cxxdemo/      <- C++ classes, templates, iostream
│   │   ├── fbdemo/       <- PNG framebuffer display
│   │   ├── memdemo/     <- memset/memcpy throughput benchmark
│   │   ├── interactdemo/ <- Pocket menu variables
│   │   ├── mididemo/     <- MIDI playback (of_midi library, 18-ch OPL3)
│   │   ├── savea/        <- Save slot integrity test
│   │   ├── saveb/        <- Save cross-pollution test
│   │   ├── slotdemo/     <- File slot registry display
│   │   ├── testdemo/     <- Kernel test suite (182 assertions)
│   │   └── wavdemo/      <- WAV audio playback
│   └── sdk/              <- Headers, libc, CRT, build rules (SDK-owned)
│       ├── include/      <- openfpgaOS API headers
│       ├── libc/         <- C standard library wrappers
│       ├── crt/          <- Startup code + linker script
│       └── pc/           <- SDL2 shim for desktop builds
├── dist/
│   ├── sdk/              <- Shared openfpgaOS core configs (SDK-owned)
│   └── <GameName>/       <- YOUR standalone core (from customize.sh)
├── scripts/              <- Build/deploy/packaging scripts (SDK-owned)
├── runtime/              <- FPGA bitstream, OS binary, loader (SDK-owned)
└── build/                <- Build output (gitignored)
```

### What you change vs. what the SDK owns

| Yours (edit freely) | SDK-owned (updated via merge) |
|---------------------|-------------------------------|
| `Makefile` | `src/sdk/` |
| `src/<gamename>/` | `src/apps/` |
| `dist/<GameName>/` | `dist/sdk/` |
| `.gitignore` | `runtime/` |
| | `scripts/` |

---

## Reference

This SDK builds apps for [openfpgaOS](https://github.com/ThinkElastic/openfpgaOS) — a RISC-V operating system running on the Analogue Pocket's Cyclone V FPGA. See the openfpgaOS repo for architecture details, FPGA design, and OS internals.
