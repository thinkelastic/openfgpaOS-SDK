/*
 * of_modplay.h -- MOD/XM/S3M/IT player for openfpgaOS
 *
 * Wraps libxmp-lite to render tracker music through of_audio_write().
 * Designed to run from the idle hook or be pumped from the game loop.
 *
 * Dependencies:
 *   - libxmp-lite (https://github.com/libxmp/libxmp, MIT license)
 *     Clone into libs/libxmp-lite/ and compile the .c files from
 *     lite/src/ with your SDK toolchain.
 *   - openfpgaOS SDK (of_audio, of_timer)
 *
 * Usage:
 *
 *   #include "of_modplay.h"
 *
 *   // Load from flatfs or memory:
 *   of_mod_load("music.xm");           // from flatfs/fopen
 *   of_mod_load_mem(data, size);        // from memory buffer
 *
 *   // Start playback:
 *   of_mod_play();                      // loop forever
 *   of_mod_play_once();                 // play once
 *
 *   // In your game loop or idle hook:
 *   of_mod_pump();                      // render + enqueue PCM
 *
 *   // Control:
 *   of_mod_set_volume(80);              // 0-100
 *   of_mod_pause();
 *   of_mod_resume();
 *   of_mod_stop();
 *
 *   // Cleanup:
 *   of_mod_free();
 *
 * Build:
 *   Add the libxmp-lite source files to your Makefile SRCS and
 *   add -Ilibs/libxmp-lite/include to CFLAGS.
 *   Define LIBXMP_NO_DEPACKERS and LIBXMP_CORE_PLAYER when compiling
 *   libxmp-lite to minimize code size.
 */

#ifndef OF_MODPLAY_H
#define OF_MODPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "of.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include "xmp.h"  /* libxmp-lite public header */

#define MAX_MODULE_SIZE    (4 * 1024 * 1024)  /* 4MB max */

/* ======================================================================
 * Configuration
 * ====================================================================== */

/* Output sample rate — must match of_audio (48000 Hz) */
#define OF_MOD_RATE     OF_AUDIO_RATE

/* Max samples to render per pump call.
 * One frame at 50Hz/48kHz ≈ 960 stereo pairs.
 * Keep small to avoid blocking the game loop. */
#define OF_MOD_PUMP_MAX 512

/* ======================================================================
 * State
 * ====================================================================== */

static xmp_context __mod_ctx;
static int         __mod_loaded;
static int         __mod_playing;
static int         __mod_paused;
static int         __mod_volume;   /* 0-100 */
static int16_t     __mod_buf[OF_MOD_PUMP_MAX * 2]; /* stereo interleaved */

/* ======================================================================
 * Init / Free
 * ====================================================================== */

static inline void of_mod_init(void) {
    if (__mod_ctx) return;
    __mod_ctx = xmp_create_context();
    __mod_loaded  = 0;
    __mod_playing = 0;
    __mod_paused  = 0;
    __mod_volume  = 60;
}

static inline void of_mod_free(void) {
    if (!__mod_ctx) return;
    if (__mod_playing) {
        xmp_end_player(__mod_ctx);
        __mod_playing = 0;
    }
    if (__mod_loaded) {
        xmp_release_module(__mod_ctx);
        __mod_loaded = 0;
    }
    xmp_free_context(__mod_ctx);
    __mod_ctx = NULL;
}

/* ======================================================================
 * Load
 * ====================================================================== */

/* Load from file path (goes through fopen → flatfs or real FS) */
static inline int of_mod_load(const char *path) {
    of_mod_init();

    if (__mod_playing) { xmp_end_player(__mod_ctx); __mod_playing = 0; }
    if (__mod_loaded)  { xmp_release_module(__mod_ctx); __mod_loaded = 0; }

    /* Read file into memory */
    // FILE *f = fopen(path, "rb");
    // if (!f) {
        /* Try slot directly */
        of_print("  try slot directly...\n");
        FILE *f = fopen("slot:3", "rb");
    //}
    if (!f) return -1;
    
    printf("  File opened!\n");

    uint8_t *mod_buf = (uint8_t *)malloc((size_t)MAX_MODULE_SIZE);
    uint32_t file_size = fread(mod_buf, 1, MAX_MODULE_SIZE, f);

    char buf[64];
    snprintf(buf, sizeof(buf), " Size: %d\n", file_size);
    of_print(buf);

    if (!mod_buf) { fclose(f); return -1; }

    of_print("File readed!\n");

    int ret = xmp_load_module_from_memory(__mod_ctx, mod_buf, (long)file_size);
    free(mod_buf);

    if (ret < 0) return ret;
    of_print("Module readed!\n");
    __mod_loaded = 1;
    return 0;
}

