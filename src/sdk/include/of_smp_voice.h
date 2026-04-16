/*
 * of_smp_voice.h -- Software voice engine for sample-based MIDI synthesis.
 *
 * Manages up to 48 simultaneous sample voices with DAHDSR envelopes
 * and dual LFOs, driving the hardware PCM mixer.  All math is
 * fixed-point Q16.16; designed to run in a 1 kHz ISR on RV32IMFC.
 */

#ifndef OF_SMP_VOICE_H
#define OF_SMP_VOICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "of_smp_bank.h"

/* 28 = SC-55 polyphony standard. Hardware mixer has 47 usable slots
 * but running at the full ~48 saturates the allocator — every note-on
 * has to steal, causing constant click/truncate artifacts.  Keeping
 * headroom lets voice_alloc find free or ENV_DONE slots cleanly. */
#define SMP_MAX_VOICES 28

typedef enum {
    ENV_OFF = 0, ENV_DELAY, ENV_ATTACK, ENV_HOLD,
    ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE, ENV_DONE
} env_stage_t;

typedef struct {
    env_stage_t stage;
    int32_t level;      /* Q16.16 (0=silence, 0x10000=full) */
    int32_t rate;       /* level delta per tick (sign depends on stage) */
    int32_t target;     /* level at end of current stage */
    int32_t timer;      /* ticks remaining in delay/hold (counts down) */
} env_state_t;

typedef struct {
    int32_t phase;      /* Q16.16, wraps at 0x10000 (one full cycle) */
    int32_t rate;       /* phase increment per tick */
    int32_t delay_ticks;/* delay countdown (>0 = LFO silent) */
} lfo_state_t;

typedef struct {
    int active;
    const ofsf_zone_t *zone;
    uint8_t midi_ch;
    uint8_t note;
    uint8_t velocity;
    uint8_t sustain_held; /* CC64 holding this note in sustain */
    int mixer_voice;      /* hardware mixer voice index */
    env_state_t vol_env;
    env_state_t mod_env;
    lfo_state_t mod_lfo;
    lfo_state_t vib_lfo;
    uint32_t base_rate_fp16; /* base 16.16 playback rate (no bend/LFO) */
    int16_t cur_filter_fc;
    int16_t cur_filter_q;
    uint32_t age;
} smp_voice_t;

void smp_voice_init(void);
int  smp_voice_note_on(const ofsf_zone_t *zone, int midi_ch, int note,
                       int velocity, const void *sample_base);
void smp_voice_note_off(int midi_ch, int note);
void smp_voice_tick(void);  /* 1 kHz ISR */

/* Diagnostic stats for smp_voice_tick cost.  Task #10 probe: detect
 * whether the tick exceeds its 2 ms budget (500 Hz tick rate).
 * Fields are named cycles_* but actually hold microseconds — the
 * VexRiscv here does not expose rdcycle to user mode, so of_time_us()
 * (kernel ecall) is used instead. */
typedef struct {
    uint32_t cycles_max;     /* worst-case microseconds for a single tick */
    uint32_t cycles_last;    /* microseconds of most recent tick */
    uint32_t spike_count;    /* ticks where microseconds > 2000 */
    uint32_t tick_count;     /* total ticks since reset */
    uint8_t  active_peak;    /* max active voices seen since reset */
    /* Stage histogram — snapshot at the most recent tick. */
    uint8_t  stage_sustain;  /* voices in ENV_SUSTAIN (waiting for note-off) */
    uint8_t  stage_release;  /* voices in ENV_RELEASE (fading out) */
    uint8_t  stage_decay;    /* voices in ENV_DECAY (between attack and sustain) */
    uint8_t  sustain_held;   /* voices with CC64 sustain pedal still holding them */
} smp_tick_stats_t;

void smp_voice_tick_get_stats(smp_tick_stats_t *out);
void smp_voice_tick_reset_stats(void);
void smp_voice_update_volume(int midi_ch, int volume, int expression);
void smp_voice_update_pan(int midi_ch, int pan);
void smp_voice_update_bend(int midi_ch, int bend);
void smp_voice_update_mod(int midi_ch, int mod_depth);
void smp_voice_update_sustain(int midi_ch, int sustain_on);
void smp_voice_update_filter(int midi_ch, int brightness, int resonance);
void smp_voice_all_off(int midi_ch);
void smp_voice_all_off_global(void);
void smp_voice_set_master_volume(int vol);

#ifdef __cplusplus
}
#endif

#endif /* OF_SMP_VOICE_H */
