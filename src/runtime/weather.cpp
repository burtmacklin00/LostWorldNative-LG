#include "weather.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>

namespace {

const char* const NAMES[WEATHER_COUNT] = {"SUNNY", "CLOUDY", "FOG", "RAIN", "STORM"};
const int WEIGHTS[WEATHER_COUNT] = {30, 25, 15, 18, 12};   // % of the rolls

std::atomic<int> g_rolled{WEATHER_NONE};
std::atomic<unsigned> g_rng{0};
std::atomic<bool> g_forced{false};

}  // namespace

const char* weather_name(int w) { return w >= 0 && w < WEATHER_COUNT ? NAMES[w] : nullptr; }

int weather_from_name(const char* s) {
    for (int w = 0; w < WEATHER_COUNT; w++) {
        const char *a = s, *b = NAMES[w];
        while (*a && *b && toupper((unsigned char)*a) == *b) a++, b++;
        if (!*a && !*b) return w;
    }
    return WEATHER_NONE;
}

void weather_roll() {
    unsigned r = g_rng.load();
    if (!r) r = unsigned(time(nullptr)) | 1;
    int last = g_rolled.load(), w = last;
    while (w == last) {
        r = r * 1103515245u + 12345u;
        int pick = int((r >> 16) % 100);
        for (w = 0; w < WEATHER_COUNT - 1 && pick >= WEIGHTS[w]; w++) pick -= WEIGHTS[w];
    }
    g_rng = r;
    g_rolled = w;
    if (!g_forced) printf("[weather] rolled %s\n", NAMES[w]);
}

int weather_rolled() { return g_rolled; }

void weather_force(int w) { g_forced = w != WEATHER_NONE; }
