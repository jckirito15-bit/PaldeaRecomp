// Scarlet / Violet: how far town people and Pokémon keep their detailed models (F1 > People detail / Pokémon
// detail, settings npc_detail / pokemon_detail).
//
// Every person has three meshes (<name>.trmsh, _lod1, _lod2: chara/model_*), and the game swaps to the simpler ones
// by distance to the camera: the full model only within 8 m, then 15 m, then 30 m - walking up to a bench, its
// person changes model twice. Pokémon (wild and in towns) do the same at 10 / 20 / 30 m. Those distances are
// trinity_ModelLodSetting "chara_model" and "pokemon" (the field's own: 30-1500 m), loaded with the field into the
// model LOD manager. The LOD pick (f_7000b87474) compares the distance with factor[q][i] (manager + 0x90, all 1.0
// here) times the matching setting's distance i, and when the result differs from the model's cached thresholds it
// takes the new ones: a distance changed in the setting applies to every model on screen on its next pick.
//
// The manager: *(*(*(*(main+0x46d4e00)) + 0x38) + 0x50) (vtable main+0x44a00b8); its settings by name, +0x30: a
// hash map whose node list starts at +0x38 - node +0 next, +0x18 the name's chars, +0x20 its length, +0x58 three
// float distances. Each game frame (the frame clock patch, framepace.c rt_pla_frame_dt_ns) the two nodes' distances
// are set to the game's times their setting, and given back at "Game's"; a node the game wrote again (a new field
// loaded) is the game's own again. Found with a hardware read watch on the factor table (every LOD pick reads it),
// its callers' registers pointing at the settings.
//
// Pokémon Violet 4.0.0 only (runtime/game_identity.h); the manager is checked by its vtable and the node by its name
// before anything is written. LARECOMP_NPC_DETAIL=<n> / LARECOMP_POKEMON_DETAIL=<n> override the settings (1: the
// game's).
#include "../rt.h"
#include "../settings.h"
#include <stdlib.h>
#include <string.h>

#define LOD_GLOBAL     0x46d4e00ull  // main + : -> -> the scene system; +0x38 -> +0x50: the model LOD manager
#define LOD_MGR_VTABLE 0x44a00b8ull  // main + : its vtable
#define LOD_MGR_BYNAME 0x30          // the settings by name (a map)
#define MAP_FIRST      0x38          // its first node
#define NODE_NAME      0x18          // (chars; +0x20 the length)
#define NODE_DIST      0x58          // float[3]

static bool gptr(uint64_t p) { return p >= 0x7000000000ull && p < 0x8000000000ull && !(p & 7); }
#define G64(a) (*(volatile uint64_t *)(uintptr_t)(a))

// One named setting's distances times mul: the game's kept (a node the game wrote again is its own again), logged.
typedef struct { const char *name, *what; uint64_t node; float game[3], wrote[3]; int logged; } LodScale;

static void scale(uint64_t map, LodScale *t, int mul) {
    size_t len = strlen(t->name);
    uint64_t n = G64(map + MAP_FIRST), found = 0;
    for (int i = 0; i < 64 && gptr(n); i++, n = G64(n)) {
        uint64_t s = G64(n + NODE_NAME);
        if (G64(n + NODE_NAME + 8) == len && s >= 0x7000000000ull && s < 0x7fffffff00ull &&
            !memcmp((const void *)(uintptr_t)s, t->name, len)) {
            found = n;
            break;
        }
    }
    if (!found) return;
    volatile float *d = (volatile float *)(uintptr_t)(found + NODE_DIST);
    if (found != t->node || d[0] != t->wrote[0] || d[1] != t->wrote[1] || d[2] != t->wrote[2]) {  // (the game's own)
        if (!(d[0] > 0 && d[0] < d[1] && d[1] < d[2] && d[2] < 10000)) { t->node = 0; return; }  // (not distances)
        t->node = found;
        for (int i = 0; i < 3; i++) t->game[i] = t->wrote[i] = d[i];
    }
    for (int i = 0; i < 3; i++) {
        float to = t->game[i] * (float)mul;
        if (d[i] != to) d[i] = t->wrote[i] = to;
    }
    if (t->logged != mul) {
        t->logged = mul;
        LOG("[npc] %s detail distances: %g / %g / %g m (the game's: %g / %g / %g)\n", t->what, t->game[0] * mul,
            t->game[1] * mul, t->game[2] * mul, t->game[0], t->game[1], t->game[2]);
    }
}

static int mul_of(const char *env_name, int *env, int setting) {
    if (*env == -2) { const char *e = getenv(env_name); *env = e ? atoi(e) : -1; }
    int mul = *env >= 0 ? *env : setting;
    return mul < 1 ? 1 : mul > 16 ? 16 : mul;
}

void rt_sv_npc_detail(void) {
    static int env_people = -2, env_pokemon = -2;
    static LodScale people = {"chara_model", "people's", 0, {0}, {0}, -1};
    static LodScale pokemon = {"pokemon", "Pokémon's", 0, {0}, {0}, -1};
    Module *m = &g_modules[0];
    if (!m->base || LOD_GLOBAL + 8 > m->data_off + m->data_size) return;
    uint64_t p = G64(m->base + LOD_GLOBAL);
    if (!gptr(p) || !gptr(p = G64(p)) || !gptr(p = G64(p + 0x38)) || !gptr(p = G64(p + 0x50))) return;
    if (G64(p) != m->base + LOD_MGR_VTABLE) return;
    uint64_t map = G64(p + LOD_MGR_BYNAME);
    if (!gptr(map)) return;
    scale(map, &people, mul_of("LARECOMP_NPC_DETAIL", &env_people, g_settings.npc_detail));
    scale(map, &pokemon, mul_of("LARECOMP_POKEMON_DETAIL", &env_pokemon, g_settings.pokemon_detail));
}
