// Survival mode (wave arena) and its investigation trace.
//
// --survival STAGE:ZONE takes the player round that zone, stop after stop,
// in laps. The game's own encounter machine does the work: an encounter is a
// script of words at 0x1268B8 run by the task fn_0003951C, and the camera only
// walks on to the next stop when that task ends. The first encounter is copied
// to free RAM with its end replaced by an endless wait, so the task never
// ends; each time it reaches that wait the stop is cleared. Then the game's
// own walker is let go (0x1A3960, as the end of an encounter does): it carries
// the camera along the stage's path, turning and climbing as in the arcade,
// and the next stop trigger stops it. The encounter of that stop is skipped
// (the task is still the first one) and a wave is written from its enemy
// calls and camera turns instead, more of them every wave. Sometimes the walk
// is cut short by an ambush, and sometimes a golden BONUS target shows up.
// After the zone's last stop the game state kept at the first one is put
// back (game_state_mark) under a fade to black: a new, harder lap.
//
// Getting there is automatic: from the attract (the window puts the game back
// there first, see app.cpp) the coin, start and course select are pressed
// while the game runs at full speed and muted behind the loading screen, and
// the course is made to start at the chosen stage. The game then waits on the
// start screen. The last hit never takes the player's last life (the hit functions
// get one more first), so the game never reaches its CONTINUE screen: that hit
// ends the hunt with the results screen; playing again starts over there.
//
// --trace-survival writes out/survival_trace.txt: per frame the stage, zone,
// camera, tasks and the encounter machine, plus every task the game creates and
// every call of the zone trigger task. The addresses are in docs/game_ram.md.
#include "survival.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "bosses.h"
#include "inputs.h"
#include "ppc_rt.h"

namespace {

// Game RAM (docs/game_ram.md)
constexpr uint32_t STAGE = 0x1A623C;        // byte: current stage (0 = stage 1)
constexpr uint32_t ZONE = 0x1A3984;         // low nibble: current zone of the stage
constexpr uint32_t CAMERA_POS = 0x1A6F28;   // floats x, y, z: camera position
constexpr uint32_t TASK_HEAD = 0x1C10B0;    // first task; node +8 next, +0xC function
constexpr uint32_t FN_TASK_NEW = 0x00116A48;   // (size, fn) -> task
constexpr uint32_t PLAYER_LIFE = 0x1A3720;  // player 1 object (0x1A36C8) + 0x58: lives left
constexpr uint32_t FN_PLAYER_HIT[2] = {0x000463D4, 0x00071460};   // hurt the player: lives - 1
// Player 1's score, the score its counter shows (fn_0003E800 draws the digits
// again when the two differ; the game writes -1 there to force it), then the score of each
constexpr uint32_t SCORE[7] = {0x1A857C, 0x1C1D54,
                               0x1A858C, 0x1A8590, 0x1A8594, 0x1A8598, 0x1A859C};   // stage (0x1A858C + 4 * stage)
constexpr uint32_t SETTING_LIFE = 0x122A;   // byte: LIFE setting (RAM copy of the EEPROM)
constexpr uint32_t SETTING_DIFF = 0x122D;   // high nibble: difficulty (RAM copy, read live)
constexpr uint32_t PLAYLIST = 0x1C2AFC;    // 5 ints: stages of the course, in order
constexpr uint32_t PLAYLIST_INDEX = 0x1C2B10;
constexpr uint32_t FN_NEXT_STAGE = 0x00081364;   // stage byte = PLAYLIST[++PLAYLIST_INDEX]
constexpr uint32_t FN_PLAYLIST_INIT = 0x000812DC;   // PLAYLIST = 0..4, PLAYLIST_INDEX = -1 (new game)
// Encounter machine
constexpr uint32_t FN_ENCOUNTER_START = 0x00039428;   // zone trigger: (record) with record+0x14 = encounter
constexpr uint32_t FN_ENCOUNTER = 0x0003951C;         // the task that runs the script
constexpr uint32_t FN_STOP = 0x0003B3F4;              // zone trigger: the camera stops (CAM_WALK = 0)
constexpr uint32_t TRIGGER_TASK = 0x1C12A0;           // the zone trigger task (fn_00014764); +0: its first node
constexpr uint32_t SCRIPT = 0x1268B8;       // words; the machine reads SCRIPT + 4 * pc
constexpr uint32_t SCRIPT_WORDS = 0x13DF;
constexpr uint32_t VM_PC = 0x1A847C;
constexpr uint32_t VM_ALIVE = 0x1A8468;     // enemies alive (what op -1 waits for)
constexpr uint32_t VM_ALIVE2 = 0x1A83D4;    // the count op -1 waits for while flag 0x200 is set
constexpr uint32_t VM_FLAGS = 0x1A8414;
constexpr uint32_t VM_WAIT = 0x1A842C;      // frames waited by the current op
constexpr uint32_t VM_COUNTDOWN = 0x1C1D0C; // op -21: frames left (loaded from its argument when 0)
constexpr uint32_t LOOK_AT = 0x1A3454;      // floats: point the camera turns to (op -9)
// Camera walk along the stage path
constexpr uint32_t CAM_WALK = 0x1A3960;     // 1: walk on (set when an encounter ends), 0: stop (trigger 0x3B3F4)
constexpr uint32_t CAM_NODE = 0x1A3A0C;     // node of the path the camera heads for
constexpr uint32_t CAM_SPEED = 0x1A39C8;    // float: speeds up and slows down by itself
// Gait (fn_0001A4DC writes them; the stop trigger fn_0001A5B4 too): target speed
// 0..0xFF, 0 walking / 0xFF stopped, and how fast it gets there
constexpr uint32_t CAM_GAIT_SPEED = 0x1A3994, CAM_GAIT_HALT = 0x1A399C, CAM_GAIT_RAMP = 0x1A3990;
// Free RAM (the game uses up to 0x200000) for the survival scripts
constexpr uint32_t FREE_RAM = 0x700000;
constexpr int32_t WAIT_FOREVER = 0x3FFFFFFF;
constexpr uint32_t QUIET_SCRIPT = FREE_RAM + 0x40000;   // an encounter on the way in, without its enemies
constexpr uint64_t CLEAR_FRAMES = 230;      // a stop cleared: the walk on starts after this (4 s), less every lap
constexpr uint64_t WALK_LONGEST = 3600;     // a walk that never reaches a stop (1 min): the lap ends there
constexpr uint64_t FADE_FRAMES = 40;        // the fade to black at the end of a lap, and back
constexpr int32_t COUNTDOWN_FRAMES = int32_t(SURVIVAL_COUNTDOWN * 57.5);   // the countdown, as a -21 wait
constexpr int WAVE_MIN = 8, WAVE_MAX = 30;  // dinosaurs in a wave: at least as many as the arena's first (the game's own)
constexpr uint32_t FN_RAPTOR = 0x00053BB4;  // (slot 1..5, type, group of spawn positions)
constexpr float NEAR_LOOK = 120;            // another stop's section joins a wave if it looks this near one of this stop's
constexpr int AMBUSH_CHANCE = 35;           // % of the walks with an ambush on the way
constexpr int BONUS_CHANCE = 50;            // % of the walks with a golden BONUS target
constexpr uint64_t BONUS_FRAMES = 200;      // how long it stays up (3.5 s)
constexpr float BONUS_HIT = 30;             // how near a shot must land (game screen pixels)
constexpr int MAX_LIVES = 9;                // the hearts count shows one digit: past it, points
constexpr int GOLD_POINTS = 5000;
constexpr uint64_t DYING_FRAMES = 80;       // the killing attack and its flash play out (1.4 s), controls locked
constexpr uint32_t COURSE_SELECT = 0x1C2AD8;   // 0 attract, 1-2 choosing a course, 0xC playing
constexpr uint64_t LOADING_GIVE_UP = 30000;    // game frames (at full speed, ~40 s; the slowest arena takes ~9000)
constexpr const char* RECORDS = "save/survival.ini";

bool g_trace = false;
FILE* g_log = nullptr;
std::set<uint32_t> g_hooks;
uint64_t g_frame_no = 0;

float rdf(uint32_t a) { return float(f2d(rd32(a))); }

double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void hook(uint32_t fn) {
    g_hooks.insert(fn);
    g_trace_bits[(fn >> 5) & 511] |= uint8_t(1 << ((fn >> 2) & 7));
}

void log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
}

