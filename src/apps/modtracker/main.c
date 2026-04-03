/*
 * Tracker player for openfpgaOS (Analogue Pocket)
 *
 * AUDIO-FIRST architecture for glitch-free playback on VexRiscv@100MHz:
 *
 *  1. Large pre-render buffer (2048 stereo pairs) fed into kernel ring
 *     buffer via of_audio_enqueue() — the kernel drains to HW FIFO
 *     automatically during DMA waits.
 *
 *  2. pump() called aggressively: every loop iteration + idle hook
 *     during file I/O. Audio never starves.
 *
 *  3. 8-bit indexed color (76KB framebuffer vs 153KB in RGB565).
 *     Fills use memset (1 byte/pixel), blits are trivial.
 *
 *  4. No full-screen clear — only dirty regions redrawn per section.
 *     Each ui_*() clears its own area then draws.
 *
 *  5. UI at ~10fps. Audio pump runs at full loop speed (~200Hz+).
 *
 * Controls:
 *   A             : play / pause
 *   B             : stop
 *   L1 / R1       : volume -/+
 *   LEFT / RIGHT  : prev / next position
 *   X             : toggle scope / channel view
 *   START         : exit
 */

#include "of.h"
#include "of_modplay.h"
#include <string.h>
#include <stdio.h>

/* ==============
 * Cover image
 * ============== */
#include "image_data.h"

/* ══════════════════════════════════════════════════════════════════
 * Palette indices — fixed 32-color UI palette
 * ══════════════════════════════════════════════════════════════════ */
enum {
    PAL_BG = 0,       /* black */
    PAL_PANEL,        /* dark gray */
    PAL_PANEL_LT,     /* medium gray */
    PAL_BORDER,       /* border gray */
    PAL_TEXT,         /* light gray */
    PAL_TEXT_HI,      /* white */
    PAL_TITLE,        /* cyan */
    PAL_ACCENT,       /* orange */
    PAL_GREEN,        /* bright green */
    PAL_RED,          /* red */
    PAL_YELLOW,       /* yellow */
    PAL_PROGRESS,     /* teal */
    PAL_VU_LO,       /* dark green */
    PAL_VU_MID,      /* green */
    PAL_VU_HI,       /* yellow-green */
    PAL_VU_PEAK,     /* red */
    PAL_SCOPE,       /* cyan scope line */
    PAL_SCOPE_DIM,   /* dim scope center line */
    /* Channel colors 0-7 */
    PAL_CH0, PAL_CH1, PAL_CH2, PAL_CH3,
    PAL_CH4, PAL_CH5, PAL_CH6, PAL_CH7,
    PAL_DUMMY1,
    PAL_COUNT
};

static void setup_palette(void) {
    static const uint32_t pal[] = {
        [PAL_BG]       = 0x000000,
        [PAL_PANEL]    = 0x1A1A2E,
        [PAL_PANEL_LT] = 0x2A2A40,
        [PAL_BORDER]   = 0x4A4A5A,
        [PAL_TEXT]     = 0xAAAAAA,
        [PAL_TEXT_HI]  = 0xFFFFFF,
        [PAL_TITLE]    = 0x00DDFF,
        [PAL_ACCENT]   = 0xFF8800,
        [PAL_GREEN]    = 0x00DD00,
        [PAL_RED]      = 0xDD0000,
        [PAL_YELLOW]   = 0xDDDD00,
        [PAL_PROGRESS] = 0x009999,
        [PAL_VU_LO]   = 0x004400,
        [PAL_VU_MID]  = 0x00AA00,
        [PAL_VU_HI]   = 0xAAAA00,
        [PAL_VU_PEAK] = 0xDD0000,
        [PAL_SCOPE]    = 0x00FFFF,
        [PAL_SCOPE_DIM]= 0x224444,
        [PAL_CH0]      = 0x00DDDD,
        [PAL_CH1]      = 0xDD00DD,
        [PAL_CH2]      = 0x00DD00,
        [PAL_CH3]      = 0xDDDD00,
        [PAL_CH4]      = 0xDD8800,
        [PAL_CH5]      = 0x4488DD,
        [PAL_CH6]      = 0xDD4444,
        [PAL_CH7]      = 0xAAAACC,
        [PAL_DUMMY1]   = 0X000000,
    };
    of_video_palette_bulk(pal, PAL_COUNT);
}

