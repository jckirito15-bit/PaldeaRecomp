// Pokémon Scarlet & Violet's camera projection for a window that isn't 16:9 (the "Unlocked" aspect ratio), as a pure
// function: runtime/hle/widescreen.c (rt_sv_proj_aspect / rt_sv_proj_half_fovy) calls it from the patched projection
// builder (tools/sv_patches.py), and tools/test_widescreen_proj.c checks its math against the game's own builder.
//
// The game's builder (0x7000cf0ef4, perspective cameras: [cam+0x180] == 0) makes P00 = cot(fovy / 2) / aspect and
// P11 = cot(fovy / 2) from the camera's aspect field [cam+0x190] (16/9) and its fovy [+0x18c].
// The renderer allocates every 16:9 render target `wide` times wider (runtime/nvn/vulkan/textures.c tex_scalable,
// runtime/nvn/output_scale.c: wide = window aspect / 16:9), so a 16:9 camera drawing into one must see `wide` times
// the 16:9 aspect, or its picture is stretched across the wider target (e.g. 2x at 5120x1440):
//   wider (wide > 1, 21:9, 32:9): aspect x wide, fovy kept - Hor+: the same vertical view, more on both sides;
//   taller (wide < 1, 16:10, 4:3): aspect x wide and tan(fovy / 2) / wide - Vert+: the 16:9 horizontal view, more
//   above and below (P00 unchanged; aspect alone would crop the sides instead).
// Only cameras with the 16:9 aspect change: one with another aspect (a square or 4:3 render-to-texture view) draws
// into a target the renderer doesn't reshape either. Orthographic cameras (the 2D layouts) take the other branch.
#pragma once
#include <math.h>
#include <stddef.h>

#define WIDESCREEN_PROJ_ASPECT_16_9 (16.0f / 9.0f)

// aspect: the camera's aspect field; wide: the output's aspect over 16:9 (1: Original 16:9, or a 16:9 window);
// half_fovy: optional pointer to fovy / 2 in radians, adjusted in place for taller outputs. Returns the aspect to
// build with.
static inline float widescreen_proj_aspect(float aspect, float wide, float *half_fovy) {
    // (a window of a few pixels, a minimized one: no shape to follow; NaN fails every comparison)
    if (!(wide > 0.05f && wide < 20.0f) || fabsf(wide - 1.0f) < 1e-6f) return aspect;
    if (!(fabsf(aspect - WIDESCREEN_PROJ_ASPECT_16_9) < 0.01f)) return aspect;
    if (wide < 1.0f && half_fovy != NULL) {
        float h = *half_fovy;
        if (!(h > 0.0f && h < 1.5707f)) return aspect;  // (not a usable perspective: leave the camera as it is)
        *half_fovy = atanf(tanf(h) / wide);
    }
    return aspect * wide;
}