// Live tasks: node -> function
std::map<uint32_t, uint32_t> tasks() {
    std::map<uint32_t, uint32_t> m;
    for (uint32_t t = rd32(TASK_HEAD); t && t < RAM_SIZE && m.size() < 4096; t = rd32(t + 8)) m[t] = rd32(t + 0xC);
    return m;
}

// Words that follow each negative opcode of the encounter machine (tools/encounters.py)
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

// Calls that bring in enemies counted by the machine (seen in the trace)
bool is_enemy_fn(uint32_t fn) {
    switch (fn) {
    case 0x53BB4: case 0x61AF8: case 0x627E8: case 0x46B50: case 0x536CC: case 0x63ADC: case 0x7409C:
    case 0x71914: return true;
    default: return false;
    }
}

}  // namespace

// Arenas: the first encounter with known enemies of each zone (found with
// LW_SURV_AUTOPLAY and the trace, docs/game_ram.md). Stages 2 and 5 do not use
// the encounter machine; stage 1 zone 7 is the T-Rex chase, where the game
// loops the camera and loads the zone again and again
const SurvivalZone SURVIVAL_ZONES[] = {
    {1, 1, L"Jungle shack", false},
    {1, 5, L"Dark forest", false},
    {3, 0, L"Visitor center hall", false},
    {3, 1, L"Service corridor", false},
    {4, 1, L"Control room", true},
};
const int SURVIVAL_ZONE_COUNT = int(sizeof(SURVIVAL_ZONES) / sizeof(SURVIVAL_ZONES[0]));

const wchar_t* survival_zone_name(int stage, int zone) {
    for (const SurvivalZone& z : SURVIVAL_ZONES)
        if (z.stage == stage && z.zone == zone) return z.name;
    return nullptr;
}

namespace {

uint32_t script_addr(uint32_t pc) { return SCRIPT + 4 * pc; }
uint32_t script_pc(uint32_t addr) { return (addr - SCRIPT) / 4; }

// Calls of known enemies in encounter `id` (for the trace: which zones can be arenas)
int encounter_enemies(int id) {
    uint32_t i = 0;
    bool in = false;
    int n = 0;
    while (i < SCRIPT_WORDS) {
        int32_t op = int32_t(rd32(script_addr(i)));
        if (op == -10) {
            if (in) break;
            in = int32_t(rd32(script_addr(i + 1))) == id;
        }
        if (op < 0) {
            int k = op_args(op);
            if (k < 0) return -1;
            i += 1 + uint32_t(k);
        } else {
            if (in && is_enemy_fn(rd32(script_addr(i + 1)))) n++;
            i += 3 + rd32(script_addr(i + 2));
        }
    }
    return n;
}

// Copies the encounter whose header is at pc h to QUIET_SCRIPT with every
// call that brings in a counted enemy turned into waits for no enemy (-1,
// one per word: the copy keeps its layout), so it runs as the game plays it,
// camera turns, cutscenes and all, with nobody to shoot. False: not understood
bool quiet_copy(uint32_t h) {
    std::vector<int32_t> out;
    int removed = 0;
    for (uint32_t i = h; i < SCRIPT_WORDS;) {
        int32_t op = int32_t(rd32(script_addr(i)));
        if (op < 0) {
            int k = op_args(op);
            if (k < 0) return false;
            for (int j = 0; j <= k; j++) out.push_back(int32_t(rd32(script_addr(i + uint32_t(j)))));
            if ((op == -10 && i != h) || op == -998 || op == -999) break;
            i += 1 + uint32_t(k);
        } else {
            uint32_t n = 3 + rd32(script_addr(i + 2));
            uint32_t fn = rd32(script_addr(i + 1));
            bool enemy = is_enemy_fn(fn) || fn == 0x6053C || fn == 0x5F8FC || fn == 0x4BF4C;
            for (uint32_t j = 0; j < n; j++) out.push_back(enemy ? -1 : int32_t(rd32(script_addr(i + j))));
            removed += enemy;
            i += n;
        }
        if (out.size() > 0x3000) return false;
    }
    for (size_t j = 0; j < out.size(); j++) wr32(QUIET_SCRIPT + 4 * uint32_t(j), uint32_t(out[j]));
    printf("[survival] on the way: encounter %d without its %d enemies\n", out.size() > 1 ? out[1] : -1, removed);
    return true;
}

// Best wave per stage and zone, with its score and lap (save/survival.ini:
// "stage:zone wave score lap"; older files have no lap)
struct Best {
    int wave = 0, score = 0, lap = 0;
};
struct Records {
    std::map<std::pair<int, int>, Best> best;
    void load() {
        if (FILE* f = fopen(RECORDS, "r")) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                int s, z;
                Best b;
                if (sscanf(line, "%d:%d %d %d %d", &s, &z, &b.wave, &b.score, &b.lap) >= 4) best[{s, z}] = b;
            }
            fclose(f);
        }
    }
    void save() const {
        if (FILE* f = fopen(RECORDS, "w")) {
            for (auto& [k, v] : best) fprintf(f, "%d:%d %d %d %d\n", k.first, k.second, v.wave, v.score, v.lap);
            fclose(f);
        }
    }
};

struct Survival {
    bool on = false;
    bool gate = false;            // the window is still putting the game back at the attract
    uint64_t loading_frames = 0;
    SurvivalStep step = SurvivalStep::Preparing;
    int stage = 0, zone = 0;      // 0-based stage byte, zone nibble
    enum { WAITING, STARTING, HOLDING } state = WAITING;
    bool zone_empty = false;      // its encounters had no enemies (stage 1 zone 0, the walk in): the next zone's will do
    SurvivalMode mode = SurvivalMode::Off;
    int encounter = -1;
    std::vector<std::vector<int32_t>> enemies;   // {delay, fn, nargs, args...} of the encounter
    std::vector<std::vector<int32_t>> looks;     // {-9, x, y, z}
    std::vector<int32_t> setup;                  // first -25 / -26 of the encounter
    uint32_t end_pc = 0;          // the endless wait the machine reaches after a wave
    std::vector<uint32_t> enemy_pcs;   // where the script being run calls an enemy
    int wave = 0;                 // 0: the encounter itself
    bool armed = false;           // a new course: its first stage becomes the chosen one
    int base_diff = 7;
    int lives = 0, start_lives = 0;
    // The lap: fight at a stop, a pause once it is cleared, the walk on to the
    // next one (maybe an ambush on the way), and after the last stop the fade
    // back to the first
    enum Phase { FIGHT, CLEARED, WALK, AMBUSH, LAP_OUT, LAP_LOAD } phase = FIGHT;
    uint64_t phase_at = 0;        // frame the phase began
    int lap = 1;
    int stop = 0;                 // stops reached in this lap (0: the first)
    bool last_stop = false;       // no stop of the zone left after this one
    int pending = -1;             // encounter whose trigger the walk went past (the next stop's)
    bool stop_passed = false;     // the next stop's trigger went off (during an ambush the walker was stopped already)
    bool ambush_planned = false;  // this walk: an ambush halfway to the next stop
    float walk_length = 0;        // how far the next stop was (straight) when the walk began
    uint64_t bonus_at = 0;        // this walk: when the BONUS target shows up (0: none)
    int first_encounter = -1;     // the arena's first stop, where each lap starts again
    size_t first_count = 0;       // dinosaurs of its encounter (the first wave, the game's own)
    std::vector<int> zone_encounters;   // the encounters with enemies of the zone (its stops)
    std::vector<int32_t> raptor_types;  // the raptor types they use
    uint32_t last_sig = 0;        // the wave before (the same one never comes twice in a row)
    std::vector<std::vector<int32_t>> first_enemies, first_looks;
    std::vector<int32_t> first_setup;
    int saved_lives = 0, saved_score = 0;   // kept over the state put back at a new lap
    uint64_t fade_in_at = 0;      // the picture comes back from black (after a new lap)
    double lap_started = 0;       // banner
    double ambush_started = 0;
    // The golden BONUS target (fractions of the 4:3 screen)
    bool bonus_on = false;
    uint64_t bonus_until = 0;
    float bonus_bx = 0, bonus_by = 0, bonus_x = 0, bonus_y = 0;
    bool trigger_was[2] = {};
    int reward = 0;
    double reward_at = 0;

    int alive() const { return std::max(int(rd32(VM_ALIVE)), int(rd32(VM_ALIVE2))); }
    bool last_hit = false;        // a hit function ran with one life left
    uint64_t dying_until = 0;     // the last attack plays out until this frame, then the results
    bool confirm = false;         // set by the window: start, or play again
    uint32_t rng = 12345;
    Records records;
    Best best_before;
    bool new_record = false;
    double wave_started = 0;
    double results_at = 0;
    std::mutex view_mutex;        // the window reads mode, wave, score...
    SurvivalView view;

