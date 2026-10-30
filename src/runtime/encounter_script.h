// The encounter machine's script (docs/game_ram.md), as survival and random
// spawns read it. The task fn_0003951C runs a stream of 32-bit words at
// 0x1268B8 (the boot code copies it there from the ROM): each encounter starts
// with a header (op -10, its number) and is a row of items. An item is an
// operation (a negative word and the arguments op_args says) or a call
// (delay in frames, function, nargs, args). tools/encounters.py lists them.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace script {

constexpr uint32_t BASE = 0x1268B8;       // RAM: the machine reads BASE + 4 * pc
constexpr uint32_t ROM = 0xFFF468BC;      // where the boot code copies it from
constexpr uint32_t WORDS = 0x13DF;
constexpr uint32_t NOT_FOUND = ~0u;
constexpr uint32_t FN_ENCOUNTER_START = 0x00039428;   // zone trigger: (record) with record+0x14 = encounter
constexpr uint32_t FN_RAPTOR = 0x00053BB4;            // (slot 1..5, type, group of spawn positions)
constexpr uint32_t FN_PLACED = 0x00063ADC;            // (kind, x, z, heading): the stage 2 river
constexpr uint32_t FN_PLACED_XZ = 0x0007409C;         // (x, z): the same stretch

// Words that follow each negative opcode (-1: not one the machine knows)
int op_args(int32_t op);
// Calls that bring in enemies counted by the machine (seen in the trace)
bool is_enemy_fn(uint32_t fn);

// One operation or call and its words, from word pc
struct Item {
    uint32_t pc = 0;
    std::vector<int32_t> w;
    bool call() const { return w[0] >= 0; }
    int32_t op() const { return w[0]; }
    uint32_t fn() const { return call() ? uint32_t(w[1]) : 0; }
    bool enemy() const { return call() && is_enemy_fn(fn()); }
    bool end() const { return w[0] == -998 || w[0] == -999; }   // the encounter is over
};

// The script's words: from RAM or from the ROM
using Word = int32_t (*)(uint32_t pc);

// The item at word pc (false: an unknown operation, or a call longer than 64 words)
bool read(Word word, uint32_t pc, Item& it);
// Word pc of encounter id's header (NOT_FOUND: not there, or an unknown operation before it)
uint32_t find(Word word, int id);

// Gives f each item of the encounter whose header is at word h, the header
// first, up to its end operation (given too) or the next header. False: it
// stopped at an unknown item (its first word in *bad)
template <class F>
bool walk(Word word, uint32_t h, F&& f, int32_t* bad = nullptr) {
    Item it;
    for (uint32_t i = h; i < WORDS; i += uint32_t(it.w.size())) {
        if (!read(word, i, it)) {
            if (bad) *bad = word(i);
            return false;
        }
        if (it.op() == -10 && i != h) break;
        f(it);
        if (it.end()) break;
    }
    return true;
}

// The random numbers of both modes: 0..n-1 (0 when n is 0)
inline uint32_t rand_below(uint32_t& state, uint32_t n) {
    state = state * 1103515245u + 12345u;
    return n ? (state >> 16) % n : 0;
}
// The first state: from the clock, or LW_SEED=N (tests: the same game again)
inline uint32_t seed() {
    if (const char* s = getenv("LW_SEED")) return uint32_t(strtoul(s, nullptr, 0));
    return uint32_t(time(nullptr));
}

}  // namespace script
