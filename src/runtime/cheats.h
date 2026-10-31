// Cheats from Settings > GAME > AMMO: the standard gun refills by itself when
// it gets to its last bullet (AUTO RELOAD) or never runs out (UNLIMITED). The pickup guns
// are left alone. Records and achievements are not kept while one is on
// (runtime/stats.cpp).
#pragma once

enum class AmmoCheat { Off = 0, AutoReload = 1, Unlimited = 2 };

void cheats_set_ammo(int mode);   // window thread: 0 off, 1 auto reload, 2 unlimited
bool cheats_active();             // any cheat on
void cheats_frame();              // game thread, every vblank