/* ══════════════════════════════════════════════════════════════════
 * Font 4x6 (ASCII 32-127, 3 bytes/glyph)
 * ══════════════════════════════════════════════════════════════════ */
static const uint8_t font4x6[] = {
    /* 32  ' ' */ 0x00,0x00,0x00, /* 33  '!' */ 0x44,0x40,0x40,
    /* 34  '"' */ 0xAA,0x00,0x00, /* 35  '#' */ 0xAF,0xAF,0xA0,
    /* 36  '$' */ 0x6D,0x47,0xB6, /* 37  '%' */ 0xA2,0x44,0xA0,
    /* 38  '&' */ 0x4A,0x4A,0xD0, /* 39  ''' */ 0x44,0x00,0x00,
    /* 40  '(' */ 0x24,0x44,0x20, /* 41  ')' */ 0x42,0x22,0x40,
    /* 42  '*' */ 0xA4,0xA0,0x00, /* 43  '+' */ 0x04,0xE4,0x00,
    /* 44  ',' */ 0x00,0x02,0x40, /* 45  '-' */ 0x00,0xE0,0x00,
    /* 46  '.' */ 0x00,0x00,0x40, /* 47  '/' */ 0x22,0x44,0x80,
    /* 48  '0' */ 0x4A,0xAA,0x40, /* 49  '1' */ 0x4C,0x44,0xE0,
    /* 50  '2' */ 0xC2,0x48,0xE0, /* 51  '3' */ 0xC2,0x42,0xC0,
    /* 52  '4' */ 0xAA,0xE2,0x20, /* 53  '5' */ 0xE8,0xC2,0xC0,
    /* 54  '6' */ 0x68,0xCA,0x40, /* 55  '7' */ 0xE2,0x24,0x40,
    /* 56  '8' */ 0x6A,0x4A,0x40, /* 57  '9' */ 0x4A,0x62,0xC0,
    /* 58  ':' */ 0x04,0x04,0x00, /* 59  ';' */ 0x04,0x02,0x40,
    /* 60  '<' */ 0x24,0x84,0x20, /* 61  '=' */ 0x0E,0x0E,0x00,
    /* 62  '>' */ 0x84,0x24,0x80, /* 63  '?' */ 0xC2,0x40,0x40,
    /* 64  '@' */ 0x4A,0xE8,0x60, /* 65  'A' */ 0x4A,0xEA,0xA0,
    /* 66  'B' */ 0xCA,0xCA,0xC0, /* 67  'C' */ 0x68,0x88,0x60,
    /* 68  'D' */ 0xCA,0xAA,0xC0, /* 69  'E' */ 0xE8,0xC8,0xE0,
    /* 70  'F' */ 0xE8,0xC8,0x80, /* 71  'G' */ 0x68,0xAA,0x60,
    /* 72  'H' */ 0xAA,0xEA,0xA0, /* 73  'I' */ 0xE4,0x44,0xE0,
    /* 74  'J' */ 0x22,0x2A,0x40, /* 75  'K' */ 0xAA,0xCA,0xA0,
    /* 76  'L' */ 0x88,0x88,0xE0, /* 77  'M' */ 0xAE,0xEA,0xA0,
    /* 78  'N' */ 0xAE,0xEE,0xA0, /* 79  'O' */ 0x4A,0xAA,0x40,
    /* 80  'P' */ 0xCA,0xC8,0x80, /* 81  'Q' */ 0x4A,0xAE,0x60,
    /* 82  'R' */ 0xCA,0xCA,0xA0, /* 83  'S' */ 0x68,0x42,0xC0,
    /* 84  'T' */ 0xE4,0x44,0x40, /* 85  'U' */ 0xAA,0xAA,0x60,
    /* 86  'V' */ 0xAA,0xAA,0x40, /* 87  'W' */ 0xAA,0xEE,0xA0,
    /* 88  'X' */ 0xAA,0x4A,0xA0, /* 89  'Y' */ 0xAA,0x44,0x40,
    /* 90  'Z' */ 0xE2,0x48,0xE0, /* 91  '[' */ 0x64,0x44,0x60,
    /* 92  '\' */ 0x88,0x44,0x20, /* 93  ']' */ 0x62,0x22,0x60,
    /* 94  '^' */ 0x4A,0x00,0x00, /* 95  '_' */ 0x00,0x00,0xE0,
    /* --- Lowercase + extras (96-127) --- */
    /* 96  '`' */ 0x42,0x00,0x00, /* 97  'a' */ 0x00,0x6A,0xE0,
    /* 98  'b' */ 0x88,0xCA,0xC0, /* 99  'c' */ 0x00,0x68,0x60,
    /* 100 'd' */ 0x22,0x6A,0x60, /* 101 'e' */ 0x04,0xAE,0x60,
    /* 102 'f' */ 0x24,0xE4,0x40, /* 103 'g' */ 0x00,0x6A,0x62,
    /* 104 'h' */ 0x88,0xCA,0xA0, /* 105 'i' */ 0x40,0x44,0x40,
    /* 106 'j' */ 0x20,0x22,0xA4, /* 107 'k' */ 0x88,0xAC,0xA0,
    /* 108 'l' */ 0xC4,0x44,0x60, /* 109 'm' */ 0x00,0xEE,0xA0,
    /* 110 'n' */ 0x00,0xCA,0xA0, /* 111 'o' */ 0x00,0x4A,0x40,
    /* 112 'p' */ 0x00,0xCA,0xC8, /* 113 'q' */ 0x00,0x6A,0x62,
    /* 114 'r' */ 0x00,0x68,0x80, /* 115 's' */ 0x00,0x64,0xC0,
    /* 116 't' */ 0x4E,0x44,0x20, /* 117 'u' */ 0x00,0xAA,0x60,
    /* 118 'v' */ 0x00,0xAA,0x40, /* 119 'w' */ 0x00,0xAE,0xE0,
    /* 120 'x' */ 0x00,0xA4,0xA0, /* 121 'y' */ 0x00,0xAA,0x62,
    /* 122 'z' */ 0x00,0xE4,0xE0, /* 123 '{' */ 0x24,0x84,0x20,
    /* 124 '|' */ 0x44,0x44,0x40, /* 125 '}' */ 0x84,0x24,0x80,
    /* 126 '~' */ 0x05,0xA0,0x00, /* 127 DEL */ 0x00,0x00,0x00,
};

