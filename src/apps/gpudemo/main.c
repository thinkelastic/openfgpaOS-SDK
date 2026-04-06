/*
 * openfpgaOS GPU Accelerator Demo
 *
 * Showcases the hardware 3D GPU: span rasterisation (Doom-style
 * textured columns + floor spans) and triangle rasterisation
 * (rotating 3D cube with colormapped shading).
 *
 * Controls:
 *   D-pad left/right  — rotate cube
 *   A button           — toggle between span demo and triangle demo
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "of.h"
#include "of_cache.h"
#include "of_gpu.h"

#define SCREEN_W 320
#define SCREEN_H 200

/* Ring buffer is now 16 KB M10K BRAM inside the GPU — no CPU allocation needed */

/* Texture data in SDRAM */
static uint8_t __attribute__((section(".sdram")))
    checkerboard_tex[64 * 64];

static uint8_t __attribute__((section(".sdram")))
    wall_tex[64 * 64];

/* Colormap: 64 light levels */
static uint8_t colormap[64 * 256];

/* Simple sin/cos LUT (8-bit fixed-point, 256 entries = full circle) */
static int16_t sin_lut[256];
static int16_t cos_lut[256];

static void build_sin_table(void) {
    for (int i = 0; i < 256; i++) {
        double a = i * 2.0 * 3.14159265 / 256.0;
        sin_lut[i] = (int16_t)(sin(a) * 256.0);
        cos_lut[i] = (int16_t)(cos(a) * 256.0);
    }
}

static void build_colormap(void) {
    /* Generate a simple colormap: each light level dims the palette */
    for (int light = 0; light < 64; light++) {
        for (int i = 0; i < 256; i++) {
            int dimmed = (i * (63 - light)) / 63;
            if (dimmed > 255) dimmed = 255;
            colormap[light * 256 + i] = (uint8_t)dimmed;
        }
    }
}

static void build_checkerboard(void) {
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            checkerboard_tex[y * 64 + x] = ((x ^ y) & 8) ? 0xE0 : 0x40;
}

static void build_wall_texture(void) {
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            /* Brick-like pattern */
            int bx = x % 32, by = y % 16;
            int offset = (y / 16) & 1 ? 16 : 0;
            bx = (x + offset) % 32;
            if (bx == 0 || by == 0)
                wall_tex[y * 64 + x] = 0x30;  /* mortar */
            else
                wall_tex[y * 64 + x] = 0x90 + (bx & 3) * 4;  /* brick */
        }
}

static void set_palette(void) {
    /* Simple grayscale + warm tint for upper half */
    for (int i = 0; i < 256; i++) {
        uint8_t r = i, g = i, b = i;
        if (i >= 0x80) { r = i; g = (i * 7) >> 3; b = (i * 5) >> 3; }
        of_video_palette(i, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
    }
}

/* ================================================================
 * Span Demo: Doom-style textured walls + floor
 * ================================================================ */
static void draw_span_demo(int frame) {
    uint32_t fb_addr = (uint32_t)(uintptr_t)of_video_surface();
    uint32_t tex_addr = (uint32_t)(uintptr_t)wall_tex;
    uint32_t floor_addr = (uint32_t)(uintptr_t)checkerboard_tex;

    of_gpu_set_framebuffer(fb_addr, SCREEN_W);
    of_gpu_clear(OF_GPU_CLEAR_COLOR, 0x10, 0);

    /* Draw vertical wall columns (Doom R_DrawColumn style) */
    int wall_h = 80 + sin_lut[frame & 255] / 8;
    if (wall_h < 20) wall_h = 20;
    if (wall_h > 180) wall_h = 180;
    int wall_top = (SCREEN_H - wall_h) / 2;

    for (int x = 0; x < SCREEN_W; x++) {
        int light = (x * 40 / SCREEN_W);  /* darken towards edges */
        /* Column walks T (row) through the 64×64 texture, S (col) is fixed.
         * tex_width=64: addr = base + (t>>16)*64 + (s>>16) */
        of_gpu_span_t col = {
            .fb_addr   = fb_addr + wall_top * SCREEN_W + x,
            .tex_addr  = tex_addr,
            .s         = ((x + frame) & 63) << 16,  /* fixed U column */
            .t         = 0,                           /* start at row 0 */
            .sstep     = 0,                           /* S doesn't change */
            .tstep     = (64 << 16) / wall_h,        /* T walks down */
            .count     = wall_h,
            .light     = light,
            .flags     = OF_GPU_SPAN_COLORMAP | OF_GPU_SPAN_COLUMN,
            .fb_stride = SCREEN_W,
            .tex_width = 64,
        };
        of_gpu_draw_span(&col);
    }

    /* Draw floor spans */
    for (int y = wall_top + wall_h; y < SCREEN_H; y++) {
        int dist = y - (SCREEN_H / 2);
        if (dist <= 0) dist = 1;
        int light = (dist > 50) ? 50 : dist;
        int step = (64 << 16) / (dist * 2);
        of_gpu_span_t span = {
            .fb_addr   = fb_addr + y * SCREEN_W,
            .tex_addr  = floor_addr,
            .s         = (frame * 0x4000),
            .t         = (y * 0x8000),
            .sstep     = step,
            .tstep     = 0,
            .count     = SCREEN_W,
            .light     = light > 63 ? 63 : light,
            .flags     = OF_GPU_SPAN_COLORMAP,
            .fb_stride = 1,
            .tex_width = 64,
        };
        of_gpu_draw_span(&span);
    }

    of_gpu_finish();
}

/* ================================================================
 * Triangle Demo: Rotating 3D cube
 * ================================================================ */

/* Cube: 8 vertices, 12 triangles (2 per face) */
static const int8_t cube_verts[8][3] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
};
static const uint8_t cube_faces[12][3] = {
    {0,1,2},{0,2,3}, {4,6,5},{4,7,6},  /* front, back */
    {0,4,5},{0,5,1}, {2,6,7},{2,7,3},  /* bottom, top */
    {0,3,7},{0,7,4}, {1,5,6},{1,6,2},  /* left, right */
};
static const uint8_t face_colors[6] = { 0xC0, 0xA0, 0x80, 0xE0, 0x60, 0xB0 };

