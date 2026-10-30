// The first state of the modes' random numbers (survival, random spawns,
// random weather): from the clock, or LW_SEED=N (tests: the same game again)
#pragma once
#include <cstdint>
#include <cstdlib>
#include <ctime>

inline uint32_t random_seed() {
    if (const char* s = getenv("LW_SEED")) return uint32_t(strtoul(s, nullptr, 0));
    return uint32_t(time(nullptr));
}