/* ══════════════════════════════════════════════════════════════════
 * Drawing — all 8bpp, using SDK helpers where possible
 * ══════════════════════════════════════════════════════════════════ */

static uint8_t *fb;

/* Direct pixel — no bounds check for inner loops (caller clips) */
#define PX(x,y,c) fb[(y)*320+(x)] = (c)

/* Safe pixel with bounds check */
OF_FASTTEXT static inline void px(int x, int y, uint8_t c) {
    if ((unsigned)x < 320 && (unsigned)y < 240) PX(x, y, c);
}

/* Horizontal line via memset — the fastest fill on this CPU */
OF_FASTTEXT static void hline(int x, int y, int w, uint8_t c) {
    if (y < 0 || y >= 240) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > 320) w = 320 - x;
    if (w > 0) memset(fb + y * 320 + x, c, (size_t)w);
}

/* Fill rect via memset per row — way faster than pixel loops */
OF_FASTTEXT static void rect(int x, int y, int w, int h, uint8_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 320) w = 320 - x;
    if (y + h > 240) h = 240 - y;
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; r++)
        memset(fb + (y + r) * 320 + x, c, (size_t)w);
}

OF_FASTTEXT static void border(int x, int y, int w, int h, uint8_t c) {
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    for (int r = 1; r < h - 1; r++) {
        px(x, y + r, c);
        px(x + w - 1, y + r, c);
    }
}

