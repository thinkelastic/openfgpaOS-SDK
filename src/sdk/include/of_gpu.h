/*
 * of_gpu.h -- 3D Accelerator API for openfpgaOS
 *
 * Hardware-accelerated triangle rasterizer with span fast-path.
 * Designed for porting 90s-era 3D games: Quake, Quake2, Duke3D,
 * Doom, Super Mario 64, Half-Life, Descent, Wipeout.
 *
 * Architecture:
 *   CPU writes commands into a ring buffer in SDRAM.
 *   GPU reads via DMA and processes independently.
 *   CPU and GPU run in parallel — fire and forget.
 *
 * Two drawing primitives:
 *   - Triangles: screen-space vertices with texture coords, color, depth.
 *     Supports affine and perspective-correct texturing.
 *   - Spans: pre-rasterized horizontal/vertical pixel runs.
 *     Fast-path for Doom/Duke3D/Quake-style engines that already
 *     generate spans. Bypasses triangle setup entirely.
 *
 * Textures:
 *   8-bit indexed (palette lookup via colormap BRAM) or 16-bit RGB565.
 *   Colormap BRAM enables the classic light×texel→color lookup used by
 *   Doom, Duke3D, Quake, and Descent without burning SDRAM bandwidth.
 *
 * Depth:
 *   16-bit Z-buffer in SRAM. Per-command enable/disable.
 *
 * Sync:
 *   Fence tokens for CPU-GPU synchronization. Poll or wait.
 *
 * Resource budget: ~5K ALMs, 64 M10K blocks, ~6 DSP blocks.
 *
 * Example — triangle-based game (SM64, Wipeout):
 *
 *   of_gpu_init();
 *   while (1) {
 *       of_gpu_clear(OF_GPU_CLEAR_COLOR | OF_GPU_CLEAR_DEPTH, 0, 0xFFFF);
 *       of_gpu_bind_texture(&level_tex);
 *       of_gpu_depth_test(OF_GPU_DEPTH_LESS);
 *       of_gpu_draw_triangles(verts, tri_count * 3);
 *       uint32_t fence = of_gpu_fence();
 *       of_gpu_kick();
 *       // ... game logic, audio, input while GPU works ...
 *       of_gpu_wait(fence);
 *       of_video_flip();
 *   }
 *
 * Example — span-based game (Doom, Duke3D):
 *
 *   of_gpu_init();
 *   of_gpu_colormap_upload(palette_lookup, 16384);
 *   while (1) {
 *       // Engine generates spans as usual
 *       of_gpu_draw_span(&floor_span);
 *       of_gpu_draw_span(&wall_column);
 *       uint32_t fence = of_gpu_fence();
 *       of_gpu_kick();
 *       // ... BSP traversal, game logic ...
 *       of_gpu_wait(fence);
 *       of_video_flip();
 *   }
 */

#ifndef OF_GPU_H
#define OF_GPU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ======================================================================
 * Constants
 * ====================================================================== */

/* Clear flags (bitmask) */
#define OF_GPU_CLEAR_COLOR      (1 << 0)
#define OF_GPU_CLEAR_DEPTH      (1 << 1)

/* Depth test functions */
typedef enum {
    OF_GPU_DEPTH_NONE   = 0,    /* Disabled — no read, no write */
    OF_GPU_DEPTH_ALWAYS = 1,    /* Always pass, write Z */
    OF_GPU_DEPTH_LESS   = 2,    /* Pass if fragment Z < buffer Z */
    OF_GPU_DEPTH_LEQUAL = 3,    /* Pass if fragment Z <= buffer Z */
    OF_GPU_DEPTH_EQUAL  = 4,    /* Pass if fragment Z == buffer Z */
} of_gpu_depth_func_t;

/* Blend modes */
typedef enum {
    OF_GPU_BLEND_NONE    = 0,   /* Opaque write */
    OF_GPU_BLEND_ALPHA   = 1,   /* Skip pixel if alpha test fails */
    OF_GPU_BLEND_ADD     = 2,   /* Additive (for future expansion) */
} of_gpu_blend_t;

/* Texture formats */
typedef enum {
    OF_GPU_TEXFMT_I8     = 0,   /* 8-bit indexed → colormap BRAM lookup */
    OF_GPU_TEXFMT_RGB565 = 1,   /* 16-bit direct color */
} of_gpu_texfmt_t;

/* Texture wrap modes */
typedef enum {
    OF_GPU_WRAP_REPEAT = 0,     /* Tile (power-of-2 mask) */
    OF_GPU_WRAP_CLAMP  = 1,     /* Clamp to edge */
} of_gpu_wrap_t;

