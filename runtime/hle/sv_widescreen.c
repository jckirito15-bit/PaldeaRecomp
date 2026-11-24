// Pokémon Scarlet & Violet: the perspective cameras' half-fovy and aspect (tools/sv_patches.py, 0x7000cf0ef4; the math:
// widescreen_proj.h, from PokeV-Recomp). Every 16:9 camera follows the window's shape - Hor+ on wider outputs, Vert+ on
// taller ones. The aspect field itself keeps the game's 16:9 (the game's dirty-check compares it).
#include <math.h>
#include <stddef.h>

#include "../hle.h"
#include "../settings.h"
#include "widescreen_proj.h"

extern float widescreen_factor(void);  // runtime/hle/widescreen.c: output aspect / 16:9 (1 = normal)

float rt_sv_proj_half_fovy(float half_fovy, float aspect) {
    (void)widescreen_proj_aspect(aspect, widescreen_factor(), &half_fovy);
    return half_fovy;
}
float rt_sv_proj_aspect(float aspect) {
    float wide = widescreen_factor(), a = widescreen_proj_aspect(aspect, wide, NULL);
    static float logged = 1.0f;
    if (a != aspect && wide != logged) {
        logged = wide;
        LOG("[widescreen] camera aspect %.4f -> %.4f (output %.3f x 16:9)\n", aspect, a, wide);
    }
    return a;
}