    uint32_t rand(uint32_t n) {
        rng = rng * 1103515245u + 12345u;
        return (rng >> 16) % n;
    }

    // Copies the encounter starting at header pc h to free RAM, ending in an
    // endless wait, and gathers its enemies and camera turns
    bool take_over(uint32_t h) {
        std::vector<int32_t> out;
        std::vector<uint32_t> calls;
        bool counted = false;   // the countdown is in: after the first camera turn, before the first enemy
        // What the encounter waits before its first enemy (-21, and that call's own
        // delay) is part of the countdown: it ends when the first one can come
        int32_t waited = 0;
        size_t count_at = 0;    // the countdown's word in out
        bool first_enemy = true;
        uint32_t i = h;
        while (i < SCRIPT_WORDS) {
            int32_t op = int32_t(rd32(script_addr(i)));
            if ((op == -10 && i != h) || op == -998 || op == -999) break;
            if (op < 0) {
                int n = op_args(op);
                if (n < 0) {
                    printf("[survival] unknown operation %d in encounter %d\n", op, encounter);
                    return false;
                }
                std::vector<int32_t> item;
                for (int k = 0; k <= n; k++) item.push_back(int32_t(rd32(script_addr(i + k))));
                if (op == -9) looks.push_back(item);
                if ((op == -25 || op == -26) && setup.size() < 7) setup.insert(setup.end(), item.begin(), item.end());
                out.insert(out.end(), item.begin(), item.end());
                if (op == -21 && first_enemy) {
                    if (counted) out[count_at] = std::max(0, out[count_at] - item[1]);
                    else waited += item[1];
                }
                if (op == -9 && !counted) {
                    out.push_back(-21), out.push_back(std::max(0, COUNTDOWN_FRAMES - waited));
                    count_at = out.size() - 1, counted = true;
                }
                i += 1 + n;
            } else {
                int32_t nargs = int32_t(rd32(script_addr(i + 2)));
                std::vector<int32_t> item;
                for (int k = 0; k < 3 + nargs; k++) item.push_back(int32_t(rd32(script_addr(i + k))));
                if (is_enemy_fn(uint32_t(item[1]))) {
                    if (!counted) {
                        out.push_back(-21), out.push_back(std::max(0, COUNTDOWN_FRAMES - waited));
                        count_at = out.size() - 1, counted = true;
                    }
                    if (first_enemy) out[count_at] = std::max(0, out[count_at] - item[0]), first_enemy = false;
                    enemies.push_back(item);
                    calls.push_back(uint32_t(out.size()));
                }
                out.insert(out.end(), item.begin(), item.end());
                i += 3 + nargs;
            }
        }
        if (enemies.empty()) {
            printf("[survival] encounter %d has no known enemies\n", encounter);
            return false;
        }
        uint32_t base = FREE_RAM;
        write_script(base, out, calls);
        wr32(VM_PC, script_pc(base));
        printf("[survival] encounter %d: %zu enemy calls, %zu camera turns\n", encounter,
               enemies.size(), looks.size());
        return true;
    }

    // The enemy calls, camera turns and setup of encounter `id` (false: none of them known)
    bool read_encounter(int id, std::vector<std::vector<int32_t>>& e, std::vector<std::vector<int32_t>>& l,
                        std::vector<int32_t>& st) const {
        uint32_t i = 0;
        while (i < SCRIPT_WORDS) {   // its header
            int32_t op = int32_t(rd32(script_addr(i)));
            if (op == -10 && int32_t(rd32(script_addr(i + 1))) == id) break;
            int n = op < 0 ? op_args(op) : -2;
            if (op < 0 && n < 0) return false;
            i += op < 0 ? 1 + uint32_t(n) : 3 + rd32(script_addr(i + 2));
        }
        e.clear(), l.clear(), st.clear();
        for (uint32_t h = i; i < SCRIPT_WORDS;) {
            int32_t op = int32_t(rd32(script_addr(i)));
            if ((op == -10 && i != h) || op == -998 || op == -999) break;
            if (op < 0) {
                int n = op_args(op);
                if (n < 0) return false;
                std::vector<int32_t> item;
                for (int k = 0; k <= n; k++) item.push_back(int32_t(rd32(script_addr(i + uint32_t(k)))));
                if (op == -9) l.push_back(item);
                if ((op == -25 || op == -26) && st.size() < 7) st.insert(st.end(), item.begin(), item.end());
                i += 1 + uint32_t(n);
            } else {
                int32_t nargs = int32_t(rd32(script_addr(i + 2)));
                std::vector<int32_t> item;
                for (int k = 0; k < 3 + nargs; k++) item.push_back(int32_t(rd32(script_addr(i + uint32_t(k)))));
                if (is_enemy_fn(uint32_t(item[1]))) e.push_back(item);
                i += 3 + uint32_t(nargs);
            }
        }
        return !e.empty();
    }

    // An encounter is a row of sections: a camera turn (-9) to where a group of
    // spawn positions is, maybe its setup (-25 / -26), its enemy calls (the
    // raptor's third argument is that group) and a wait for all of them (-1)
    struct Section {
        std::vector<int32_t> look, setup;
        std::vector<std::vector<int32_t>> calls;
    };
    std::vector<Section> sections_of(int id) const {
        std::vector<Section> out;
        uint32_t i = 0;
        while (i < SCRIPT_WORDS) {   // its header
            int32_t op = int32_t(rd32(script_addr(i)));
            if (op == -10 && int32_t(rd32(script_addr(i + 1))) == id) break;
            int n = op < 0 ? op_args(op) : -2;
            if (op < 0 && n < 0) return out;
            i += op < 0 ? 1 + uint32_t(n) : 3 + rd32(script_addr(i + 2));
        }
        Section cur;
        auto close = [&] {
            if (!cur.calls.empty()) out.push_back(cur);
            cur.calls.clear();
            cur.setup.clear();
        };
        for (uint32_t h = i; i < SCRIPT_WORDS;) {
            int32_t op = int32_t(rd32(script_addr(i)));
            if ((op == -10 && i != h) || op == -998 || op == -999) break;
            if (op < 0) {
                int n = op_args(op);
                if (n < 0) break;
                std::vector<int32_t> item;
                for (int k = 0; k <= n; k++) item.push_back(int32_t(rd32(script_addr(i + uint32_t(k)))));
                if (op == -9) close(), cur.look = item;
                if (op == -25 || op == -26) cur.setup.insert(cur.setup.end(), item.begin(), item.end());
                if (op == -1) close();
                i += 1 + uint32_t(n);
            } else {
                int32_t nargs = int32_t(rd32(script_addr(i + 2)));
                std::vector<int32_t> item;
                for (int k = 0; k < 3 + nargs; k++) item.push_back(int32_t(rd32(script_addr(i + uint32_t(k)))));
                if (is_enemy_fn(uint32_t(item[1]))) cur.calls.push_back(item);
                i += 3 + uint32_t(nargs);
            }
        }
        close();
        return out;
    }

    // What a wave at this stop can be made of: the sections of its encounter,
    // and those of the zone's other encounters that turn the camera near
    // where one of this stop's turns does (the same place, seen from here)
    std::vector<Section> wave_pool() const {
        std::vector<Section> pool = sections_of(encounter);
        std::vector<std::vector<int32_t>> own;
        for (const Section& sc : pool)
            if (!sc.look.empty()) own.push_back(sc.look);
        for (int id : zone_encounters) {
            if (id == encounter) continue;
            for (const Section& sc : sections_of(id)) {
                if (sc.look.empty()) continue;
                bool near = false;
                for (auto& l : own)
                    near |= std::hypot(float(l[1] - sc.look[1]), float(l[3] - sc.look[3])) < NEAR_LOOK;
                if (near) pool.push_back(sc);
            }
        }
        if (pool.empty() && !enemies.empty()) {   // (no sections found: everything as one)
            Section sc;
            sc.calls = enemies;
            sc.setup = setup;
            if (!looks.empty()) sc.look = looks[0];
            pool.push_back(sc);
        }
        return pool;
    }