/* Span flags (bitmask) */
#define OF_GPU_SPAN_COLORMAP    (1 << 0)  /* Apply colormap BRAM lookup */
#define OF_GPU_SPAN_COLUMN      (1 << 1)  /* Vertical (stride-based advance) */
#define OF_GPU_SPAN_SKIP_ZERO   (1 << 2)  /* Skip transparent texels (index 0 or 255) */
#define OF_GPU_SPAN_DEPTH_TEST  (1 << 3)  /* Z-test per pixel */
#define OF_GPU_SPAN_DEPTH_WRITE (1 << 4)  /* Write Z per pixel */
#define OF_GPU_SPAN_PERSP       (1 << 5)  /* Perspective-correct subdivision */

/* ======================================================================
 * Vertex format
 *
 * Screen-space: CPU does all transforms, GPU does rasterize + shade.
 * Coordinates are 12.4 fixed-point for sub-pixel precision.
 * Texture coords are 16.16 fixed-point.
 * ====================================================================== */

typedef struct {
    int16_t  x, y;          /* Screen position, 12.4 fixed-point */
    uint16_t z;             /* Depth, 16-bit (0 = near, 0xFFFF = far) */
    uint16_t pad;           /* Alignment */
    int32_t  s, t;          /* Texture coords, 16.16 fixed-point */
    int32_t  w;             /* 1/W for perspective correction, 16.16 */
                            /* Set to 0x00010000 (1.0) for affine */
    uint8_t  r, g, b, a;   /* Vertex color / light level */
} of_gpu_vertex_t;          /* 24 bytes */

/* ======================================================================
 * Texture descriptor
 * ====================================================================== */

typedef struct {
    uint32_t          addr;         /* SDRAM byte address of texel data */
    uint16_t          width;        /* Texels (must be power of 2) */
    uint16_t          height;       /* Texels (must be power of 2) */
    of_gpu_texfmt_t   format;       /* Texel format */
    of_gpu_wrap_t     wrap_s;       /* Horizontal wrap */
    of_gpu_wrap_t     wrap_t;       /* Vertical wrap */
} of_gpu_texture_t;

/* ======================================================================
 * Span descriptor (fast-path for Doom/Duke3D/Quake span engines)
 *
 * Bypasses triangle setup. The CPU provides pre-computed per-pixel
 * stepping parameters directly, just like PocketQuake's MMIO interface.
 * ====================================================================== */

typedef struct {
    uint32_t fb_addr;       /* Framebuffer destination byte address */
    uint32_t tex_addr;      /* Texture base byte address in SDRAM */
    int32_t  s, t;          /* Initial tex coords, 16.16 fixed-point */
    int32_t  sstep, tstep;  /* Per-pixel tex coord step, 16.16 */
    uint16_t count;         /* Number of pixels */
    uint8_t  light;         /* Light/shade level (colormap row index) */
    uint8_t  flags;         /* OF_GPU_SPAN_* bitmask */
    int16_t  fb_stride;     /* FB advance per pixel (1=horiz, 320=column) */
    /* Texture addressing mode (selects between multiply and shift) */
    uint16_t tex_width;     /* Quake: t * width + s (multiply mode) */
    uint8_t  tex_shift;     /* Duke3D/Doom: (t >> shift) << bits | ... */
    uint8_t  tex_bits;      /* Duke3D/Doom: bit-combine width */
    /* Optional: depth buffer */
    uint32_t z_addr;        /* Z-buffer start address (SRAM) */
    int32_t  zi;            /* Initial 1/Z, fixed-point */
    int32_t  zistep;        /* Per-pixel 1/Z step */
    /* Optional: perspective correction (Quake-style 16px subdivision) */
    int32_t  sdivz, tdivz;  /* S/Z, T/Z at span start */
    int32_t  zi_persp;      /* 1/Z at span start */
    int32_t  sdivz_step;    /* S/Z step per 16 pixels */
    int32_t  tdivz_step;    /* T/Z step per 16 pixels */
    int32_t  zi_step;       /* 1/Z step per 16 pixels */
} of_gpu_span_t;

/* ======================================================================
 * Initialization
 * ====================================================================== */

/* Initialize the GPU. Call once at startup.
 * Sets up the command ring buffer, resets GPU state. */
void of_gpu_init(void);

/* Shut down the GPU. Waits for pending work, releases resources. */
void of_gpu_shutdown(void);

/* ======================================================================
 * Colormap BRAM
 *
 * Upload a lookup table for indexed textures:
 *   output_color = colormap[light_level * 256 + texel_index]
 *
 * Doom:   colormap.lmp (34 × 256 = 8704 bytes)
 * Duke3D: palookup tables (shade × 256, up to 64 × 256 = 16384 bytes)
 * Quake:  colormap (64 × 256 = 16384 bytes)
 * Descent: similar lighting tables
 *
 * Max 16KB (64 light levels × 256 palette entries).
 * ====================================================================== */

void of_gpu_colormap_upload(const uint8_t *data, uint32_t size);

/* ======================================================================
 * GPU State Commands
 *
 * State is "sticky" — set once, applies to all subsequent draws
 * until changed. State changes are recorded into the command ring
 * and take effect in order.
 * ====================================================================== */

/* Bind a texture for subsequent triangle draws. */
void of_gpu_bind_texture(const of_gpu_texture_t *tex);

