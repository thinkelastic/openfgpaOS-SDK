/*
 * openfpgaOS MIDI Demo Application
 *
 * MODE 1 (default): plays a MIDI file from slot:3
 * MODE 2 (DPad UP):  diagnostic — plays Guitar/Bass/Snare on loop
 *                    so you can listen and verify each instrument
 *
 * Controls:
 *   START   = play/pause
 *   SELECT  = restart
 *   DPad UP = toggle diagnostic mode (guitar/bass/snare)
 *   L1/R1   = volume down/up
 */

#include "of.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* Maximum MIDI file size we support */
#define MIDI_MAX_SIZE   (256 * 1024)

/* Buffer for the loaded MIDI file (aligned for DMA) */
static uint8_t midi_buf[MIDI_MAX_SIZE] __attribute__((aligned(512)));
static uint32_t midi_len;

/* Per-instrument diagnostic MIDIs.
 * Each plays ONE sustained note for ~2 seconds so you can clearly hear
 * the timbre. The loop in main() steps through them with screen labels. */
typedef struct {
    const char *name;
    int program;     /* GM program (0-127), or -1 for drums */
    int channel;     /* MIDI channel */
    int note;        /* MIDI note number */
    uint8_t buf[256];
    uint32_t len;
} diag_inst_t;

static diag_inst_t diag_inst[] = {
    /* GM melodic instruments — each on its own channel */
    { "00 Acoustic Grand Piano",     0, 0, 60, {0}, 0 },
    { "24 Nylon Guitar",            24, 0, 52, {0}, 0 },
    { "27 Electric Guitar Clean",   27, 0, 52, {0}, 0 },
    { "29 Overdriven Guitar",       29, 0, 52, {0}, 0 },
    { "30 Distortion Guitar",       30, 0, 52, {0}, 0 },
    { "33 Electric Bass finger",    33, 1, 28, {0}, 0 },
    { "34 Electric Bass pick",      34, 1, 28, {0}, 0 },
    { "38 Synth Bass 1",            38, 1, 28, {0}, 0 },
    { "56 Trumpet",                 56, 0, 60, {0}, 0 },
    { "65 Alto Sax",                65, 0, 60, {0}, 0 },
    { "73 Flute",                   73, 0, 72, {0}, 0 },
    { "80 Square Lead",             80, 0, 60, {0}, 0 },
    /* Drums (channel 9) */
    { "Drum: Bass Drum",            -1, 9, 35, {0}, 0 },
    { "Drum: Acoustic Snare",       -1, 9, 38, {0}, 0 },
    { "Drum: Closed Hi-Hat",        -1, 9, 42, {0}, 0 },
    { "Drum: Crash Cymbal",         -1, 9, 49, {0}, 0 },
};
#define DIAG_INST_COUNT  (sizeof(diag_inst)/sizeof(diag_inst[0]))

static int vlq_append(uint8_t *p, int *t, uint32_t v) {
    int n = 0;
    if (v < 0x80) {
        p[(*t)++] = v;
        n = 1;
    } else if (v < 0x4000) {
        p[(*t)++] = (v >> 7) | 0x80;
        p[(*t)++] = v & 0x7F;
        n = 2;
    } else {
        p[(*t)++] = (v >> 14) | 0x80;
        p[(*t)++] = ((v >> 7) & 0x7F) | 0x80;
        p[(*t)++] = v & 0x7F;
        n = 3;
    }
    return n;
}

/* Build a tiny single-note MIDI for one instrument. */
static void build_diag_inst(diag_inst_t *d) {
    static const uint8_t hdr[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96
    };
    memcpy(d->buf, hdr, sizeof(hdr));

    static uint8_t track[128];
    int t = 0;

    /* Tempo: 120 BPM */
    track[t++] = 0; track[t++] = 0xFF; track[t++] = 0x51; track[t++] = 0x03;
    track[t++] = 0x07; track[t++] = 0xA1; track[t++] = 0x20;

    /* Program change (skip for drums) */
    if (d->program >= 0) {
        track[t++] = 0;
        track[t++] = 0xC0 | d->channel;
        track[t++] = d->program;
    }
    /* Channel volume max */
    track[t++] = 0; track[t++] = 0xB0 | d->channel; track[t++] = 7; track[t++] = 127;

    /* Sustained note for ~2 seconds (384 ticks at 120 BPM, 96 tpq) */
    track[t++] = 0; track[t++] = 0x90 | d->channel; track[t++] = d->note; track[t++] = 110;
    vlq_append(track, &t, 384);
    track[t++] = 0x80 | d->channel; track[t++] = d->note; track[t++] = 0;

    /* End of track */
    track[t++] = 0; track[t++] = 0xFF; track[t++] = 0x2F; track[t++] = 0x00;

    /* MTrk */
    int o = sizeof(hdr);
    d->buf[o++] = 'M'; d->buf[o++] = 'T'; d->buf[o++] = 'r'; d->buf[o++] = 'k';
    d->buf[o++] = (t >> 24) & 0xFF;
    d->buf[o++] = (t >> 16) & 0xFF;
    d->buf[o++] = (t >> 8) & 0xFF;
    d->buf[o++] = t & 0xFF;
    memcpy(d->buf + o, track, t);
    d->len = o + t;
}

