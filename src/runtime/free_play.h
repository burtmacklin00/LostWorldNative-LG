// The arcade's own FREE PLAY coin setting, in RAM only (the EEPROM is not
// touched): with it on, the game shows FREE PLAY instead of CREDIT 0, starts
// and continues with START alone and never counts credits, as on a cabinet
// set to free play in the test menu.
//
// The system code works it out at boot from the coin setting (0x1225 = 0x1A,
// FREE PLAY): 0x12F4 = 0xFF (fn_0010A1C8; read by fn_0010BCEC, the credits,
// and fn_0010C37C, which then takes none), and the game's own copy 0x1A61F0
// = 1 (fn_0002EA48, once at boot; it picks the FREE PLAY lettering).
// After boot both are written each frame while free play is on.
#pragma once

namespace free_play {

void set(bool on);                         // the setting (menu or command line)
bool native();                             // the game is in its FREE PLAY mode now
void frame(unsigned long long game_frame);   // game thread, every frame

}  // namespace free_play