OF_FASTTEXT static void glyph(int gx, int gy, char ch, uint8_t c) {
    int idx = (unsigned char)ch - 32;
    if (idx < 0 || idx > 95) return;   /* <-- era 63, ahora 95 */
    const uint8_t *g = &font4x6[idx * 3];
    uint8_t rows[6] = {
        (g[0]>>4)&0xF, g[0]&0xF, (g[1]>>4)&0xF,
        g[1]&0xF, (g[2]>>4)&0xF, g[2]&0xF
    };
    for (int r = 0; r < 6; r++)
        for (int col = 0; col < 4; col++)
            if (rows[r] & (8 >> col))
                px(gx + col, gy + r, c);
}

OF_FASTTEXT static int text(int x, int y, const char *s, uint8_t c) {
    int ox = x;
    while (*s) { glyph(x, y, *s++, c); x += 5; }
    return x - ox;
}

OF_FASTTEXT static void text_r(int rx, int y, const char *s, uint8_t c) {
    int w = (int)strlen(s) * 5 - 1;
    text(rx - w, y, s, c);
}

OF_FASTTEXT static void fmt_time(char *b, int ms) {
    int s = ms / 1000;
    snprintf(b, 16, "%d:%02d", s / 60, s % 60);
}

/* ══════════════════════════════════════════════════════════════════
 * Audio — uses of_audio_free/write (proven working), pumps hard
 * ══════════════════════════════════════════════════════════════════ */

/* Render buffer — 1024 stereo pairs (~21ms @ 48kHz).
 * Large enough to reduce call overhead, small enough to fit stack. */
#define PUMP_MAX 1024
OF_FASTDATA static int16_t audio_buf[PUMP_MAX * 2];

/* Scope capture buffer */
OF_FASTDATA static int16_t scope[320];

OF_FASTTEXT static void capture_scope(const int16_t *buf, int pairs) {
    if (pairs <= 0) return;
    for (int x = 0; x < 320; x++) {
        int i = x * pairs / 320;
        if (i >= pairs) i = pairs - 1;
        scope[x] = (int16_t)(((int)buf[i*2] + buf[i*2+1]) >> 1);
    }
}

/* The main audio pump — fill the HW FIFO as much as possible.
 * Uses of_audio_free() + of_audio_write() which are known working. */
OF_FASTTEXT static void pump(void) {
    if (!__mod_playing || __mod_paused) return;

    /* Try to fill the FIFO in multiple passes */
    for (int tries = 0; tries < 3; tries++) {
        int fifo_free = of_audio_free();
        if (fifo_free < 64) break;  /* FIFO full enough */

        int chunk = fifo_free;
        if (chunk > PUMP_MAX) chunk = PUMP_MAX;

        int ret = xmp_play_buffer(__mod_ctx, audio_buf, chunk * 4, 0);
        if (ret < 0) { of_mod_stop(); return; }

        capture_scope(audio_buf, chunk);
        of_audio_write(audio_buf, chunk);
    }
}

/* Idle hook — kernel calls this during DMA/bridge waits */
OF_FASTTEXT static void idle_hook(void) {
    pump();
}

/* ══════════════════════════════════════════════════════════════════
 * Layout
 * ══════════════════════════════════════════════════════════════════ */