static void build_all_diag(void) {
    for (unsigned i = 0; i < DIAG_INST_COUNT; i++)
        build_diag_inst(&diag_inst[i]);
}

/* ================================================================
 * RAW OPL3 mode — bypasses of_midi entirely.
 * Programs OPL3 channel 0 directly with extreme parameter variations
 * so the user can hear if FB, WS, CNT are taking effect.
 * ================================================================ */
typedef struct {
    const char *name;
    uint8_t mod_char;  /* 0x20: AM/VIB/EGT/KSR/MULT */
    uint8_t mod_tl;    /* 0x40: KSL/TL */
    uint8_t mod_ad;    /* 0x60: AR/DR */
    uint8_t mod_sr;    /* 0x80: SL/RR */
    uint8_t mod_ws;    /* 0xE0: WS */
    uint8_t car_char;  /* 0x23 */
    uint8_t car_tl;    /* 0x43 */
    uint8_t car_ad;    /* 0x63 */
    uint8_t car_sr;    /* 0x83 */
    uint8_t car_ws;    /* 0xE3 */
    uint8_t fb_cnt;    /* 0xC0: feedback + connection */
} raw_patch_t;

static const raw_patch_t raw_patches[] = {
    /* Pure sine carrier, no modulator (CNT=1 additive, mod TL=63 silent) */
    { "RAW: pure sine",
      0x21, 0x3F, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x00,
      0x01 },  /* CNT=1, FB=0 */

    /* Sine carrier with FM modulation, no feedback */
    { "RAW: FM no fb",
      0x21, 0x10, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x00,
      0x00 },  /* CNT=0, FB=0 */

    /* FM with max feedback (FB=7) — should sound very buzzy */
    { "RAW: FM max fb",
      0x21, 0x10, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x00,
      0x0E },  /* CNT=0, FB=7 */

    /* Quarter-sine waveform on carrier — should sound very different */
    { "RAW: car WS=3",
      0x21, 0x3F, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x03,
      0x01 },  /* CNT=1 additive, only car heard */

    /* Half-sine carrier */
    { "RAW: car WS=1",
      0x21, 0x3F, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x01,
      0x01 },

    /* Abs-sine carrier */
    { "RAW: car WS=2",
      0x21, 0x3F, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x02,
      0x01 },

    /* Distortion Guitar (program 30 from of_midi bank) — direct write */
    { "RAW: dist guitar bnk",
      0x21, 0x14, 0xF6, 0x54, 0x02,
      0x21, 0x00, 0xF2, 0x52, 0x02,
      0x06 },  /* FB=3, CNT=0 */

    /* Distortion guitar from DOOM GENMIDI.OP2 (program 30, known good) */
    { "RAW: doom dist gtr",
      0x33, 0x0A, 0xF1, 0xC5, 0x00,
      0x11, 0x00, 0xF1, 0xC5, 0x00,
      0x0E },  /* FB=7, CNT=0 */

    /* Aggressive square-ish sound with max FB and WS=3 quarter-sine */
    { "RAW: aggr square",
      0x21, 0x10, 0xF0, 0x07, 0x03,
      0x21, 0x00, 0xF0, 0x07, 0x03,
      0x0E },  /* FB=7, CNT=0 */

    /* Moderate feedback — should sound buzzy/brassy, clearly tonal */
    { "RAW: hi fb sine",
      0x21, 0x00, 0xF0, 0x07, 0x00,
      0x21, 0x00, 0xF0, 0x07, 0x00,
      0x08 },  /* FB=4, CNT=0 */

    /* Stronger feedback — more metallic, some inharmonic content */
    { "RAW: ext metallic",
      0x21, 0x00, 0xF0, 0x0F, 0x00,
      0x21, 0x00, 0xF0, 0x0F, 0x00,
      0x0A },  /* FB=5, CNT=0 */

    /* EGT=0 A/B test: should decay from max → SL=2 → then slowly to silence */
    { "RAW: EGT=0 slow",
      0x01, 0x00, 0xF4, 0x22, 0x00,  /* EGT=0, DR=4, SL=2, RR=2 (slow) */
      0x01, 0x00, 0xF4, 0x22, 0x00,
      0x01 },

    /* EGT=1 A/B test: same DR/SL — should decay to SL=2 then HOLD there */
    { "RAW: EGT=1 slow",
      0x21, 0x00, 0xF4, 0x22, 0x00,  /* EGT=1, DR=4, SL=2, RR=2 (slow) */
      0x21, 0x00, 0xF4, 0x22, 0x00,
      0x01 },

    /* Sustained: no decay, holds at max volume */
    { "RAW: sustained",
      0x21, 0x00, 0xF0, 0x05, 0x00,  /* EGT=1, DR=0, SL=0, RR=5 */
      0x21, 0x00, 0xF0, 0x05, 0x00,
      0x01 },
};
#define RAW_PATCH_COUNT  (sizeof(raw_patches)/sizeof(raw_patches[0]))

