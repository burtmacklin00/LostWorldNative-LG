#include "wide_aim.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "ppc_rt.h"

namespace wide_aim {
namespace {

constexpr uint32_t SETTINGS = 0x121C;
constexpr uint32_t CAL[2] = {SETTINGS + 0x20, SETTINGS + 0x26};   // centre x, centre y, right, left, top, bottom
constexpr int CENTRE = 128;
constexpr unsigned long long BOOT_FRAMES = 1500;   // game_ram.md: no settings writes during boot

std::atomic<bool> g_on{false};
std::atomic<int> g_d{0};                     // the widened calibration's half range (ADC steps)
std::atomic<int> g_factory[2][3] = {};       // centre, right, left as the game had them
std::atomic<bool> g_ready[2] = {};           // factory values known and the widened ones written

}  // namespace

void configure(bool on, float aspect) {
    // Window edge of the wide picture, in the game's units: the 4:3 screen is
    // 496 px wide and the calibrated edges (232 units) sit 228.3 px from its
    // centre (inputs.cpp's measured mapping), so 1 unit = 0.984 px. The ADC
    // must reach it with a few steps to spare (0..255 around 128: 125)
    float half_px = 384.0f * std::max(aspect, 4.0f / 3.0f) / 2.0f;
    float half_units = half_px / 0.984f;
    int d = int(std::floor(125.0f * 232.0f / half_units));
    g_d = std::clamp(d, 20, 100);
    g_on = on && aspect > 4.0f / 3.0f + 0.01f;
    if (g_on) printf("[gun] widescreen aim: the guns reach the side areas (ADC 128 +- %d = 4:3 edges)\n", int(g_d));
}

bool active() { return g_on; }

void frame(unsigned long long game_frame) {
    if (!g_on || game_frame < BOOT_FRAMES) return;
    int d = g_d;
    for (int p = 0; p < 2; p++) {
        uint32_t a = CAL[p];
        int c = rd8(a), r = rd8(a + 2), l = rd8(a + 3);
        bool ours = c == CENTRE && r == CENTRE + d && l == CENTRE - d;
        if (!ours) {
            // The game's own calibration (at boot, or reloaded from the EEPROM
            // after the test menu): kept to convert the gun positions
            if (!(l < c && c < r)) {   // not a usable calibration (blank EEPROM...)
                g_ready[p] = false;
                continue;
            }
            g_factory[p][0] = c, g_factory[p][1] = r, g_factory[p][2] = l;
            wr8(a, uint8_t(CENTRE));
            wr8(a + 2, uint8_t(CENTRE + d));
            wr8(a + 3, uint8_t(CENTRE - d));
            if (!g_ready[p]) printf("[gun] player %d: calibration x %d (%d..%d) widened to %d (%d..%d)\n", p + 1, c, l, r,
                                    CENTRE, CENTRE - d, CENTRE + d);
        }
        g_ready[p] = true;
    }
}

float remap_x(int player, float adc) {
    if (!g_on || !g_ready[player]) return adc;
    float c = float(g_factory[player][0]), r = float(g_factory[player][1]), l = float(g_factory[player][2]);
    float d = float(g_d);
    // The same position for the game: (adc - c) / (edge - c) is kept, on each side
    float g = adc >= c ? (adc - c) / (r - c) : (adc - c) / (c - l);
    return float(CENTRE) + g * d;
}

}  // namespace wide_aim