#define M      4
#define HDR_Y  0
#define HDR_H  22
#define INF_Y  (HDR_H + 1)
#define INF_H  20
#define MID_Y  (INF_Y + INF_H + 1)
#define BOT_H  18
#define BOT_Y  (240 - BOT_H)
#define MID_H  (BOT_Y - MID_Y - 1)

/* ══════════════════════════════════════════════════════════════════
 * State
 * ══════════════════════════════════════════════════════════════════ */
static int view_mode;
static int dis_vu;
static uint8_t ch_vol[XMP_MAX_CHANNELS];
static uint8_t ch_pk[XMP_MAX_CHANNELS];
static uint8_t decay_cnt;

/* ══════════════════════════════════════════════════════════════════
 * UI sections — each clears its own area, no full-screen wipe
 * ══════════════════════════════════════════════════════════════════ */

static void ui_header(void) {
    rect(0, HDR_Y, 320, HDR_H, PAL_PANEL);
    hline(0, HDR_Y + HDR_H - 1, 320, PAL_ACCENT);

    text(M, HDR_Y + 3, "TRACKER", PAL_TITLE);

    const char *st = "STOP"; uint8_t sc = PAL_RED;
    if (__mod_playing && !__mod_paused) { st = "PLAY"; sc = PAL_GREEN; }
    else if (__mod_paused) { st = "PAUS"; sc = PAL_YELLOW; }
    text_r(316, HDR_Y + 3, st, sc);

    if (__mod_loaded) {
        const char *n = of_mod_get_name();
        if (n && n[0]) text(M, HDR_Y + 12, n, PAL_TEXT_HI);

        struct xmp_module_info mi;
        xmp_get_module_info(__mod_ctx, &mi);
        char b[24];
        snprintf(b, sizeof(b), "%s %dch", mi.mod->type, mi.mod->chn);
        text_r(316, HDR_Y + 12, b, PAL_ACCENT);
    }
}

static void ui_info(void) {
    rect(0, INF_Y, 320, INF_H, PAL_PANEL);

    if (!__mod_playing) {
        text(M, INF_Y + 3, "[A] PLAY", PAL_TEXT);
        return;
    }

    struct xmp_frame_info fi;
    xmp_get_frame_info(__mod_ctx, &fi);
    char b[64];

    pump();  /* <-- sneak in a pump between draws */

    snprintf(b, sizeof(b), "P:%02d R:%02d B:%d",
             fi.pos, fi.row, fi.bpm);
    text(M, INF_Y + 2, b, PAL_TEXT);

    char t1[16], t2[16];
    fmt_time(t1, fi.time);
    fmt_time(t2, fi.total_time);
    snprintf(b, sizeof(b), "%s/%s", t1, t2);
    text_r(316, INF_Y + 2, b, PAL_TEXT_HI);

    /* Progress bar */
    int bx = M, bw = 312, by = INF_Y + 12;
    rect(bx, by, bw, 5, PAL_BG);
    hline(bx, by, bw, PAL_BORDER);
    hline(bx, by + 4, bw, PAL_BORDER);
    if (fi.total_time > 0) {
        int f = (int)((long)fi.time * (bw - 2) / fi.total_time);
        if (f > bw - 2) f = bw - 2;
        if (f > 0) rect(bx + 1, by + 1, f, 3, PAL_PROGRESS);
    }
}

static void disable_ui_scope(void) {
    /* Clear scope area */
    rect(0, MID_Y, 320, MID_H, PAL_BG);
}