/* Set depth test function. OF_GPU_DEPTH_NONE to disable. */
void of_gpu_depth_test(of_gpu_depth_func_t func);

/* Set blend mode. */
void of_gpu_blend(of_gpu_blend_t mode);

/* Set alpha test reference value (for OF_GPU_BLEND_ALPHA).
 * Fragments with alpha < ref are discarded. */
void of_gpu_alpha_ref(uint8_t ref);

/* Set the framebuffer target for triangle rendering.
 * Normally set automatically from of_video_surface(), but can be
 * overridden for render-to-texture or shadow maps. */
void of_gpu_set_framebuffer(uint32_t addr, uint16_t stride);

/* Set the Z-buffer target (SRAM address). */
void of_gpu_set_zbuffer(uint32_t addr, uint16_t stride);

/* Set vertex color interpretation:
 *   For I8 textures:  vertex R channel = colormap light level
 *   For RGB565:       vertex RGB = modulate color (Gouraud) */
void of_gpu_shade_mode(bool gouraud);

/* ======================================================================
 * Draw Commands
 * ====================================================================== */

/* Clear framebuffer and/or depth buffer.
 * flags: OF_GPU_CLEAR_COLOR | OF_GPU_CLEAR_DEPTH
 * color: palette index (8-bit) or packed RGB565
 * depth: 16-bit Z value (typically 0xFFFF for far) */
void of_gpu_clear(uint32_t flags, uint16_t color, uint16_t depth);

/* Draw triangles from a vertex array.
 * Every 3 vertices form one triangle. count must be a multiple of 3.
 * Vertices are screen-space (CPU does projection).
 * Uses currently bound texture, depth func, blend mode.
 *
 * The vertex data is copied into the command ring — the caller's
 * buffer can be reused immediately after this call returns. */
void of_gpu_draw_triangles(const of_gpu_vertex_t *verts, uint32_t count);

/* Draw indexed triangles.
 * indices: array of uint16_t vertex indices into verts[].
 * idx_count: number of indices (must be multiple of 3).
 * Useful for mesh rendering (SM64, Descent, Wipeout). */
void of_gpu_draw_indexed(const of_gpu_vertex_t *verts, uint32_t vert_count,
                         const uint16_t *indices, uint32_t idx_count);

/* Draw a pre-computed span (fast-path for span-based engines).
 * Bypasses triangle setup — the CPU provides all per-pixel parameters.
 * Span data is copied into the command ring. */
void of_gpu_draw_span(const of_gpu_span_t *span);

/* Batch-submit multiple spans. More efficient than individual calls
 * when the engine generates many spans per frame (Quake, Duke3D). */
void of_gpu_draw_spans(const of_gpu_span_t *spans, uint32_t count);

/* ======================================================================
 * Synchronization
 *
 * The command ring decouples CPU and GPU. The CPU can be many commands
 * ahead. Fences let the CPU know when the GPU has caught up.
 *
 * Typical frame:
 *   1. Record draw commands (of_gpu_clear, of_gpu_draw_*, ...)
 *   2. Insert fence (of_gpu_fence)
 *   3. Kick GPU (of_gpu_kick)
 *   4. Do CPU work (game logic, audio, next frame prep)
 *   5. Wait for fence before flipping (of_gpu_wait)
 * ====================================================================== */

/* Insert a fence into the command stream.
 * Returns a token that can be polled or waited on. */
uint32_t of_gpu_fence(void);

/* Flush buffered commands to the GPU.
 * Commands are not guaranteed to start processing until kick. */
void of_gpu_kick(void);

/* Convenience: fence + kick in one call. Returns fence token. */
uint32_t of_gpu_submit(void);

/* Check if the GPU has reached a fence. Non-blocking. */
bool of_gpu_fence_reached(uint32_t token);

/* Block until the GPU reaches a fence.
 * Spins on the fence — use of_gpu_fence_reached() for non-blocking. */
void of_gpu_wait(uint32_t token);

/* Wait for the GPU to finish ALL pending commands. */
void of_gpu_finish(void);

/* ======================================================================
 * Ring buffer management (advanced)
 *
 * The ring buffer lives in SDRAM. Default size: 64KB.
 * For heavy scenes, a larger ring avoids stalls.
 * ====================================================================== */

/* Query free space in the command ring (bytes). */
uint32_t of_gpu_ring_free(void);

/* Check if the ring has room for at least `bytes` of commands. */
bool of_gpu_ring_can_fit(uint32_t bytes);

/* ======================================================================
 * Status / debug
 * ====================================================================== */

/* Get number of triangles the GPU has drawn since init. */
uint32_t of_gpu_stat_triangles(void);

/* Get number of pixels the GPU has drawn since init. */
uint32_t of_gpu_stat_pixels(void);

/* Get number of texture cache misses since init. */
uint32_t of_gpu_stat_cache_misses(void);

#ifdef __cplusplus
}
#endif

#endif /* OF_GPU_H */
