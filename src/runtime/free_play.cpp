#include "free_play.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "ppc_rt.h"

namespace free_play {
namespace {

constexpr uint32_t COIN_SETTING = 0x1225;    // settings (0x121C + 9): 0x1A = FREE PLAY
constexpr uint8_t COIN_FREE_PLAY = 0x1A;
constexpr uint32_t FREE_FLAG = 0x12F4;       // system: 0xFF free play, 0 coins
constexpr uint32_t GAME_FLAG = 0x1A61F0;     // the game's copy (int)

std::atomic<bool> g_on{true};
std::atomic<bool> g_native{false};
bool g_written = false;   // game thread only

}  // namespace

void set(bool on) { g_on = on; }

bool native() { return g_native; }

void frame(bool boot_over) {
    if (!boot_over) return;   // game_ram.md: no writes there during boot
    if (g_on) {
        wr8(FREE_FLAG, 0xFF);
        wr32(GAME_FLAG, 1);
        if (!g_written) printf("[coin] free play: the game's own FREE PLAY mode\n");
        g_written = true;
        g_native = true;
    } else if (g_written) {
        // Back to what the arcade's coin setting says
        bool setting = rd8(COIN_SETTING) == COIN_FREE_PLAY;
        wr8(FREE_FLAG, setting ? 0xFF : 0);
        wr32(GAME_FLAG, setting ? 1 : 0);
        g_written = false;
        g_native = setting;
        printf("[coin] free play off: coins needed\n");
    } else {
        g_native = rd8(FREE_FLAG) != 0;
    }
}

}  // namespace free_play