static void ui_scope(void) {
    /* Clear scope area */
    rect(0, MID_Y, 320, MID_H, PAL_BG);

    /* Center line — dotted, every 4th pixel */
    int cy = MID_Y + MID_H / 2;
    uint8_t *row = fb + cy * 320;
    for (int x = 2; x < 318; x += 4)
        row[x] = PAL_SCOPE_DIM;

    if (!__mod_playing || __mod_paused) return;

    pump();  /* <-- pump between sections */

    /* Draw waveform — direct writes, no per-pixel bounds check */
    int half = MID_H / 2 - 2;
    int prev_y = cy;

    for (int x = 1; x < 319; x++) {
        int s = scope[x];
        int y = cy - (s * half / 32768);
        if (y < MID_Y + 1) y = MID_Y + 1;
        if (y > MID_Y + MID_H - 2) y = MID_Y + MID_H - 2;

        /* Vertical line from prev_y to y */
        int y0 = prev_y, y1 = y;
        if (y0 > y1) { y0 = y; y1 = prev_y; }
        for (int yy = y0; yy <= y1; yy++)
            PX(x, yy, PAL_SCOPE);

        prev_y = y;
    }
}

static void ui_channels(void) {
    rect(0, MID_Y, 320, MID_H, PAL_PANEL);

    if (!__mod_loaded) return;

    struct xmp_module_info mi;
    xmp_get_module_info(__mod_ctx, &mi);
    int nch = mi.mod->chn;
    if (nch <= 0) return;
    if (nch > 32) nch = 32;

    int bw = (312) / nch;
    if (bw < 3) bw = 3;
    if (bw > 14) bw = 14;
    int gap = bw > 4 ? 1 : 0;
    int xs = (320 - nch * bw) / 2;
    int mh = MID_H - 8;
    int bot = MID_Y + MID_H - 3;

    pump();  /* <-- pump between sections */

    for (int i = 0; i < nch; i++) {
        int bx = xs + i * bw;
        int v = ch_vol[i];
        int bh = v * mh / 128;
        if (bh > mh) bh = mh;
        int bar_w = bw - gap * 2;

        /* Draw bar — single color per segment for speed */
        int seg_lo = mh * 30 / 100;
        int seg_mid = mh * 65 / 100;
        int seg_hi = mh * 85 / 100;

        for (int dy = 0; dy < bh; dy++) {
            uint8_t c = dy > seg_hi ? PAL_VU_PEAK :
                        dy > seg_mid ? PAL_VU_HI :
                        dy > seg_lo ? PAL_VU_MID : PAL_VU_LO;
            hline(bx + gap, bot - dy, bar_w, c);
        }

        /* Peak marker */
        int ph = ch_pk[i] * mh / 128;
        if (ph > 1) hline(bx + gap, bot - ph, bar_w, PAL_TEXT_HI);
    }
}

static void ui_bottom(void) {
    rect(0, BOT_Y, 320, BOT_H, PAL_PANEL);
    hline(0, BOT_Y, 320, PAL_BORDER);

    /* Volume */
    text(M, BOT_Y + 3, "VOL", PAL_TEXT);
    int vx = M + 18, vw = 52;
    rect(vx, BOT_Y + 3, vw, 5, PAL_BG);
    int f = __mod_volume * (vw - 2) / 100;
    if (f > 0) {
        uint8_t vc = __mod_volume > 90 ? PAL_RED :
                     __mod_volume > 60 ? PAL_YELLOW : PAL_GREEN;
        rect(vx + 1, BOT_Y + 4, f, 3, vc);
    }
    char b[8]; snprintf(b, sizeof(b), "%d%%", __mod_volume);
    text(vx + vw + 3, BOT_Y + 3, b, PAL_TEXT_HI);

    /* Help */
    text(M, BOT_Y + 11, "A:Play B:Stop LR:Vol X:VU", PAL_TEXT);
}

/* ── VU update ──────────────────────────────────────────────────── */

