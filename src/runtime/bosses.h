// Boss mode: the bosses of the game and where each fight starts.
//
// A fight is started from a game state saved just before it (runtime.cpp,
// "Game states"). LostWorld.exe --make-boss-state N plays the boss's stage by
// itself, without a window and at full speed, and saves that state in
// save/boss/bossN.state the moment the stage script reaches the fight.
#pragma once

struct BossInfo {
    const wchar_t* name;    // as on the game's name plate
    const wchar_t* stage;   // shown in the menu
    int stage_index;        // position in the run (0x1A623C): 0..4
    // Stage script (words at 0x1268B8, read position 0x1A847C, scene label
    // 0x1A841C): the fight starts when the script of scene `label` reaches
    // word `index`
    int label, index;
};

inline constexpr BossInfo BOSSES[] = {
    {L"T.REX FEMALE", L"STAGE 1", 0, 10, 1972},   // the T-Rex comes out of the jungle
    {L"DEINOSUCHUS", L"STAGE 2", 1, 17, 3579},    // the lakeside, once the raptors are down
    {L"CARNOTAURUS", L"STAGE 4", 3, 44, 5045},    // the clearing, after the last raptors
    {L"T.REX", L"STAGE 5", 4, 49, 5046},          // the trailer, last fight of the game
};
constexpr int BOSS_COUNT = int(sizeof(BOSSES) / sizeof(BOSSES[0]));

// Game states (runtime.cpp). A load waits for the game's main loop and
// returns at once: game_state_result() says 0 pending, 1 done, -1 failed.
#include <string>
void game_state_save(const std::string& path);
bool game_state_load(const std::string& path, bool keep_home = false);
bool game_state_load_home();
void game_state_keep_home();   // the game as it is becomes the home state (if there is none yet)
bool game_state_has_home();
void game_state_mark();        // the game as it is becomes the mark (survival: the start of a lap)
bool game_state_load_mark();
int game_state_result();
bool game_state_valid(const std::string& path);
inline std::string boss_state_path(int boss) { return "save/boss/boss" + std::to_string(boss + 1) + ".state"; }