    // Enemy calls of one batch from a section: the raptors in their own slots
    // (one alive per slot), sometimes another raptor type of the zone's
    // (wait: frames between the camera turn and the first of them; -1 a short random one)
    std::vector<int32_t> batch(const Section& sc, int k, int& slot, uint32_t& sig, int32_t wait = -1) {
        std::vector<int32_t> w;
        if (!sc.look.empty())   // the camera turns there first
            w.insert(w.end(), sc.look.begin(), sc.look.end());
        if (!sc.look.empty() || wait >= 0) w.push_back(-21), w.push_back(wait >= 0 ? wait : int32_t(20 + rand(30)));
        w.insert(w.end(), sc.setup.begin(), sc.setup.end());
        std::set<uint32_t> others;   // other enemy kinds: each at most once in a batch (their own slots)
        for (int j = 0; j < k; j++) {
            auto e = sc.calls[rand(uint32_t(sc.calls.size()))];
            if (uint32_t(e[1]) != FN_RAPTOR && !others.insert(uint32_t(e[1])).second) continue;
            e[0] = int32_t(j == 0 ? rand(30) : 10 + rand(50));
            if (uint32_t(e[1]) == FN_RAPTOR && e.size() >= 6) {
                e[3] = slot, slot = slot % 5 + 1;
                if (!raptor_types.empty() && rand(100) < 40) e[4] = raptor_types[rand(uint32_t(raptor_types.size()))];
            }
            sig = sig * 31u + uint32_t(e[1]) * 7u + uint32_t(e.size() >= 6 ? e[4] * 13 + e[5] : 0);
            batch_calls.push_back(uint32_t(w.size()));
            w.insert(w.end(), e.begin(), e.end());
        }
        return w;
    }
    std::vector<uint32_t> batch_calls;   // where batch() put the enemy calls (offsets in its words)

    // The waves come from encounter `id` from now on
    bool adopt(int id) {
        std::vector<std::vector<int32_t>> e, l;
        std::vector<int32_t> st;
        if (!read_encounter(id, e, l, st)) return false;
        enemies = e, looks = l, setup = st;
        encounter = id;
        log("  waves from encounter %d\n", id);
        return true;
    }

    // Nodes of the zone trigger task (fn_00014764) that are still to come; the
    // list has the newest first, so the last ones are the nearest along the path
    std::vector<uint32_t> triggers_ahead() const {
        std::vector<uint32_t> nodes;
        uint32_t task = rd32(TRIGGER_TASK);
        if (!task || task >= RAM_SIZE || rd32(task + 0xC) != 0x14764) return nodes;
        for (uint32_t node = rd32(task); node && node < RAM_SIZE && nodes.size() < 256; node = rd32(node))
            nodes.push_back(node);
        return nodes;
    }

    // The next stop along the path (x, z), or false when none is left
    bool next_stop(float& x, float& z) const {
        auto nodes = triggers_ahead();
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
            if (rd32(*it + 4) == FN_STOP) {
                x = rdf(*it + 8), z = rdf(*it + 0x10);
                return true;
            }
        return false;
    }

    float to_next_stop() const {
        float x, z;
        if (!next_stop(x, z)) return 0;
        return std::hypot(x - rdf(CAMERA_POS), z - rdf(CAMERA_POS + 8));
    }

    // The encounter of the next stop: the nearest start trigger along the path
    // with enemies. None left: this is the zone's last stop (the stops after
    // it, if any, only lead to the next zone)
    int next_encounter() const {
        auto nodes = triggers_ahead();
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
            if (rd32(*it + 4) == FN_ENCOUNTER_START && encounter_enemies(int(rd32(*it + 0x18))) > 0)
                return int(rd32(*it + 0x18));
        return -1;
    }

    // The zone's encounters with enemies (this one and those of the stops ahead)
    // and the raptor types they use
    void find_zone_encounters() {
        zone_encounters = {encounter};
        for (uint32_t node : triggers_ahead()) {
            int id = int(rd32(node + 0x18));
            if (rd32(node + 4) == FN_ENCOUNTER_START && encounter_enemies(id) > 0 &&
                std::find(zone_encounters.begin(), zone_encounters.end(), id) == zone_encounters.end())
                zone_encounters.push_back(id);
        }
        std::set<int32_t> types;
        for (int id : zone_encounters)
            for (const Section& sc : sections_of(id))
                for (auto& e : sc.calls)
                    if (uint32_t(e[1]) == FN_RAPTOR && e.size() >= 6) types.insert(e[4]);
        raptor_types.assign(types.begin(), types.end());
        log("  zone: %zu encounters, %zu raptor types\n", zone_encounters.size(), raptor_types.size());
    }

    void set_phase(Phase p) {
        phase = p;
        phase_at = g_frame_no;
    }

    // The game's walker goes on along the path, as at the end of an encounter
    // (op -999: fn_0001A4DC(0x22, 0xFF, 0) and 0x1A3960 = 1); the camera stops
    // turning to the last point an encounter showed and looks ahead again
    void walk_on() {
        wr32(CAM_GAIT_RAMP, 0x22), wr32(CAM_GAIT_SPEED, 0xFF), wr32(CAM_GAIT_HALT, 0);
        wr32(VM_FLAGS, rd32(VM_FLAGS) & ~8u);
        wr32(CAM_WALK, 1);
    }

    void halt() {
        wr32(CAM_WALK, 0);
        wr32(CAM_GAIT_SPEED, 0), wr32(CAM_GAIT_HALT, 0xFF), wr32(CAM_GAIT_RAMP, 0);   // brakes like a stop trigger
    }

    uint64_t clear_frames() const { return std::max<uint64_t>(60, CLEAR_FRAMES - 40 * uint64_t(lap - 1)); }

    // The stop is cleared: on to the next one
    void start_walk() {
        pending = -1;
        stop_passed = false;
        uint64_t f = g_frame_no;
        bool ambush = rand(100) < uint32_t(AMBUSH_CHANCE) || getenv("LW_SURV_AMBUSH");
        bool bonus = rand(100) < uint32_t(BONUS_CHANCE) || getenv("LW_SURV_BONUS");
        walk_length = to_next_stop();
        ambush_planned = ambush && walk_length > 40;   // not on a short one
        bonus_at = bonus ? f + 20 + rand(80) : 0;
        walk_on();
        set_phase(WALK);
        log("  walk on (ambush %d, bonus %d)\n", int(ambush), int(bonus));
    }

    // Dinosaurs on the way: the walker stops and a short wave comes from the
    // next stop's encounter (the one the camera is heading to)
    void start_ambush() {
        halt();
        int id = pending >= 0 ? pending : next_encounter();   // (its trigger may be behind already)
        std::vector<Section> pool = id >= 0 ? sections_of(id) : std::vector<Section>{};
        if (pool.empty()) pool = wave_pool();
        int count = std::min(5, 2 + lap), slot = 1;
        uint32_t sig = 0;
        std::vector<int32_t> w = {-21, 10};
        batch_calls.clear();
        auto b = batch(pool[rand(uint32_t(pool.size()))], count, slot, sig);   // the camera turns to them
        std::vector<uint32_t> calls;
        for (uint32_t c : batch_calls) calls.push_back(uint32_t(w.size()) + c);
        w.insert(w.end(), b.begin(), b.end());
        w.push_back(-1);
        uint32_t addr = FREE_RAM + 0x30000;   // its own buffer: the machine waits in another one
        write_script(addr, w, calls);
        wr32(VM_PC, script_pc(addr));
        wr32(VM_WAIT, 0);
        wr32(VM_COUNTDOWN, 0);
        ambush_planned = false;
        ambush_started = now_seconds();
        set_phase(AMBUSH);
        printf("[survival] ambush: %d enemies from encounter %d\n", count, id);
    }

    // Walking: an ambush when planned, else on until the next stop trigger stops the walker
    void watch_walk() {
        uint64_t f = g_frame_no;
        if (ambush_planned && f > phase_at + 20 && rd32(CAM_WALK) == 1 && to_next_stop() < 0.55f * walk_length) {
            start_ambush();
            return;
        }
        bool zone_left = int(rd32(ZONE) & 15) != zone;
        if (f > phase_at + WALK_LONGEST || zone_left) {   // no stop ahead after all: the lap is over
            printf("[survival] the walk reaches no stop%s: new lap\n", zone_left ? " (another zone)" : "");
            halt();
            start_lap_out();
            return;
        }
        if (f < phase_at + 30 || rd32(CAM_WALK) != 0 || std::fabs(rdf(CAM_SPEED)) > 0.5f) return;
        arrive();
    }

