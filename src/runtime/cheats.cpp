#include "cheats.h"

#include <atomic>
#include <cstdint>

#include "ppc_rt.h"

namespace {

// Game RAM, one int per player (+4 for player 2); the routines in
// runtime/outputs.cpp's notes. The standard gun (fn_0000BD88) sets the clip
// size to 5 and fills it; a shot (fn_0000E4D4) takes one bullet unless the
// gun in hand is a pickup (UNLIMITED not 0) and does not fire at 0; the
// reload (fn_0000C050) refills AMMO from CLIP
constexpr uint32_t AMMO = 0x1A3680;        // bullets left
constexpr uint32_t UNLIMITED = 0x1A3698;   // not 0: a pickup gun, shots don't use bullets
constexpr uint32_t CLIP = 0x1A36BC;        // bullets a reload gives
constexpr uint32_t SLOT_STATE = 0x1A3791;  // +0x10 for player 2: 1 / 2 playing

std::atomic<int> g_ammo{0};

}  // namespace

void cheats_set_ammo(int mode) { g_ammo = mode < 0 || mode > 2 ? 0 : mode; }

bool cheats_active() { return g_ammo != 0; }

void cheats_frame() {
    AmmoCheat mode = AmmoCheat(g_ammo.load());
    if (mode == AmmoCheat::Off) return;
    for (int p = 0; p < 2; p++) {
        uint8_t state = rd8(SLOT_STATE + 0x10 * p);
        if (state != 1 && state != 2) continue;          // not playing
        if (rd32(UNLIMITED + 4 * p) != 0) continue;      // a pickup gun: as the game has it
        int32_t clip = int32_t(rd32(CLIP + 4 * p)), ammo = int32_t(rd32(AMMO + 4 * p));
        if (clip <= 0 || clip > 99) continue;            // not set up (yet)
        // AUTO RELOAD refills as the last bullet comes up: at 0 the game has
        // already put up its RELOAD prompt, which then stays on for a moment
        bool refill = mode == AmmoCheat::Unlimited ? ammo < clip : ammo <= 1 && clip > 1;
        if (refill) wr32(AMMO + 4 * p, uint32_t(clip));
    }
}
