// Pokémon Scarlet / Violet frame clock (from PokeV-Recomp's framepace.c, adapted to the ZA Recomp base): the real
// frame time into the game's clock slots (tools/sv_patches.py).
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../hle.h"
#include "../settings.h"

extern bool g_za_frame_hook;

// Pokémon Legends: Arceus and Scarlet / Violet run in their own 30 fps mode, forced to 60 by tools/pla_patches.py /
// sv_patches.py, and step by the real frame time below 60 (rt_pla_frame_dt_ns; Scarlet / Violet's clock slots too,
// sv_patches.py _sv_frame_clock): 45-50 fps plays at normal speed, so no backing off to 30 here (v0.0.21 had Z-A's,
// fps60_holds, while the slots still stepped 1/60 s - Arceus never needed it).
bool rt_pla_is_60fps(void) {
    g_za_frame_hook = true;
    const char *e = ENV("LARECOMP_FPS");
    if (e) return atoi(e) >= 60;
    return g_settings.fps60;
}
// Scarlet / Violet: the real frame time also goes into the game's clock slots 0 and 1 (sv_patches.py _sv_frame_clock).
// LARECOMP_SV_SLOT_DT=0: not (the master clock only, as before v0.0.22 - slow motion below 60 fps).
bool rt_sv_slot_dt(void) {
    static int on = -1;
    if (on < 0) {
        const char *e = ENV("LARECOMP_SV_SLOT_DT");
        on = !e || atoi(e) != 0;
        LOG("[pace] Scarlet / Violet clock slots follow the real frame time: %s\n", on ? "on" : "off (LARECOMP_SV_SLOT_DT=0)");
    }
    return on;
}

static uint64_t g_frame_dt_ns;  // (rt_pla_frame_dt_ns: the frame time handed out this frame, smoothed; 0 before the first)
bool rt_sv_slot_dt(void);
// Pokémon Legends: Arceus & Pokémon Scarlet & Violet dynamic frame delta-time:
// Supplies the real wall-clock frame duration in nanoseconds (clamped to [16.67 ms, 50.0 ms] when 60 FPS is on),
// so the game runs at 1.0x normal speed at 60 FPS (no 2x speed-up) AND stays at 1.0x speed if FPS dips below 60.
uint64_t rt_pla_frame_dt_ns(uint32_t slot, uint64_t default_ns) {
    if (slot == 0) {  // (once a game frame, on the game's thread: Scarlet / Violet's npc_shown.c, npc_detail.c)
        extern void rt_sv_npc_shown(void);
        rt_sv_npc_shown();
        extern void rt_sv_npc_detail(void);
        rt_sv_npc_detail();
    }
    bool fps60 = rt_pla_is_60fps();
    uint64_t target_ns = fps60 ? 0x00fe502bull : (default_ns ? default_ns : 0x01fca056ull);
    static uint64_t last_ns;
    uint64_t cur_dt_ns = g_frame_dt_ns;
    static int logged_mode = -1;
    if (logged_mode != (int)fps60) {
        logged_mode = (int)fps60;
        LOG("[pace] game frame clock mode: %s (target dt = %.3f ms, dynamic delta-time %s)\n",
            fps60 ? "60 FPS" : "30 FPS", (double)target_ns / 1e6, fps60 ? "enabled" : "fixed");
    }
    if (!fps60) {
        last_ns = 0;
        g_frame_dt_ns = target_ns;
        return target_ns;
    }
    if (slot == 0 || !cur_dt_ns) {
        uint64_t now = plat_now_ns();
        uint64_t real_ns = last_ns ? (now - last_ns) : target_ns;
        last_ns = now;
        const char *dyn = ENV("LARECOMP_DYNAMIC_DT");
        if (dyn && atoi(dyn) == 0) {
            cur_dt_ns = target_ns;
        } else if (real_ns > 100000000ull) {
            // Ignore long loading / shader-compile stalls (> 100 ms) so the player doesn't jump ahead
            cur_dt_ns = target_ns;
        } else {
            if (real_ns < target_ns) real_ns = target_ns;
            if (real_ns > 50000000ull) real_ns = 50000000ull;  // floor at 20 FPS (50 ms) max step
            cur_dt_ns = cur_dt_ns ? (cur_dt_ns * 3 + real_ns) / 4 : real_ns;
        }
    }
    g_frame_dt_ns = cur_dt_ns;
    return cur_dt_ns;
}

// Scarlet / Violet's SetSlotMode (sv_patches.py _sv_slot_mode): the 60 fps mode's duration (16.67 ms, table entry 0)
// becomes the frame time measured this frame by the frame clock patch (rt_pla_frame_dt_ns) - every clock slot steps
// by real time; other modes' durations stay as the table has them. LARECOMP_SV_SLOT_DT=0: the table's value.
//
// The table's 30 fps entry (1) is also read by the game's step factor, slot 1 / (table[mode] - 1) (0x7000b83d1c,
// 0x7000d96428), with mode the game's own frame mode - still 1 (30 fps): _sv_frame_pace forces 60 fps in a register,
// not in the game's copies. The factor is how many 30 fps frames a frame lasted, and the UI's animations (and
// the Pokédex's book going onto the shelf on a new registration) advance by it. _sv_frame_pace sets entry 1 to
// 16.67 ms in the 60 fps mode, so the factor was 1 at 60 fps: every UI animation played at double speed (the
// map opened in 0.6 s instead of 1.2), and the book's insertion was cut short (its sequence counts in time, its
// motion in frames). So entry 1 goes back to 33.3 ms here, once that patch has written it this frame
// (SetSlotMode runs right after it, 0x7002b7a8f4): the factor is 0.5 at 60 fps, as the game's 30 fps timing
// expects. A slot set to mode 1 gets the frame time measured, as one set to mode 0 does (the same as when entry
// 1 was 16.67 ms). LARECOMP_SV_UI_STEP=0: entry 1 left at 16.67 ms (as before: UI animations at double speed).
static bool sv_ui_step(void) {
    static int on = -1;
    if (on < 0) {
        const char *e = ENV("LARECOMP_SV_UI_STEP");
        on = !e || atoi(e) != 0;
        LOG("[pace] Scarlet / Violet UI animations step by the 30 fps frame: %s\n", on ? "on" : "off (LARECOMP_SV_UI_STEP=0)");
    }
    return on;
}
uint64_t rt_sv_slot_duration(uint64_t table_ns) {
    if (!g_frame_dt_ns || !rt_pla_is_60fps()) return table_ns;
    bool ui = sv_ui_step();
    if (ui) *(volatile uint64_t *)(uintptr_t)0x7003c2fbb0ull = 0x01fca056ull;  // (the table's entry 1: 33.3 ms)
    if (!rt_sv_slot_dt()) return table_ns == 0x01fca056ull && ui ? 0x00fe502bull : table_ns;  // (nominal 1/60 s)
    if (table_ns != 0x00fe502bull && !(ui && table_ns == 0x01fca056ull)) return table_ns;
    return g_frame_dt_ns;
}