    // At the next stop. Its encounter went by as a trigger (skipped: the
    // machine still runs the first one): the waves come from it now
    void arrive() {
        ambush_planned = false;
        stop++;
        last_stop = next_encounter() < 0;
        if (pending >= 0 && pending != encounter) adopt(pending);
        log("  stop %d reached (encounter %d)%s\n", stop, pending, last_stop ? ", the last one" : "");
        next_wave();
        set_phase(FIGHT);
    }

    // Every frame while the hunt is on
    void tick() {
        if (state != HOLDING) return;
        bool at_end = rd32(VM_PC) == end_pc;
        uint64_t f = g_frame_no;
        switch (phase) {
        case FIGHT:
            if (!at_end) break;
            last_stop = next_encounter() < 0;
            set_phase(CLEARED);
            log("  survival wave %d cleared%s\n", wave + 1, last_stop ? " (last stop)" : "");
            if (g_trace) {
                log("  triggers ahead:");
                for (uint32_t node : triggers_ahead())
                    log(" %X@%.0f,%.0f,%.0f", rd32(node + 4), rdf(node + 8), rdf(node + 0xC), rdf(node + 0x10));
                log("  cam %.0f,%.0f,%.0f zone %d\n", rdf(CAMERA_POS), rdf(CAMERA_POS + 4), rdf(CAMERA_POS + 8),
                    int(rd32(ZONE) & 15));
            }
            break;
        case CLEARED:
            if (f < phase_at + clear_frames()) break;
            if (last_stop) start_lap_out();
            else start_walk();
            break;
        case WALK: watch_walk(); break;
        case AMBUSH:
            if (at_end && stop_passed) {   // it was at the stop already
                arrive();
            } else if (at_end) {           // the way is clear again
                walk_on();
                set_phase(WALK);
                phase_at = f - 31;   // (a walk already under way)
            }
            break;
        case LAP_OUT:
            if (f >= phase_at + FADE_FRAMES) {   // black: the first stop comes back
                if (game_state_load_mark()) set_phase(LAP_LOAD);
                else new_lap(false);
            }
            break;
        case LAP_LOAD:
            if (int r = game_state_result()) new_lap(r > 0);
            break;
        }
        watch_bonus();
    }

    // The end of the zone: fade out, then the state kept at the first stop
    void start_lap_out() {
        saved_lives = int(rd32(PLAYER_LIFE));
        saved_score = score();
        bonus_on = false;
        bonus_at = 0;
        ambush_planned = false;
        set_phase(LAP_OUT);
        log("  lap %d over\n", lap);
    }

    // The game is back at the first stop (or, if that failed, still at the last)
    void new_lap(bool back) {
        if (back) {
            wr32(PLAYER_LIFE, uint32_t(saved_lives));
            for (uint32_t a : {SCORE[0], SCORE[2 + size_t(stage)]}) wr32(a, uint32_t(saved_score));
            wr32(SCORE[1], ~0u);   // the counter shows it
            enemies = first_enemies, looks = first_looks, setup = first_setup;
            encounter = first_encounter;
        } else {
            printf("[survival] could not go back to the first stop: the lap goes on here\n");
        }
        lives = saved_lives;
        lap++;
        stop = 0;
        last_stop = false;
        next_wave();
        set_phase(FIGHT);
        fade_in_at = g_frame_no;
        lap_started = now_seconds();
        printf("[survival] lap %d\n", lap);
    }

    void give_reward() {
        int lives_now = int(rd32(PLAYER_LIFE));
        if (lives_now < MAX_LIVES) {
            wr32(PLAYER_LIFE, uint32_t(lives_now + 1));
            reward = 1;
        } else {
            for (uint32_t a : {SCORE[0], SCORE[2 + size_t(stage)]}) wr32(a, rd32(a) + GOLD_POINTS);
            wr32(SCORE[1], ~0u);
            reward = 2;
        }
        reward_at = now_seconds();
        log("  gold reward %d\n", reward);
    }

    // The golden target: up for a while during a walk, drifting a little; a
    // shot on it (either player's gun) gives a life, or points with the hearts full
    void watch_bonus() {
        uint64_t f = g_frame_no;
        if (bonus_at && f >= bonus_at && phase == WALK) {
            bonus_at = 0;
            bonus_on = true;
            bonus_until = f + BONUS_FRAMES;
            bonus_bx = 0.22f + float(rand(560)) / 1000.0f;
            bonus_by = 0.22f + float(rand(300)) / 1000.0f;
            log("  bonus up\n");
        }
        if (!bonus_on) {
            for (int p = 0; p < 2; p++) trigger_was[p] = g_inputs.p[p].trigger;
            return;
        }
        float t = float(f - (bonus_until - BONUS_FRAMES)) / 57.5f;
        bonus_x = bonus_bx + 0.06f * std::sin(t * 1.7f);
        bonus_y = bonus_by + 0.04f * std::sin(t * 2.3f);
        bool hit = false;
        for (int p = 0; p < 2; p++) {
            bool down = g_inputs.p[p].trigger;
            PlayerAim a = player_aim(p);
            if (down && !trigger_was[p] && a.inside &&
                std::hypot((a.x - bonus_x) * 496.0f, (a.y - bonus_y) * 384.0f) < BONUS_HIT)
                hit = true;
            trigger_was[p] = down;
        }
        if (getenv("LW_SURV_BONUS") && f >= bonus_until - BONUS_FRAMES + 60) hit = true;   // testing aid: shot by itself
        if (hit) {
            bonus_on = false;
            give_reward();
        } else if (f >= bonus_until || phase == LAP_OUT) {
            bonus_on = false;
        }
    }

    // Writes words plus the endless wait at the end; `calls`: where in them the enemies come
    void write_script(uint32_t addr, std::vector<int32_t> words, const std::vector<uint32_t>& calls) {
        enemy_pcs.clear();
        for (uint32_t k : calls) enemy_pcs.push_back(script_pc(addr) + k);
        words.push_back(-21);
        words.push_back(WAIT_FOREVER);
        for (size_t k = 0; k < words.size(); k++) wr32(addr + 4 * uint32_t(k), uint32_t(words[k]));
        end_pc = script_pc(addr) + uint32_t(words.size()) - 2;
    }

    // A wave: the countdown (the camera already turned to where the first
    // ones come from), then batches of a few dinosaurs, each from a random section of the
    // pool (the camera turns to where they come from first), with random
    // pauses between them; the next batch sometimes comes before the last
    // one is all dead. More dinosaurs and bigger batches at every stop of the
    // lap and every lap; never the same wave twice in a row
    void next_wave() {
        wave++;
        int level = stop + 3 * (lap - 1);   // harder at every stop of the lap, and every lap
        int diff = std::min(15, base_diff + 1 + level / 2);
        wr8(SETTING_DIFF, uint8_t((rd8(SETTING_DIFF) & 0x0F) | (diff << 4)));
        int total = std::min(WAVE_MAX, std::max(WAVE_MIN, int(first_count)) + 2 * level);
        std::vector<Section> pool = wave_pool();
        std::vector<int32_t> w;
        std::vector<uint32_t> calls;
        int batches = 0;
        for (int attempt = 0; attempt < 6 && !pool.empty(); attempt++) {
            w.clear();
            calls.clear();
            batches = 0;
            uint32_t sig = 0;
            int slot = 1 + int(rand(5)), left = total, prev = -1;
            while (left > 0) {
                int k = std::min(left, std::clamp(2 + level / 3 + int(rand(2)), 2, 5));
                int pick = int(rand(uint32_t(pool.size())));
                if (pick == prev && pool.size() > 1) pick = (pick + 1 + int(rand(uint32_t(pool.size() - 1)))) % int(pool.size());
                prev = pick;
                sig = sig * 131u + uint32_t(pick) * 17u + uint32_t(k);
                batch_calls.clear();
                // The first batch: the camera turns to it, then the countdown
                auto b = batch(pool[size_t(pick)], k, slot, sig, batches == 0 ? COUNTDOWN_FRAMES : -1);
                for (uint32_t c : batch_calls) calls.push_back(uint32_t(w.size()) + c);
                w.insert(w.end(), b.begin(), b.end());
                left -= int(batch_calls.size()) > 0 ? int(batch_calls.size()) : k;
                batches++;
                if (left <= 0) break;
                // Between batches: all dead first, or (more so later on) only one left
                if (int(rand(100)) < std::min(70, 25 + 5 * level)) w.push_back(-27), w.push_back(1);
                else w.push_back(-1);
                w.push_back(-21), w.push_back(int32_t(10 + rand(70)));
            }
            if (sig != last_sig) {
                last_sig = sig;
                break;
            }
        }
        w.push_back(-1);
        // Two buffers: the new wave never overwrites the words being run
        uint32_t addr = FREE_RAM + 0x10000 * (1 + (wave & 1));
        write_script(addr, w, calls);
        wr32(VM_PC, script_pc(addr));
        wr32(VM_WAIT, 0);
        wr32(VM_COUNTDOWN, 0);   // the endless wait of the break was counting down
        wave_started = now_seconds();
        printf("[survival] wave %d: %zu enemies in %d batches (from %zu sections), difficulty %d\n", wave + 1,
               calls.size(), batches, pool.size(), diff);
        log("  survival wave %d count %zu batches %d pool %zu diff %d\n", wave + 1, calls.size(), batches, pool.size(),
            diff);
    }

