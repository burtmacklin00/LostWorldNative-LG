// CLASSIC > RANDOM SPAWNS (see random_spawns.h). Addresses: docs/game_ram.md.
//
// The changes are made in place and keep every word count: nothing outside
// the encounter points into it, the boss mode's start points (bosses.h) stay
// where they are, and the machine runs the same operations in the same order
// apart from the reordered sections. Before each encounter the whole script
// is put back from the ROM, so only the encounter being played is ever
// changed, and with the mode off (or in the attract, survival or boss mode)
// it is the arcade's own.
#include "random_spawns.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <vector>

#include "ppc_rt.h"

extern int g_boss_maker;   // runtime.cpp: --make-boss-state plays by itself
bool survival_enabled();

namespace {

constexpr uint32_t SCRIPT = 0x1268B8;       // the encounter scripts (RAM, copied there at boot)
constexpr uint32_t SCRIPT_ROM = 0xFFF468BC; // where the boot code copies them from
constexpr uint32_t SCRIPT_WORDS = 0x13DF;
constexpr uint32_t FN_ENCOUNTER_START = 0x00039428;   // (record) with record+0x14 = encounter
constexpr uint32_t FN_ADD_SCORE = 0x0003DC60;   // (type, player from 1, points): every scoring hit
constexpr uint32_t FN_RAPTOR = 0x00053BB4;   // (slot 1..5, type, group of spawn positions)
constexpr uint32_t FN_PLACED = 0x00063ADC;   // (kind, x, z, heading): the stage 2 river
constexpr uint32_t FN_PLACED_XZ = 0x0007409C;   // (x, z): the same stretch
constexpr uint32_t COURSE_SELECT = 0x1C2AD8; // 0xC while a game is played
constexpr uint32_t ZONE = 0x1A3984;          // low nibble: current zone
constexpr uint32_t ZONE_INFO = 0x1A37D4;     // 12 bytes per zone: {trigger table index, x, z}
constexpr uint32_t TRIGGER_TABLES = 0x120A1C;   // 28 bytes each, +8: the zone's records
constexpr float NEAR_LOOK = 120;             // raptor groups swap only between camera turns this close
constexpr int TYPE_CHANCE = 30;              // % of the raptors that take another type of the zone's
constexpr int LIFE_MIN = 70, LIFE_MAX = 150; // a boss's life, % of the arcade's

// The bosses' life: a word in the boss object that each hit lowers (by 1, or 2
// with flag +0xC8 clear; the Deinosuchus's weak spot by 14) and the boss's
// code sets when the fight or one of its phases starts (the T-Rex female gets
// 150 or 250 in fn_000CC48C). Seen from the score call of each hit: the return
// address says which boss, and the caller's frame (r31 at the call) holds the object
struct BossHit {
    uint32_t lr;     // return address of the fn_0003DC60 call
    uint32_t slot;   // the object pointer, at this offset of the caller's frame
    uint32_t life;   // the life word in the object
};
constexpr BossHit BOSS_HITS[] = {
    {0x000CCB74, 0x08, 0x98},   // T-Rex female (stage 1)
    {0x0008E86C, 0x10, 0x4C}, {0x0008E888, 0x10, 0x4C},   // Deinosuchus, body (10 / 200 points)
    {0x0008F14C, 0x0C, 0x4C},   // Deinosuchus, its weak spot
    {0x000E86EC, 0x18, 0x98},   // Carnotaurus (stage 4)
    {0x000FE9D4, 0x08, 0x98},   // T-Rex (stage 5)
};
// A boss being fought: its object, its factor and the life it was left with
struct Boss {
    uint32_t obj = 0;
    int pct = 100;
    int32_t last = 0;
};
Boss g_boss;

std::atomic<bool> g_on{false};
uint32_t g_rng = 1;
bool g_patched = true;    // the script in RAM may not be the ROM's (a game state loaded)

uint32_t rnd(uint32_t n) {
    g_rng = g_rng * 1103515245u + 12345u;
    return n ? (g_rng >> 16) % n : 0;
}
int32_t rnd_between(int32_t lo, int32_t hi) { return lo + int32_t(rnd(uint32_t(hi - lo + 1))); }

int32_t rom_word(uint32_t pc) { return int32_t(rd32(SCRIPT_ROM + 4 * pc)); }

// Words that follow each negative opcode (as in survival.cpp, tools/encounters.py)
int op_args(int32_t op) {
    switch (op) {
    case -1: case -12: case -13: case -14: case -15: case -22: case -29: case -30: case -31: case -32: case -33:
    case -998: case -999: return 0;
    case -5: case -8: case -10: case -19: case -20: case -21: case -23: case -27: case -28: return 1;
    case -17: case -24: case -26: return 2;
    case -9: case -25: return 3;
    case -11: return 4;
    default: return -1;
    }
}

bool is_enemy_fn(uint32_t fn) {
    switch (fn) {
    case 0x53BB4: case 0x61AF8: case 0x627E8: case 0x46B50: case 0x536CC: case 0x63ADC: case 0x7409C:
    case 0x71914: return true;
    default: return false;
    }
}

// One operation or call of an encounter, with its words
struct Tok {
    uint32_t pc;
    std::vector<int32_t> w;
    bool call() const { return w[0] >= 0; }
    int32_t op() const { return w[0]; }
    uint32_t fn() const { return call() ? uint32_t(w[1]) : 0; }
    bool raptor() const { return fn() == FN_RAPTOR && w.size() >= 6 && w[3] >= 1 && w[3] <= 5; }
};

// Encounter `id` as the ROM has it, from its header to its end (included); empty if not found
std::vector<Tok> parse(int id) {
    uint32_t i = 0;
    while (i < SCRIPT_WORDS) {
        int32_t op = rom_word(i);
        if (op == -10 && rom_word(i + 1) == id) break;
        int n = op < 0 ? op_args(op) : -2;
        if (op < 0 && n < 0) return {};
        i += op < 0 ? 1 + uint32_t(n) : 3 + uint32_t(rom_word(i + 2));
    }
    std::vector<Tok> out;
    for (uint32_t h = i; i < SCRIPT_WORDS;) {
        int32_t op = rom_word(i);
        if (op == -10 && i != h) break;
        uint32_t len = op < 0 ? 1 + uint32_t(std::max(0, op_args(op))) : 3 + uint32_t(rom_word(i + 2));
        if ((op < 0 && op_args(op) < 0) || len > 64) return {};
        Tok t{i, {}};
        for (uint32_t k = 0; k < len; k++) t.w.push_back(rom_word(i + k));
        out.push_back(t);
        i += len;
        if (op == -999 || op == -998) break;
    }
    return out;
}

// The encounters started by the current zone's triggers (its stops)
std::vector<int> zone_encounters() {
    std::vector<int> ids;
    uint32_t index = rd32(ZONE_INFO + 12 * (rd32(ZONE) & 15));
    if (index > 64) return ids;
    uint32_t rec = rd32(TRIGGER_TABLES + 28 * index + 8);
    for (int k = 0; rec && rec < RAM_SIZE && k < 128; k++, rec += 24) {
        if (int32_t(rd32(rec)) == -1) break;
        if (rd32(rec) == FN_ENCOUNTER_START) ids.push_back(int(rd32(rec + 16)));
    }
    return ids;
}

// A raptor the zone's scripts bring in: where the camera looks then, its type and group
struct Spawn {
    float x, z;
    int32_t type, group;
};

void gather(const std::vector<Tok>& toks, std::vector<Spawn>& out) {
    bool look = false;
    float x = 0, z = 0;
    for (const Tok& t : toks) {
        if (t.op() == -9) look = true, x = float(t.w[1]), z = float(t.w[3]);
        if (look && t.raptor()) out.push_back({x, z, t.w[4], t.w[5]});
    }
}

// A plain section: camera turn, maybe its setup and waits, enemy calls, wait for them all
bool plain_section(const std::vector<Tok>& toks, size_t a, size_t& end) {
    if (a >= toks.size() || toks[a].op() != -9) return false;
    int enemies = 0;
    for (size_t i = a + 1; i < toks.size(); i++) {
        const Tok& t = toks[i];
        if (t.op() == -1) {
            end = i + 1;
            return enemies > 0;
        }
        if (t.call() && is_enemy_fn(t.fn())) enemies++;
        else if (!(t.op() == -25 || t.op() == -26 || t.op() == -21)) return false;
    }
    return false;
}

struct Changes {
    int moved = 0, types = 0, placed = 0, timed = 0, reordered = 0;
};

// Runs of two or more plain sections in a row, followed by another camera turn
// or the end: their order is shuffled
void reorder(std::vector<Tok>& toks, Changes& ch) {
    for (size_t a = 0; a < toks.size();) {
        std::vector<std::pair<size_t, size_t>> run;
        size_t b = a, e;
        while (plain_section(toks, b, e)) run.push_back({b, e}), b = e;
        if (run.size() < 2) {
            a++;
            continue;
        }
        int32_t next = b < toks.size() ? toks[b].op() : -999;
        if (next == -9 || next == -999 || next == -998) {
            std::vector<std::vector<Tok>> parts;
            for (auto& [s, f] : run) parts.emplace_back(toks.begin() + long(s), toks.begin() + long(f));
            for (size_t i = parts.size() - 1; i > 0; i--) std::swap(parts[i], parts[rnd(uint32_t(i + 1))]);
            size_t k = a;
            uint32_t pc = toks[a].pc;   // the same words in all, laid out again from the first
            for (auto& p : parts)
                for (Tok& t : p) {
                    t.pc = pc, pc += uint32_t(t.w.size());
                    toks[k++] = t;
                }
            ch.reordered += int(run.size());
        }
        a = b;
    }
}

void shuffle(int id) {
    std::vector<Tok> toks = parse(id);
    if (toks.empty()) return;
    std::vector<Spawn> spawns;
    std::set<int32_t> types;
    std::vector<int> ids = zone_encounters();
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    for (int other : ids) gather(other == id ? toks : parse(other), spawns);
    for (const Spawn& s : spawns) types.insert(s.type);
    Changes ch;
    bool look = false;
    float x = 0, z = 0;
    for (Tok& t : toks) {
        if (t.op() == -9) look = true, x = float(t.w[1]), z = float(t.w[3]);
        if (!t.call() || !is_enemy_fn(t.fn())) continue;
        if (t.raptor() && look) {
            std::vector<int32_t> groups;
            for (const Spawn& s : spawns)
                if (std::hypot(s.x - x, s.z - z) < NEAR_LOOK) groups.push_back(s.group);
            int32_t g = groups.empty() ? t.w[5] : groups[rnd(uint32_t(groups.size()))];
            if (g != t.w[5]) t.w[5] = g, ch.moved++;
            if (types.size() > 1 && types.count(t.w[4]) && rnd(100) < uint32_t(TYPE_CHANCE)) {
                auto it = types.begin();
                std::advance(it, rnd(uint32_t(types.size())));
                if (*it != t.w[4]) t.w[4] = *it, ch.types++;
            }
        }
        if (t.fn() == FN_PLACED && t.w.size() >= 7) {
            t.w[4] += rnd_between(-30, 30);
            t.w[5] += rnd_between(-20, 20);
            t.w[6] = (t.w[6] + rnd_between(-1024, 1024)) & 0xFFFF;
            ch.placed++;
        }
        if (t.fn() == FN_PLACED_XZ && t.w.size() >= 5) {
            t.w[3] += rnd_between(-25, 25);
            t.w[4] += rnd_between(-15, 15);
            ch.placed++;
        }
        // When it comes: its delay 70..130 %, and now and then a little later when it had none
        int32_t d = t.w[0];
        t.w[0] = d > 0 ? std::max(1, d * rnd_between(70, 130) / 100) : rnd(3) == 0 ? rnd_between(0, 20) : 0;
        ch.timed += t.w[0] != d;
    }
    reorder(toks, ch);
    for (const Tok& t : toks)
        for (size_t k = 0; k < t.w.size(); k++) wr32(SCRIPT + 4 * (t.pc + uint32_t(k)), uint32_t(t.w[k]));
    g_patched = true;
    printf("[random] encounter %d: %d raptors moved, %d other types, %d placed, %d retimed, %d sections reordered\n",
           id, ch.moved, ch.types, ch.placed, ch.timed, ch.reordered);
}

void restore() {
    for (uint32_t i = 0; i < SCRIPT_WORDS; i++) wr32(SCRIPT + 4 * i, rd32(SCRIPT_ROM + 4 * i));
    g_patched = false;
}

bool rom_ok() {   // the script is where the boot code copies it from: encounter 2 first
    static int ok = -1;
    if (ok < 0) {
        ok = rom_word(0) == -10 && rom_word(1) == 2;
        if (!ok) printf("[random] the encounter scripts are not where they should be in the ROM: mode off\n");
    }
    return ok > 0;
}

// Research aid: LW_HIT_PROBE=FILE writes RAM 0x100000..0x200000 to FILE the
// frame after every scoring hit and every 120 frames otherwise, each block
// headed by {frame, hits so far} (to find where the bosses keep their life)
FILE* g_probe = nullptr;
uint32_t g_probe_hits = 0, g_probe_dumped = ~0u, g_probe_count = 0;
uint64_t g_probe_frame = 0;
constexpr uint32_t PROBE_FROM = 0x100000, PROBE_SIZE = 0x100000, PROBE_MAX = 600;

}  // namespace