/* Per-face 1-texel textures — avoids race between CPU write and GPU read. */
static uint8_t __attribute__((section(".sdram")))
    face_tex[6] __attribute__((aligned(4)));

static void draw_triangle_demo(int frame) {
    uint32_t fb_addr = (uint32_t)(uintptr_t)of_video_surface();

    of_gpu_set_framebuffer(fb_addr, SCREEN_W);
    of_gpu_clear(OF_GPU_CLEAR_COLOR, 0x08, 0);

    /* Initialise per-face texels and flush to SDRAM before GPU draws */
    for (int i = 0; i < 6; i++)
        face_tex[i] = face_colors[i];
    OF_SVC->cache_clean_range(face_tex, sizeof(face_tex));

    int angle_y = frame * 2;
    int angle_x = frame;
    int sy = sin_lut[angle_y & 255], cy = cos_lut[angle_y & 255];
    int sx = sin_lut[angle_x & 255], cx = cos_lut[angle_x & 255];

    /* Transform and project vertices */
    int16_t proj_x[8], proj_y[8];
    for (int i = 0; i < 8; i++) {
        int x = cube_verts[i][0] * 80;
        int y = cube_verts[i][1] * 80;
        int z = cube_verts[i][2] * 80;
        /* Rotate Y */
        int rx = (x * cy - z * sy) >> 8;
        int rz = (x * sy + z * cy) >> 8;
        /* Rotate X */
        int ry = (y * cx - rz * sx) >> 8;
        rz = (y * sx + rz * cx) >> 8;
        /* Perspective projection */
        int d = 300 + rz;
        if (d < 50) d = 50;
        proj_x[i] = (int16_t)(160 + (rx * 200) / d);
        proj_y[i] = (int16_t)(100 + (ry * 200) / d);
    }

    /* Draw each face as 2 triangles */
    for (int f = 0; f < 12; f++) {
        int i0 = cube_faces[f][0];
        int i1 = cube_faces[f][1];
        int i2 = cube_faces[f][2];

        /* Simple backface cull via cross product */
        int dx1 = proj_x[i1] - proj_x[i0], dy1 = proj_y[i1] - proj_y[i0];
        int dx2 = proj_x[i2] - proj_x[i0], dy2 = proj_y[i2] - proj_y[i0];
        if (dx1 * dy2 - dx2 * dy1 <= 0) continue;

        /* Bind per-face 1-texel texture (no race: each face has its own) */
        of_gpu_texture_t solid_tex = {
            .addr = (uint32_t)(uintptr_t)&face_tex[f / 2],
            .width = 1, .height = 1,
            .format = OF_GPU_TEXFMT_I8,
            .wrap_s = OF_GPU_WRAP_REPEAT, .wrap_t = OF_GPU_WRAP_REPEAT,
        };
        of_gpu_bind_texture(&solid_tex);

        of_gpu_vertex_t tri[3] = {
            { .x = proj_x[i0] * 16, .y = proj_y[i0] * 16, .r = 0 },
            { .x = proj_x[i1] * 16, .y = proj_y[i1] * 16, .r = 0 },
            { .x = proj_x[i2] * 16, .y = proj_y[i2] * 16, .r = 0 },
        };
        of_gpu_draw_triangles(tri, 3);
    }

    of_gpu_finish();
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void) {
    of_video_init();
    set_palette();

    printf("GPU Demo: initialising...\n");

    build_sin_table();
    build_colormap();
    build_checkerboard();
    build_wall_texture();

    /* Flush texture data from CPU D-cache to SDRAM so the GPU can read it.
     * The GPU reads SDRAM directly — it never sees the CPU's cache. */
    OF_SVC->cache_clean_range(checkerboard_tex, sizeof(checkerboard_tex));
    OF_SVC->cache_clean_range(wall_tex, sizeof(wall_tex));
    OF_SVC->cache_clean_range(face_tex, sizeof(face_tex));

    /* Init GPU (ring buffer is 16 KB M10K BRAM inside the GPU) */
    of_gpu_init();
    of_gpu_colormap_upload(colormap, sizeof(colormap));

    printf("GPU Demo: ready. A = toggle mode, D-pad = rotate\n");

    int mode = 0;  /* 0 = span demo, 1 = triangle demo */
    int frame = 0;

    while (1) {
        of_input_poll();
        if (of_btn_pressed(OF_BTN_A)) mode ^= 1;

        if (mode == 0)
            draw_span_demo(frame);
        else
            draw_triangle_demo(frame);

        of_video_flip();
        frame++;
    }
}