    int score() const { return int(rd32(SCORE[0])); }

    void set_diff(int d) { wr8(SETTING_DIFF, uint8_t((rd8(SETTING_DIFF) & 0x0F) | (d << 4))); }
    void restore() { set_diff(base_diff); }   // the difficulty as the settings have it

    // The hunt is over: keep the record and show the results
    void finish() {
        auto& best = records.best[{stage + 1, zone}];
        best_before = best;
        new_record = wave + 1 > best.wave || (wave + 1 == best.wave && score() > best.score);
        if (new_record) {
            best = {wave + 1, score(), lap};
            records.save();
        }
        printf("[survival] end: lap %d, wave %d, %d points%s\n", lap, wave + 1, score(),
               new_record ? " (record)" : "");
        mode = SurvivalMode::Results;
        results_at = now_seconds();
    }

    // Play again (testing aid: LW_SURV_AUTOSTART): back to the first stop, as a
    // new lap with lives, score and waves as at the start
    void restart() {
        set_diff(base_diff);
        wave = -1;
        lap = 0;
        saved_lives = start_lives;
        saved_score = 0;
        best_before = records.best[{stage + 1, zone}];
        new_record = false;
        mode = SurvivalMode::Playing;
        if (game_state_load_mark()) set_phase(LAP_LOAD);
        else new_lap(false);
    }

    // After a hit with one life left (survival_enter gave one more): did it take it?
    void watch_lives() {
        lives = int(rd32(PLAYER_LIFE));
        if (!last_hit) return;
        last_hit = false;
        if (lives <= 1) {   // it did: the hunt is over, once the attack has been seen
            lives = 0;
            if (!dying_until) dying_until = g_frame_no + DYING_FRAMES, log("  last hit\n");
        } else {            // that call did not hurt
            wr32(PLAYER_LIFE, uint32_t(--lives));
        }
    }

    // Buttons pressed on the way in, while the loading screen is up: a credit
    // and start (again until the course select opens), then a shot on it
    // (any course: the stage is set)
    void press_buttons() {
        uint64_t f = loading_frames;
        uint32_t select = rd32(COURSE_SELECT);
        bool boot = g_frame_no < 900;   // a button held during the boot changes what it does
        g_inputs.p[0].coin = !boot && select == 0 && f % 240 < 6;
        g_inputs.p[0].start = !boot && select == 0 && f % 240 >= 60 && f % 240 < 66;
        g_inputs.p[0].trigger = !boot && (select == 1 || select == 2) && f % 90 < 6;
        g_inputs.p[0].offscreen = false;
        g_inputs.p[0].gun_x = 128, g_inputs.p[0].gun_y = 128;
        if (select == 0xC && shoot_in) {   // in the stage: shoots its way to the arena, lives kept
            g_inputs.p[0].trigger = f % 12 < 3;
            g_inputs.p[0].offscreen = f % 240 < 3;
            g_inputs.p[0].gun_x = int(40 + (f * 7) % 180);
            g_inputs.p[0].gun_y = int(40 + (f * 3) % 170);
            if (!loading_lives) loading_lives = rd32(PLAYER_LIFE);
            if (loading_lives) wr32(PLAYER_LIFE, loading_lives);
        }
        step = boot || state == STARTING ? step
             : select == 0 ? SurvivalStep::Coin
             : select != 0xC ? SurvivalStep::Course
                             : SurvivalStep::Travelling;
    }

    float progress() const {
        switch (step) {
        case SurvivalStep::Preparing: return 0.03f;
        case SurvivalStep::Coin: return 0.12f;
        case SurvivalStep::Course: return 0.25f;
        case SurvivalStep::Travelling:   // creeps on: the trip has no known length
            return 0.35f + 0.6f * (1 - std::exp(-float(travel_frames) / 1500.0f));
        default: return 1;
        }
    }
    uint64_t travel_frames = 0;
    uint32_t loading_lives = 0;   // the player's lives as the stage began (kept on the way in)
    bool shoot_in = false;        // SurvivalZone::shoot_in of this arena

    // The arena's first encounter is ours (take_over): the start screen
    void arrive() {
        base_diff = rd8(SETTING_DIFF) >> 4;
        state = HOLDING;
        wave = 0;
        first_encounter = encounter;   // each lap starts again here
        first_enemies = enemies, first_looks = looks, first_setup = setup;
        first_count = enemies.size();
        find_zone_encounters();
        start_lives = lives = std::max(1, int(rd32(PLAYER_LIFE)));
        wave_started = now_seconds();
        if (mode == SurvivalMode::Loading) mode = SurvivalMode::Ready, step = SurvivalStep::Arrived;
    }

    // The arena kept the first time it was reached (save/survival/), made
    // with this LIFE and difficulty setting: the way there only once
    std::string cache;            // its file: being loaded or saved this run
    bool cache_loading = false, cache_saving = false;
    std::string cache_path() const {
        char p[96];
        snprintf(p, sizeof(p), "save/survival/%d_%d_l%u_d%u.state", stage + 1, zone, unsigned(rd8(SETTING_LIFE)),
                 unsigned(rd8(SETTING_DIFF) >> 4));
        return p;
    }
    // Loaded: the encounter the state stands at (the copy at FREE_RAM, as
    // take_over left it) is taken over again from its start
    bool resume() {
        if (int32_t(rd32(FREE_RAM)) != -10) return false;
        int id = int32_t(rd32(FREE_RAM + 4));
        uint32_t i = 0;
        while (i < SCRIPT_WORDS) {   // its header in the script
            int32_t op = int32_t(rd32(script_addr(i)));
            if (op == -10 && int32_t(rd32(script_addr(i + 1))) == id) break;
            int n = op < 0 ? op_args(op) : -2;
            if (op < 0 && n < 0) return false;
            i += op < 0 ? 1 + uint32_t(n) : 3 + rd32(script_addr(i + 2));
        }
        if (i >= SCRIPT_WORDS) return false;
        encounter = id;
        zone = int(rd32(ZONE) & 15);
        enemies.clear(), looks.clear(), setup.clear();
        if (!take_over(i)) return false;
        arrive();
        return true;
    }

    // A new run: everything as at the start
    void reset(int s, int z) {
        stage = s - 1;
        zone = z;
        state = WAITING;
        zone_empty = false;
        mode = SurvivalMode::Loading;
        encounter = -1;
        enemies.clear(), looks.clear(), setup.clear();
        wave = 0;
        armed = false;
        lives = start_lives = 0;
        last_hit = false;
        dying_until = 0;
        confirm = false;
        enemy_pcs.clear();
        phase = FIGHT;
        lap = 1;
        stop = 0;
        last_stop = false;
        pending = -1;
        ambush_planned = false;
        bonus_at = fade_in_at = 0;
        first_encounter = -1;
        first_enemies.clear(), first_looks.clear(), first_setup.clear();
        first_count = 0;
        zone_encounters.clear(), raptor_types.clear();
        last_sig = 0;
        bonus_on = false;
        reward = 0;
        lap_started = ambush_started = 0;
        loading_frames = travel_frames = 0;
        loading_lives = 0;
        cache.clear();
        cache_loading = cache_saving = false;
        shoot_in = false;
        for (const SurvivalZone& sz : SURVIVAL_ZONES)
            if (sz.stage == s && sz.zone == z) shoot_in = sz.shoot_in;
        step = SurvivalStep::Preparing;
        new_record = false;
        records.load();
        best_before = records.best[{s, z}];
        wave_started = 0;
    }