static void raw_program_ch0(const raw_patch_t *p) {
    /* Modulator slot 0 */
    of_audio_opl_write(0x20, p->mod_char);
    of_audio_opl_write(0x40, p->mod_tl);
    of_audio_opl_write(0x60, p->mod_ad);
    of_audio_opl_write(0x80, p->mod_sr);
    of_audio_opl_write(0xE0, p->mod_ws);
    /* Carrier slot 3 */
    of_audio_opl_write(0x23, p->car_char);
    of_audio_opl_write(0x43, p->car_tl);
    of_audio_opl_write(0x63, p->car_ad);
    of_audio_opl_write(0x83, p->car_sr);
    of_audio_opl_write(0xE3, p->car_ws);
    /* FB/CNT + L+R output */
    of_audio_opl_write(0xC0, p->fb_cnt | 0x30);
}

static int raw_block = 4;  /* configurable octave */

static void raw_play_a4(void) {
    /* A note at the configured block.
     * Fnum=0x241 always corresponds to A in the chosen octave.
     * Block=4 should be A4=440Hz. Lower blocks = lower octaves. */
    of_audio_opl_write(0xA0, 0x41);
    /* B0 layout: KON[5] | BLOCK[4:2] | FNUM_HI[1:0]
     * Fnum=0x241 → high bits = 0b10. */
    uint8_t b0 = 0x20 | ((raw_block & 0x07) << 2) | 0x02;
    of_audio_opl_write(0xB0, b0);
}

static void raw_key_off(void) {
    of_audio_opl_write(0xB0, 0x12);
}

__attribute__((unused))
static int load_midi_file(void) {
    /* Demo loads the MIDI file from data slot 3; filename-based access
     * (e.g. fopen("music.mid")) requires SDK plumbing of_file_get_name
     * from the OS, which is not wired up yet. */
    FILE *f = fopen("slot:3", "rb");
    if (!f) return -1;

    size_t n = fread(midi_buf, 1, MIDI_MAX_SIZE, f);
    fclose(f);

    if (n < 14)
        return -2;

    if (midi_buf[0] != 'M' || midi_buf[1] != 'T' ||
        midi_buf[2] != 'h' || midi_buf[3] != 'd')
        return -10;

    midi_len = (uint32_t)n;
    return 0;
}

/* Mode: 0 = MIDI file player, 1 = instrument diagnostic, 2 = raw OPL3 */
#define MODE_PLAY  0
#define MODE_DIAG  1
#define MODE_RAW   2