bool random_spawns_option(const std::string& name) {
    if (name != "--random-spawns") return false;
    random_spawns_set(true);
    return true;
}

void random_spawns_hooks() {
    for (uint32_t fn : {FN_ENCOUNTER_START, FN_ADD_SCORE}) g_trace_bits[(fn >> 5) & 511] |= uint8_t(1 << ((fn >> 2) & 7));
    if (const char* p = getenv("LW_HIT_PROBE")) g_probe = fopen(p, "wb");
}

void random_spawns_frame() {
    g_probe_frame++;
    if (!g_probe || g_probe_count >= PROBE_MAX) return;
    if (g_probe_hits == g_probe_dumped && g_probe_frame % 120) return;
    uint32_t head[2] = {uint32_t(g_probe_frame), g_probe_hits};
    fwrite(head, 4, 2, g_probe);
    fwrite(g_ram + PROBE_FROM, 1, PROBE_SIZE, g_probe);
    fflush(g_probe);
    g_probe_dumped = g_probe_hits;
    g_probe_count++;
}

void random_spawns_set(bool on) {
    if (on && !g_on) g_rng = uint32_t(time(nullptr)) | 1;
    g_on = on;
}

bool random_spawns_on() { return g_on; }

void random_spawns_enter(Ctx& c, uint32_t fn) {
    if (fn == FN_ADD_SCORE && g_probe) {
        g_probe_hits++;
        printf("[probe] frame %llu hit %u: type %u player %u points %u (lr %08X)\n", (unsigned long long)g_probe_frame,
               g_probe_hits, c.r[3], c.r[4], c.r[5], c.lr);
    }
    bool shuffled = g_on && rd32(COURSE_SELECT) == 0xC && !survival_enabled() && g_boss_maker < 0;
    if (fn == FN_ADD_SCORE && shuffled)
        for (const BossHit& b : BOSS_HITS) {
            if (c.lr != b.lr) continue;
            uint32_t obj = rd32(c.r[31] + b.slot);
            if (obj >= RAM_SIZE - 0x100) break;
            int32_t life = int32_t(rd32(obj + b.life));
            if (obj != g_boss.obj) {
                g_boss = {obj, LIFE_MIN + int(rnd(LIFE_MAX - LIFE_MIN + 1)), -1};
                if (const char* e = getenv("LW_BOSS_LIFE")) g_boss.pct = std::max(1, atoi(e));   // testing aid: this %
            }
            // First hit, or the life was set again since the last one (a new phase): scaled
            if (life > g_boss.last && life > 10) {
                int32_t scaled = std::max(1, life * g_boss.pct / 100);
                wr32(obj + b.life, uint32_t(scaled));
                printf("[random] boss life %d -> %d (%d %%)\n", life, scaled, g_boss.pct);
                life = scaled;
            }
            g_boss.last = life;
            break;
        }
    if (fn != FN_ENCOUNTER_START || !rom_ok()) return;
    if (g_patched || shuffled) restore();
    if (shuffled) shuffle(int(rd32(c.r[3] + 0x14)));
}
