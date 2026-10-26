// Records and achievements: what the player has done over all games, saved in
// save/stats.ini, and local achievements (save/achievements.ini) announced
// with a toast and listed in the RECORDS screen of the main menu.
//
// The game thread reads the game's own counters at every vblank (the ones of
// its stage results screen: shots, HITS, TECHNICAL SHOT) and the call that
// scores each hit; see docs/game_ram.md. The window thread tells it about the
// boss fights and reads a copy of everything to draw.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Ctx;

constexpr int STAT_STAGES = 5, STAT_BOSSES = 4, STAT_COURSES = 3;

struct Stats {
    // Totals
    int64_t games = 0, games_finished = 0, play_frames = 0;
    int64_t shots = 0, hits = 0, technical = 0, kills = 0, lives_lost = 0, continues = 0;
    // Per stage (0 = stage 1), whole stages played in arcade mode
    struct Stage {
        int clears = 0, best_score = 0;
        int best_accuracy = -1;   // per mille, -1 none yet
        int best_frames = 0;      // 0 none yet
    } stage[STAT_STAGES];
    // Per boss of runtime/bosses.h, boss mode
    struct Boss {
        int attempts = 0, wins = 0, best_frames = 0, flawless = 0;
    } boss[STAT_BOSSES];
    int best_score[STAT_COURSES] = {};   // per course: Beginner, Expert, Shuffle
    int best_wave = 0;                   // survival mode
    // Best of a single stage or game, for the achievements
    int best_stage_technical = 0;
    int untouched_stages = 0;            // stages cleared without losing a life
    int one_credit_finishes = 0;
    int finished_course[STAT_COURSES] = {};
};

// Achievement table (stats.cpp): progress is read from Stats
struct AchievementInfo {
    const char* key;          // in save/achievements.ini
    const wchar_t* name;
    const wchar_t* text;      // what it asks
    int64_t goal;             // progress needed (1 for yes / no ones)
};
const std::vector<AchievementInfo>& achievement_table();
int64_t achievement_progress(const Stats& s, int index);   // capped at the goal

// Window thread
void stats_init();                  // a normal launch: tracking on, files loaded (tests leave it off)
void stats_boss_begin(int boss);    // a boss fight has just been loaded
void stats_boss_end(bool won);      // that fight is over
struct StatsView {
    Stats stats;
    std::vector<int64_t> unlocked;  // per achievement: unix time, 0 locked
};
StatsView stats_view();
std::vector<int> stats_take_new_achievements();   // unlocked since the last call, for the toast
void stats_save();                  // at exit (it also saves by itself every 30 s of play)

// Game thread
void stats_frame();                 // every vblank
void stats_enter(Ctx& c, uint32_t fn);   // entry of a hooked function
void stats_state_loaded();          // a game state was put back: its counters are not this player's