    void publish() {
        std::lock_guard<std::mutex> lock(view_mutex);
        view.mode = mode;
        view.stage = stage + 1;
        view.zone = zone;
        view.wave = wave + 1;
        view.score = mode == SurvivalMode::Loading ? 0 : score();
        view.lives = lives;
        view.best_wave = best_before.wave;
        view.best_score = best_before.score;
        view.new_record = new_record;
        view.wave_started = wave_started;
        view.results_at = results_at;
        view.lap = lap;
        view.best_lap = best_before.lap;
        view.lap_started = lap_started;
        view.ambush_started = ambush_started;
        bool playing = mode == SurvivalMode::Playing;
        view.walking = playing && phase == WALK;
        view.ambush = playing && phase == AMBUSH;
        view.zone_done = playing && ((phase == CLEARED && last_stop) || phase == LAP_OUT || phase == LAP_LOAD);
        view.resting = playing && (phase == CLEARED || phase == WALK || phase == LAP_OUT || phase == LAP_LOAD);
        view.rest_left = phase == CLEARED ? float(phase_at + clear_frames() - std::min(g_frame_no, phase_at + clear_frames())) / 57.524f : 0;
        float fade = 0;
        if (phase == LAP_OUT) fade = float(g_frame_no - phase_at) / float(FADE_FRAMES);
        else if (phase == LAP_LOAD) fade = 1;
        else if (fade_in_at && g_frame_no < fade_in_at + FADE_FRAMES) fade = 1 - float(g_frame_no - fade_in_at) / float(FADE_FRAMES);
        view.fade = playing ? std::clamp(fade, 0.0f, 1.0f) : 0;
        view.bonus_on = playing && bonus_on;
        view.bonus_x = bonus_x, view.bonus_y = bonus_y;
        view.bonus_left = bonus_on ? float(bonus_until - std::min(g_frame_no, bonus_until)) / float(BONUS_FRAMES) : 0;
        view.reward = reward;
        view.reward_at = reward_at;
        view.step = mode == SurvivalMode::Loading ? step : SurvivalStep::Arrived;
        uint32_t pc = rd32(VM_PC);
        int to_come = 0;
        for (uint32_t p : enemy_pcs) to_come += p >= pc;
        view.total = int(enemy_pcs.size());
        view.left = view.resting ? 0 : std::min(view.total, alive() + to_come);
        view.progress = mode == SurvivalMode::Loading ? progress() : 1;
    }
};

Survival g_surv;
int g_request_stage = 0, g_request_zone = 0;   // --survival
std::atomic<bool> g_begin{false}, g_ungate{false}, g_end{false};
std::atomic<int> g_begin_stage{0}, g_begin_zone{0};

}  // namespace

bool survival_option(const std::string& name, const char* value) {
    if (name == "--trace-survival") {
        g_trace = true;
        g_log = fopen(getenv("LW_SURV_TRACE") ? getenv("LW_SURV_TRACE") : "out/survival_trace.txt", "w");
        hook(FN_TASK_NEW);
        return true;
    }
    if (name == "--survival") {
        int s = 1, z = 1;
        if (!value || sscanf(value, "%d:%d", &s, &z) < 1) return false;
        g_request_stage = s, g_request_zone = z;
        return true;
    }
    return false;
}

bool survival_cli_request(int& stage, int& zone) {
    if (g_request_stage <= 0) return false;
    stage = g_request_stage, zone = g_request_zone;
    return true;
}

void survival_hooks() {
    hook(FN_ENCOUNTER);
    hook(FN_NEXT_STAGE);
    hook(FN_PLAYLIST_INIT);
    hook(FN_PLAYER_HIT[0]);
    hook(FN_PLAYER_HIT[1]);
}

bool survival_record(int stage, int zone, int& wave, int& score, int* lap) {
    Records r;
    r.load();
    auto it = r.best.find({stage, zone});
    if (it == r.best.end() || it->second.wave <= 0) return false;
    wave = it->second.wave, score = it->second.score;
    if (lap) *lap = it->second.lap;
    return true;
}

// The window thread asks; the game thread does it at its next frame
void survival_begin(int stage, int zone) {
    g_begin_stage = stage, g_begin_zone = zone;
    g_begin = true;
    std::lock_guard<std::mutex> lock(g_surv.view_mutex);   // the loading screen right away
    g_surv.view = SurvivalView{};
    g_surv.view.mode = SurvivalMode::Loading;
    g_surv.view.stage = stage, g_surv.view.zone = zone;
    int w = 0, sc = 0, lp = 0;
    if (survival_record(stage, zone, w, sc, &lp))
        g_surv.view.best_wave = w, g_surv.view.best_score = sc, g_surv.view.best_lap = lp;
}

void survival_ungate() { g_ungate = true; }

void survival_end() {
    g_end = true;
    std::lock_guard<std::mutex> lock(g_surv.view_mutex);
    g_surv.view.mode = SurvivalMode::Off;
}

bool survival_enabled() { return g_surv.on || g_begin; }

SurvivalView survival_view() {
    std::lock_guard<std::mutex> lock(g_surv.view_mutex);
    return g_surv.view;
}

bool survival_owns_inputs() {
    return (g_surv.on && (g_surv.mode == SurvivalMode::Loading || g_surv.dying_until)) || g_begin;
}

void survival_confirm() { g_surv.confirm = true; }

double survival_now() { return now_seconds(); }

void survival_enter(Ctx& c, uint32_t fn) {
    if (!g_hooks.count(fn)) return;
    if (g_surv.gate && fn != FN_TASK_NEW) return;
    if (fn == FN_TASK_NEW && g_trace) log("  task+ %08X size %X from %08X\n", c.r[4], c.r[3], c.lr);
    if (fn == FN_PLAYLIST_INIT && g_surv.on) g_surv.armed = true;
    if (fn == FN_NEXT_STAGE && g_surv.on && g_surv.armed) {
        // The course (Expert skips stages with more calls) lands on the chosen stage
        g_surv.armed = false;
        for (int32_t i = std::max(0, int32_t(rd32(PLAYLIST_INDEX)) + 1); i < 5; i++)
            wr32(PLAYLIST + 4 * uint32_t(i), uint32_t(g_surv.stage));
    }
    if ((fn == FN_PLAYER_HIT[0] || fn == FN_PLAYER_HIT[1]) && g_surv.on && g_surv.mode == SurvivalMode::Playing &&
        int(rd32(PLAYER_LIFE)) <= 1) {   // this hit would end the game: it takes the extra life instead
        wr32(PLAYER_LIFE, rd32(PLAYER_LIFE) + 1);
        g_surv.last_hit = true;
    }
    if (fn == FN_ENCOUNTER && g_surv.on) {
        Survival& s = g_surv;
        uint32_t pc = rd32(VM_PC);
        // On the way in, an encounter before the arena runs without its
        // dinosaurs: nobody shoots them, and some never leave by themselves
        // (stage 3's lobby), which held the way there forever. Ending the
        // encounter at once instead loses the stage's path (the game wanders off)
        if (s.state == Survival::WAITING && s.mode == SurvivalMode::Loading && pc < SCRIPT_WORDS &&
            int32_t(rd32(script_addr(pc))) == -10 && quiet_copy(pc)) {
            wr32(VM_PC, script_pc(QUIET_SCRIPT));
            return;
        }
        if (s.state == Survival::STARTING) {   // first run of the encounter: its header
            if (s.take_over(pc)) {
                s.arrive();
                // The arena as it is now, for next time (it loads from there)
                if (s.cache.empty()) {
                    s.cache = s.cache_path();
                    CreateDirectoryA("save/survival", nullptr);
                    game_state_save(s.cache + ".tmp");
                    s.cache_saving = true;
                }
            } else {   // no enemies in this one: wait for the next encounter, of this zone or a later one
                s.enemies.clear(), s.looks.clear(), s.setup.clear();
                s.state = Survival::WAITING;
                s.zone_empty = true;
            }
        }
    }
}

