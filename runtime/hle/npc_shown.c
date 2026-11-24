// Scarlet / Violet: how many town people are shown at once (F1 > People shown, settings npc_shown), and the GlobalHeap
// accounting that decides how far away they load.
//
// Towns place their people (FieldNpc: the ones standing in plazas and outside shops, always in the same spots) all
// around, but the game's character manager gives a model to the 12 nearest and keeps the rest hidden: walking into a
// plaza, its people appear a few at a time as they become some of the nearest 12, and in a crowd they swap in and out.
// That 12 is FieldNpc's maxGenerationNum in world/data/system/object_generation_range/object_generation_range_array.bin
// (a 64 there does the same as here: tested), copied at boot into the manager (vtable main+0x4468fd8, +0xe8, a u32);
// its update (f_70010358c8) reads it every frame. The game's own globals main+0x473af30 / +0x473af48 point at the
// manager. Each game frame (the frame clock patch, framepace.c rt_pla_frame_dt_ns) the limit is set to the setting,
// and given back when it goes back to the game's. Nothing of the game's files is changed or shipped.
//
// Not the generation range: maxCreateDistance there (50 m) is not what hides them, and above 50 m the game creates no
// town people at all (75 / 100 / 150 / 300 m: as few as at 2 m). Not the dither fade (model_dither_fade: 75-90 m).
//
// Pokémon Violet 4.0.0 only (runtime/game_identity.h: the one build PokeV-Recomp runs); the manager and the GlobalHeap
// are checked by their vtables before anything is written. LARECOMP_NPC_SHOWN=<n> overrides the setting (0: the game's).
#include "../rt.h"
#include "../settings.h"
#include <stdlib.h>

#define NPC_MGR_GLOBAL 0x473af30ull   // main + : the manager (or 0 before the field is up)
#define NPC_MGR_VTABLE 0x4468fd8ull   // main + : its vtable
#define NPC_MGR_LIMIT  0xe8           // u32: characters with a model at once
#define NPC_MGR_SLOTS  0xc58          // u64: quality slots (how many town people may load their parts from afar)
#define GLOBAL_HEAP    0x4763918ull   // main + : the GlobalHeap (a static object: +0x28 total, +0x38 used)
#define GLOBAL_HEAP_VT 0x44a6500ull   // main + : its vtable

static bool guest_ptr(uint64_t p) { return p >= 0x7000000000ull && p < 0x8000000000ull && !(p & 7); }

// The GlobalHeap's used size, kept in range. Every so often the game sets it to total - (its two StandardAllocators'
// free sizes) (f_7002b79f60 -> f_7002a647e4) with no clamp - its own Free clamps at 0 (f_7002a64bc4) - and here the
// second allocator outgrows the GlobalHeap (the kernel lets the game map memory past the application's: the
// hardware's limit, kernel.c LARECOMP_PHYS_LIMIT, left the player and the town unloaded), so a few cities in, used
// went to about -2 GB. The manager's quality slots are lowered with the GlobalHeap's headroom and never raised
// (f_70010358c8 0x70010366c8..0x7001037800): that one bogus value left 1 for good, and from then on the static town
// people loaded their parts only within 13 m instead of 47.5 (their components' load radius) - popping in right in
// front of the player and flickering, city after city. Each game frame a used past the total is set to 0 (more free
// than the total), and the slots that value took are given back; lowerings from real memory pressure stay. Tested
// on a 7-city route at 64 shown: it hit twice, the slots stayed at 10.
static void global_heap_in_range(uint64_t mgr) {
    Module *m = &g_modules[0];
    uint64_t gh = m->base + GLOBAL_HEAP;
    if (*(volatile uint64_t *)(uintptr_t)gh != m->base + GLOBAL_HEAP_VT) return;
    volatile uint64_t *used = (volatile uint64_t *)(uintptr_t)(gh + 0x38);
    uint64_t total = *(volatile uint64_t *)(uintptr_t)(gh + 0x28);
    volatile uint64_t *slots = (volatile uint64_t *)(uintptr_t)(mgr + NPC_MGR_SLOTS);
    static uint64_t good_slots;  // (the slots before a bogus value)
    static int nlog;
    if (*used <= total) {
        good_slots = *slots;
        return;
    }
    uint64_t back = good_slots > *slots ? good_slots : *slots;
    if (nlog++ < 8)
        LOG("[npc] GlobalHeap used %llx past its total %llx: set to 0, quality slots %llu -> %llu\n",
            (unsigned long long)*used, (unsigned long long)total, (unsigned long long)*slots, (unsigned long long)back);
    *used = 0;
    *slots = back;
}

void rt_sv_npc_shown(void) {
    static int env = -2;
    if (env == -2) { const char *e = getenv("LARECOMP_NPC_SHOWN"); env = e ? atoi(e) : -1; }
    int want = env >= 0 ? env : g_settings.npc_shown;
    static uint64_t mgr;    // the manager last seen
    static uint32_t game;   // its own limit (the table's)
    static uint32_t wrote;  // what was written last (anything else there is the game's own again)
    static int logged = -1;
    Module *m = &g_modules[0];
    if (!m->base || NPC_MGR_GLOBAL + 8 > m->data_off + m->data_size) return;
    uint64_t p = *(volatile uint64_t *)(uintptr_t)(m->base + NPC_MGR_GLOBAL);
    if (!guest_ptr(p) || *(volatile uint64_t *)(uintptr_t)p != m->base + NPC_MGR_VTABLE) {
        mgr = 0;
        return;
    }
    volatile uint32_t *lim = (volatile uint32_t *)(uintptr_t)(p + NPC_MGR_LIMIT);
    uint32_t cur = *lim;
    if (p != mgr || cur != wrote) {  // (a new manager, or the game set its limit again: that is the game's)
        if (cur == 0 || cur > 1024) { mgr = 0; return; }  // (not a limit: leave it alone)
        mgr = p;
        game = wrote = cur;
    }
    uint32_t to = want > (int)game ? (uint32_t)want : game;
    if (cur != to) *lim = wrote = to;
    if (logged != (int)to) {
        logged = (int)to;
        LOG("[npc] people shown at once: %u (the game's: %u)\n", to, game);
    }
    global_heap_in_range(p);
}