int main(void) {
    printf("\033[2J\033[H");
    printf("    openfpgaOS MIDI Demo\n");
    printf("    ====================\n\n");

    build_all_diag();

    int rc = of_midi_init();
    if (rc < 0) {
        printf(" MIDI init failed! rc=%d\n", rc);
        while (1) {}
    }

    /* Try to load MIDI file from data slot 3 */
    int have_midi = (load_midi_file() == 0);
    if (have_midi)
        printf(" MIDI file: %u bytes\n", (unsigned)midi_len);
    else
        printf(" No MIDI file in slot 3\n");

    printf(" %u instruments + %u raw patches\n",
           (unsigned)DIAG_INST_COUNT, (unsigned)RAW_PATCH_COUNT);

    int mode = have_midi ? MODE_PLAY : MODE_DIAG;
    int volume = 255;
    int paused = 0;
    int idx = 0;
    int auto_advance = 1;
    uint32_t note_start_ms = 0;

    printf("\n SELECT=switch mode  L1/R1=volume\n");
    printf(" Play: START=pause A=restart\n");
    printf(" Diag: LEFT/RIGHT=prev/next START=auto A=replay\n");
    printf(" Raw:  LEFT/RIGHT=prev/next X/Y=octave\n\n");

    goto enter_mode;

    while (1) {
        of_input_poll();
        of_input_state_t state;
        of_input_state(0, &state);

        /* SELECT = switch mode */
        if (state.buttons_pressed & OF_BTN_SELECT) {
            /* Stop current mode */
            if (mode == MODE_RAW) raw_key_off();
            else of_midi_stop();
            paused = 0;
            idx = 0;

            /* Cycle: play → diag → raw → play ... (skip play if no file) */
            mode = (mode + 1) % 3;
            if (mode == MODE_PLAY && !have_midi) mode = MODE_DIAG;

enter_mode:
            printf("\033[10;2H                                       ");
            if (mode == MODE_PLAY) {
                printf("\033[10;2H MODE: MIDI File Player");
                of_midi_play(midi_buf, midi_len, 1);
            } else if (mode == MODE_DIAG) {
                printf("\033[10;2H MODE: Instrument Diagnostic");
                of_midi_play(diag_inst[idx].buf, diag_inst[idx].len, 0);
                note_start_ms = of_time_ms();
            } else {
                printf("\033[10;2H MODE: RAW OPL3");
                of_audio_opl_reset();
                of_audio_opl_write(0x105, 0x01);
                of_audio_opl_write(0x01, 0x20);
                of_audio_opl_write(0x101, 0x20);
                raw_program_ch0(&raw_patches[idx]);
                raw_play_a4();
            }
            printf("\033[11;2H >>> %-30s",
                   mode == MODE_PLAY ? "Playing MIDI file" :
                   mode == MODE_DIAG ? diag_inst[idx].name :
                   raw_patches[idx].name);
        }

        /* Volume (all modes) */
        if (state.buttons_pressed & OF_BTN_L1) {
            volume -= 32; if (volume < 0) volume = 0;
            of_midi_set_volume(volume);
            printf("\033[13;2H Vol: %3d/255  ", volume);
        }
        if (state.buttons_pressed & OF_BTN_R1) {
            volume += 32; if (volume > 255) volume = 255;
            of_midi_set_volume(volume);
            printf("\033[13;2H Vol: %3d/255  ", volume);
        }

        /* Mode-specific controls */
        if (mode == MODE_PLAY) {
            if (state.buttons_pressed & OF_BTN_START) {
                paused = !paused;
                if (paused) of_midi_pause(); else of_midi_resume();
                printf("\033[12;2H %s   ", paused ? "PAUSED " : "PLAYING");
            }
            if (state.buttons_pressed & OF_BTN_A) {
                of_midi_stop();
                of_midi_play(midi_buf, midi_len, 1);
                paused = 0;
                printf("\033[12;2H RESTART       ");
            }
        } else if (mode == MODE_DIAG) {
            int change = -1;
            if (state.buttons_pressed & OF_BTN_RIGHT)
                change = (idx + 1) % (int)DIAG_INST_COUNT;
            if (state.buttons_pressed & OF_BTN_LEFT)
                change = (idx + (int)DIAG_INST_COUNT - 1) % (int)DIAG_INST_COUNT;
            if (state.buttons_pressed & OF_BTN_A)
                change = idx;
            if (state.buttons_pressed & OF_BTN_START) {
                auto_advance = !auto_advance;
                printf("\033[12;2H Auto: %s   ", auto_advance ? "ON " : "OFF");
            }
            if (auto_advance && change < 0) {
                uint32_t now = of_time_ms();
                if (now - note_start_ms > 2500)
                    change = (idx + 1) % (int)DIAG_INST_COUNT;
            }
            if (change >= 0) {
                of_midi_stop();
                idx = change;
                of_midi_play(diag_inst[idx].buf, diag_inst[idx].len, 0);
                note_start_ms = of_time_ms();
                printf("\033[11;2H >>> %-30s", diag_inst[idx].name);
            }
        } else { /* MODE_RAW */
            int change = -1;
            if (state.buttons_pressed & OF_BTN_RIGHT)
                change = (idx + 1) % (int)RAW_PATCH_COUNT;
            if (state.buttons_pressed & OF_BTN_LEFT)
                change = (idx + (int)RAW_PATCH_COUNT - 1) % (int)RAW_PATCH_COUNT;
            if (state.buttons_pressed & OF_BTN_X) {
                raw_block = (raw_block + 7) & 0x07;
                printf("\033[12;2H Octave: %d  ", raw_block);
                change = idx;
            }
            if (state.buttons_pressed & OF_BTN_Y) {
                raw_block = (raw_block + 1) & 0x07;
                printf("\033[12;2H Octave: %d  ", raw_block);
                change = idx;
            }
            if (change >= 0) {
                raw_key_off();
                idx = change;
                of_audio_opl_reset();
                of_audio_opl_write(0x105, 0x01);
                of_audio_opl_write(0x01, 0x20);
                of_audio_opl_write(0x101, 0x20);
                raw_program_ch0(&raw_patches[idx]);
                raw_play_a4();
                printf("\033[11;2H >>> %-30s", raw_patches[idx].name);
            }
        }

        of_midi_pump();
        usleep(1 * 1000);
    }

    return 0;
}