/* Load from memory buffer (zero-copy if buffer persists) */
static inline int of_mod_load_mem(const void *data, uint32_t size) {
    of_mod_init();

    if (__mod_playing) { xmp_end_player(__mod_ctx); __mod_playing = 0; }
    if (__mod_loaded)  { xmp_release_module(__mod_ctx); __mod_loaded = 0; }

    int ret = xmp_load_module_from_memory(__mod_ctx, (void *)data, (long)size);
    if (ret < 0) return ret;
    __mod_loaded = 1;
    return 0;
}

/* ======================================================================
 * Playback control
 * ====================================================================== */

static inline int of_mod_play(void) {
    if (!__mod_loaded) return -1;
    if (__mod_playing) xmp_end_player(__mod_ctx);
    int ret = xmp_start_player(__mod_ctx, OF_MOD_RATE, 0);
    if (ret < 0) return ret;
    xmp_set_player(__mod_ctx, XMP_PLAYER_VOLUME, __mod_volume);
    xmp_set_player(__mod_ctx, XMP_PLAYER_AMP, 0); //adjust XMP_PLAYER amplification: 0 none, 1 default, 2,3 extra amp
    __mod_playing = 1;
    __mod_paused  = 0;
    return 0;
}

static inline int of_mod_play_once(void) {
    int ret = of_mod_play();
    if (ret == 0) {
        /* Disable looping — stop at end */
        xmp_set_player(__mod_ctx, XMP_PLAYER_MIX, 0);
    }
    return ret;
}

static inline void of_mod_stop(void) {
    if (__mod_playing) {
        xmp_end_player(__mod_ctx);
        __mod_playing = 0;
    }
}

static inline void of_mod_pause(void) {
    __mod_paused = 1;
}

static inline void of_mod_resume(void) {
    __mod_paused = 0;
}

static inline int of_mod_is_playing(void) {
    return __mod_playing && !__mod_paused;
}

/* ======================================================================
 * Volume (0-100)
 * ====================================================================== */

static inline void of_mod_set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    __mod_volume = vol;
    if (__mod_playing) {
        xmp_set_player(__mod_ctx, XMP_PLAYER_VOLUME, vol);
    }
}

static inline int of_mod_get_volume(void) {
    return __mod_volume;
}

/* ======================================================================
 * Pump — call from game loop or idle hook
 *
 * Renders one frame of module audio and enqueues it to the
 * hardware audio FIFO via of_audio_write() or of_audio_enqueue().
 * ====================================================================== */

static inline void of_mod_pump(void) {
    if (!__mod_playing || __mod_paused) return;

    /* Check how much space is available in the audio FIFO */
    int free_pairs = of_audio_free();
    if (free_pairs <= 0) return;
    if (free_pairs > OF_MOD_PUMP_MAX) free_pairs = OF_MOD_PUMP_MAX;

    /* Render one frame from libxmp */
    int ret = xmp_play_buffer(__mod_ctx, __mod_buf, free_pairs * 4, 0);
    if (ret < 0) {
        /* End of module or error */
        of_mod_stop();
        return;
    }

    /* Enqueue rendered PCM to hardware */
    of_audio_write(__mod_buf, free_pairs);
}

/* ======================================================================
 * Module info (optional — for display)
 * ====================================================================== */

static inline const char *of_mod_get_name(void) {
    if (!__mod_loaded) return "";
    struct xmp_module_info mi;
    xmp_get_module_info(__mod_ctx, &mi);
    return mi.mod->name;
}

static inline int of_mod_get_position(void) {
    if (!__mod_playing) return 0;
    struct xmp_frame_info fi;
    xmp_get_frame_info(__mod_ctx, &fi);
    return fi.pos;
}

static inline int of_mod_get_total_time(void) {
    if (!__mod_playing) return 0;
    struct xmp_frame_info fi;
    xmp_get_frame_info(__mod_ctx, &fi);
    return fi.total_time;
}

#ifdef __cplusplus
}
#endif

#endif /* OF_MODPLAY_H */