bool survival_spawn_call(Ctx& c, uint32_t fn) {
    uint32_t r = c.r[3];   // node+4: {fn, x, y, z, radius, p0, p1}
    if (g_trace) {
        log("  spawn %08X pos %.1f %.1f %.1f r %.1f p %08X %08X\n", fn, rdf(r + 4), rdf(r + 8), rdf(r + 12),
            rdf(r + 16), rd32(r + 20), rd32(r + 24));
        if (fn == FN_ENCOUNTER_START)
            log("  encounter %d stage %d zone %d enemies %d\n", int(rd32(r + 20)), rd8(STAGE) + 1, int(rd32(ZONE) & 15),
                encounter_enemies(int(rd32(r + 20))));
    }
    Survival& s = g_surv;
    if (s.gate) return false;
    int zone_now = int(rd32(ZONE) & 15);
    if (s.on && s.state == Survival::WAITING && fn == FN_ENCOUNTER_START && rd8(STAGE) == s.stage &&
        (zone_now == s.zone || (s.zone_empty && zone_now > s.zone))) {
        if (zone_now != s.zone) {
            printf("[survival] zone %d has no enemies: the arena is zone %d\n", s.zone, zone_now);
            s.zone = zone_now;
        }
        s.encounter = int(rd32(r + 20));
        s.state = Survival::STARTING;
        s.step = SurvivalStep::Travelling;
        printf("[survival] starts at encounter %d\n", s.encounter);
    }
    // Holding: no other encounter starts (the machine keeps running the first
    // one); the one the walk goes past gives the waves of the next stop
    if (s.on && s.state == Survival::HOLDING && fn == FN_ENCOUNTER_START) {
        if (s.phase == Survival::WALK || s.phase == Survival::AMBUSH) s.pending = int(rd32(r + 20));
        return true;
    }
    if (s.on && s.state == Survival::HOLDING && fn == FN_STOP && (s.phase == Survival::WALK || s.phase == Survival::AMBUSH))
        s.stop_passed = true;
    return false;
}

bool survival_frame(Ctx&) {
    g_frame_no++;
    Survival& s = g_surv;
    bool hold = false;
    if (g_end.exchange(false) && s.on) {
        s.restore();
        s.on = false;
        s.mode = SurvivalMode::Off;
    }
    if (g_begin.exchange(false)) {
        if (s.on) s.restore();
        s.on = true;
        s.gate = true;
        s.rng = uint32_t(time(nullptr));
        s.reset(g_begin_stage, g_begin_zone);
        printf("[survival] stage %d, zone %d\n", s.stage + 1, s.zone);
    }
    if (g_ungate.exchange(false)) s.gate = false;
    if (s.on) {
        switch (s.mode) {
        case SurvivalMode::Loading:
            if (s.gate) break;
            if (s.loading_frames == 0 && !getenv("LW_SURV_NO_CACHE")) {   // the arena kept from another time
                std::string path = s.cache_path();
                if (game_state_valid(path) && game_state_load(path)) s.cache = path, s.cache_loading = true;
            }
            if (s.cache_loading) {
                s.loading_frames = 1;
                int r = game_state_result();
                if (r == 0) break;
                s.cache_loading = false;
                if (r > 0 && s.resume()) {
                    printf("[survival] arena loaded from %s\n", s.cache.c_str());
                    break;
                }
                printf("[survival] %s did not load: the way there instead\n", s.cache.c_str());
                DeleteFileA(s.cache.c_str());
                s.cache.clear();
                if (r > 0) {   // loaded but not understood: from the attract again
                    s.on = false;
                    s.mode = SurvivalMode::Off;
                    s.publish();
                    break;
                }
            }
            s.loading_frames++;
            if (s.step == SurvivalStep::Travelling) s.travel_frames++;
            s.press_buttons();
            if (s.loading_frames > LOADING_GIVE_UP) {
                printf("[survival] did not reach zone %d of stage %d\n", s.zone, s.stage + 1);
                s.on = false;
                s.mode = SurvivalMode::Off;
                s.publish();   // the window goes back to the menu
            }
            break;
        case SurvivalMode::Ready:   // the start screen: the game waits for the first shot
            if (s.cache_saving && game_state_result() != 0) {
                s.cache_saving = false;
                bool ok = game_state_result() > 0 &&
                          MoveFileExA((s.cache + ".tmp").c_str(), s.cache.c_str(), MOVEFILE_REPLACE_EXISTING);
                printf("[survival] arena %s %s\n", ok ? "kept in" : "could not be kept in", s.cache.c_str());
            }
            if (s.confirm || getenv("LW_SURV_AUTOSTART")) {   // testing aid: no window to shoot at
                s.confirm = false;
                s.mode = SurvivalMode::Playing;
                s.wave_started = s.lap_started = now_seconds();
                for (uint32_t a : SCORE) wr32(a, 0);
                wr32(SCORE[1], ~0u);   // the counter shows the 0 (it still showed the score of the way in)
                game_state_mark();   // the first stop: every lap starts again from here
                log("  survival wave 1 count %zu (the encounter itself)\n", s.enemies.size());
                s.set_phase(Survival::FIGHT);
            } else {
                hold = true;
            }
            s.lives = int(rd32(PLAYER_LIFE));
            break;
        case SurvivalMode::Playing:
            s.tick();
            s.watch_lives();
            if (s.dying_until) {   // the player's controls do nothing meanwhile (survival_owns_inputs)
                g_inputs.p[0].trigger = g_inputs.p[0].offscreen = false;
                if (g_frame_no >= s.dying_until) {
                    s.dying_until = 0;
                    s.finish();
                }
            }
            hold = s.mode == SurvivalMode::Results;
            break;
        case SurvivalMode::Results:
            if (s.confirm || getenv("LW_SURV_AUTOSTART")) {
                s.confirm = false;
                s.restart();
            } else {
                hold = true;
            }
            break;
        default: break;
        }
        s.publish();
    }
    if (!g_trace) return hold;
    if (const char* life = getenv("LW_SURV_LIFE"); life && g_frame_no > 1000) wr8(SETTING_LIFE, uint8_t(atoi(life)));
    if (getenv("LW_SURV_GOD")) {   // testing aid: the player's life never drops
        if (s.on) s.lives = std::max(s.lives, 5);
        uint32_t life = rd32(PLAYER_LIFE);
        if (life > 0 && life < 5) wr32(PLAYER_LIFE, 5);
    }
    if (getenv("LW_SURV_AUTOPLAY") && rd32(COURSE_SELECT) == 0xC) {   // testing aid: shoots all over the screen
        uint64_t f = g_frame_no;
        g_inputs.p[0].trigger = f % 12 < 3;
        g_inputs.p[0].offscreen = f % 240 < 3;
        g_inputs.p[0].gun_x = int(40 + (f * 7) % 180);
        g_inputs.p[0].gun_y = int(40 + (f * 3) % 170);
    }
    static std::map<uint32_t, uint32_t> last;
    auto now = tasks();
    for (auto& [t, fn] : last)
        if (!now.count(t) || now[t] != fn) log("  task- %08X at %08X\n", fn, t);
    last = now;
    log("f %llu st %d zone %08X cam %.2f %.2f %.2f tasks %zu node %d walk %d speed %.2f\n",
        (unsigned long long)g_frame_no, rd8(STAGE), rd32(ZONE), rdf(CAMERA_POS), rdf(CAMERA_POS + 4),
        rdf(CAMERA_POS + 8), now.size(), rd32(CAM_NODE), rd32(CAM_WALK), rdf(CAM_SPEED));
    {   // encounter machine
        uint32_t pc = rd32(VM_PC);
        static uint32_t last_pc = ~0u;
        if (pc != last_pc) {
            last_pc = pc;
            log("  vm pc %u op %d flags %X alive %d/%d wait %d look %.1f %.1f %.1f\n", pc,
                int32_t(rd32(script_addr(pc))), rd32(VM_FLAGS), rd32(VM_ALIVE), rd32(VM_ALIVE2), rd32(VM_WAIT),
                rdf(LOOK_AT), rdf(LOOK_AT + 4), rdf(LOOK_AT + 8));
        }
    }
    if (g_frame_no % 60 == 0) {
        std::map<uint32_t, int> by_fn;
        for (auto& [t, fn] : now) by_fn[fn]++;
        log("  tasks:");
        for (auto& [fn, n] : by_fn) log(" %X*%d", fn, n);
        log("\n");
        fflush(g_log);
    }
    // RAM snapshots for offline searches: LW_SURV_RAM=FROM,TO,EVERY (frames)
    static unsigned long long ram_from = 0, ram_to = 0, ram_every = 0;
    static FILE* ram = nullptr;
    if (const char* e = getenv("LW_SURV_RAM"); e && !ram_every) {
        if (sscanf(e, "%llu,%llu,%llu", &ram_from, &ram_to, &ram_every) != 3) ram_from = 0, ram_to = ~0ull, ram_every = 60;
        ram = fopen("out/survival_ram.bin", "wb");
    }
    if (ram && g_frame_no >= ram_from && g_frame_no <= ram_to && g_frame_no % ram_every == 0) {
        uint32_t hdr[2] = {uint32_t(g_frame_no), 0};
        fwrite(hdr, 4, 2, ram);
        fwrite(g_ram + 0x130000, 1, 0xA0000, ram);
    }
    return hold;
}