static void update_vu(void) {
     if (!__mod_playing || __mod_paused) {
        for (int i = 0; i < XMP_MAX_CHANNELS; i++) {
            if (ch_vol[i] > 4) ch_vol[i] -= 4; else ch_vol[i] = 0;
            if (ch_pk[i] > 2) ch_pk[i] -= 2; else ch_pk[i] = 0;
        }
        return;
    }
    struct xmp_frame_info fi;
    xmp_get_frame_info(__mod_ctx, &fi);
    for (int i = 0; i < XMP_MAX_CHANNELS; i++) {
        int v = fi.channel_info[i].volume * 2;
        if (v > 128) v = 128;
        ch_vol[i] = (uint8_t)v;
        if (v > ch_pk[i]) ch_pk[i] = (uint8_t)v;
    }
    if (++decay_cnt >= 4) {
        decay_cnt = 0;
        for (int i = 0; i < XMP_MAX_CHANNELS; i++)
            if (ch_pk[i] > 2) ch_pk[i] -= 2; else ch_pk[i] = 0;
    }
}

/* ══════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════ */

int main(void) {
    of_video_init();
    of_video_set_color_mode(OF_VIDEO_MODE_8BIT);
    of_video_set_display_mode(OF_DISPLAY_FRAMEBUFFER);
    of_audio_init();
    

    /* Install idle hook so audio keeps pumping during file I/O */
    of_set_idle_hook(idle_hook);


    /* Show app cover 5s */
    setup_cover_pal();
    fb = of_video_surface();
    memcpy(fb, cover_map, 76800);
    of_video_flip();
    
    uint32_t last_ui = of_time_ms();
    uint32_t now;
    for(;;) {
        now = of_time_ms();
        if (now - last_ui >= 5000) {
            break;
        }
    }
    of_video_flip();

    setup_palette();
    of_video_clear(PAL_BG);
    //of_video_flip();

    memset(scope, 0, sizeof(scope));
    memset(ch_vol, 0, sizeof(ch_vol));
    memset(ch_pk, 0, sizeof(ch_pk));

    /* Load module */
    int ok = 0;
    if (of_mod_load("music.xm") == 0) ok = 1;
    else if (of_mod_load("music.mod") == 0) ok = 1;
    else if (of_mod_load("music.s3m") == 0) ok = 1;
    else if (of_mod_load("music.it") == 0) ok = 1;

    //default volume to 60%
    of_mod_set_volume(60);

    /* Pre-fill audio before first frame */
    if (ok) {
        of_mod_play();
        pump();
        pump();
    }

    for (;;) {
        /* AUDIO FIRST — always */
        pump();

        of_input_poll();
        fb = of_video_surface();

        /* Controls */
        if (of_btn_pressed(OF_BTN_A)) {
            if (__mod_playing && !__mod_paused) of_mod_pause();
            else if (__mod_paused) of_mod_resume();
            else if (ok) of_mod_play();
        }
        if (of_btn_pressed(OF_BTN_B)) of_mod_stop();
        if (of_btn_pressed(OF_BTN_L1)) of_mod_set_volume(of_mod_get_volume()-5);
        if (of_btn_pressed(OF_BTN_R1)) of_mod_set_volume(of_mod_get_volume()+5);
        if (of_btn_pressed(OF_BTN_LEFT) && __mod_playing)
            xmp_prev_position(__mod_ctx);
        if (of_btn_pressed(OF_BTN_RIGHT) && __mod_playing)
            xmp_next_position(__mod_ctx);
        if (of_btn_pressed(OF_BTN_X)) { if (view_mode == 2) view_mode=0; else view_mode++; }
        if (of_btn_pressed(OF_BTN_START)) { of_mod_free(); of_exit(); }

        /* Pump again after controls */
        pump();

        /* Draw UI at ~10fps — leaves more CPU for audio */
        now = of_time_ms();
        if (now - last_ui >= 100) {
            last_ui = now;

            update_vu();
            ui_header();

            pump();  /* pump between UI sections */

            ui_info();

            if (view_mode == 0) 
                ui_scope(); 
            else if (view_mode == 1) 
                ui_channels();
            else
                disable_ui_scope();

            pump();  /* pump before bottom bar */

            ui_bottom();
            of_video_flip();
        }
    }
}