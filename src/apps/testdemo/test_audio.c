#include "test.h"

void test_audio_ring(void) {
    section_start("Audio FIFO");

    of_audio_init();

    /* Check FIFO has free space */
    int free = of_audio_free();
    ASSERT("fifo free", free > 0);

    /* Write a short test tone directly to FIFO */
    #define TONE_FRAMES 240
    static int16_t tone[TONE_FRAMES * 2];
    for (int i = 0; i < TONE_FRAMES; i++) {
        uint32_t phase = ((uint32_t)i * 440 * 65536 / 48000) & 0xFFFF;
        int32_t tri = (int32_t)(phase & 0xFFFF) - 32768;
        if (tri < 0) tri = -tri;
        tri = (tri - 16384) * 2;
        int16_t s = (int16_t)(tri >> 1);
        tone[i*2] = s;
        tone[i*2+1] = s;
    }

    int written = of_audio_write(tone, TONE_FRAMES);
    ASSERT("write", written > 0);

    /* FIFO should have less free space now */
    int free_after = of_audio_free();
    ASSERT("fifo used", free_after < free);

    (void)written; (void)free_after;

    section_end();
}

void test_mixer(void) {
    section_start("Mixer");

    /* Init mixer: 4 voices, 48kHz output */
    of_mixer_init(4, 48000);
    test_pass("init");

    /* Generate a short 8-bit unsigned PCM tone (~50ms at 11025Hz) */
    #define MIX_TONE_LEN 551
    static uint8_t pcm_tone[MIX_TONE_LEN];
    for (int i = 0; i < MIX_TONE_LEN; i++) {
        /* 440Hz triangle wave, unsigned 8-bit (128 = silence) */
        int phase = (i * 440 / 11025) & 1;
        pcm_tone[i] = phase ? 192 : 64;
    }

    /* Play the tone */
    int voice = of_mixer_play(pcm_tone, MIX_TONE_LEN, 11025, 10, 128);
    ASSERT("play", voice >= 0);

    /* Pump repeatedly with delays to let FIFO drain and voices advance.
     * The tone is ~50ms at 11025Hz; at 48kHz output that's ~2400 samples.
     * Pump in 20ms intervals to mix and push to FIFO. */
    for (int i = 0; i < 20; i++) {
        of_delay_ms(20);
        of_mixer_pump();
    }

    /* After 400ms of pumping, the short tone should have finished */
    int still_active = of_mixer_voice_active(voice);
    ASSERT("voice done", still_active == 0);

    /* Verify we can play another sound (mixer still functional) */
    int v2 = of_mixer_play(pcm_tone, MIX_TONE_LEN, 11025, 10, 64);
    ASSERT("replay", v2 >= 0);

    of_mixer_stop_all();
    test_pass("stop all");

    (void)voice;

    section_end();
}

void test_audio(void) {
    section_start("Audio");

    /* Init audio subsystem */
    of_audio_init();
    test_pass("init");

    /* Check FIFO has free space */
    int free = of_audio_free();
    ASSERT("fifo free", free > 0);

    /* Generate a 440Hz sine wave test tone (50ms)
     * 48kHz stereo int16_t = 48000 pairs/sec */
    {
        #define TONE_RATE 48000
        #define TONE_HZ 440
        #define TONE2_FRAMES 2400  /* 50ms of audio */
        static int16_t tone2[TONE2_FRAMES * 2];  /* stereo pairs */

        for (int i = 0; i < TONE2_FRAMES; i++) {
            /* Phase: 0..65535 wrapping */
            uint32_t phase = (uint32_t)i * TONE_HZ * 65536 / TONE_RATE;
            /* Triangle wave → approximate sine */
            int32_t tri = (int32_t)(phase & 0xFFFF) - 32768;
            if (tri < 0) tri = -tri;
            tri = tri - 16384;  /* center around 0 */
            int16_t sample = (int16_t)(tri * 2);  /* scale to int16 range */
            /* ~50% volume */
            sample >>= 1;
            tone2[i * 2]     = sample;  /* left */
            tone2[i * 2 + 1] = sample;  /* right */
        }

        /* Write to FIFO */
        int written = of_audio_write(tone2, TONE2_FRAMES);
        ASSERT("write tone", written > 0);

        (void)written;
    }

    section_end();
}
