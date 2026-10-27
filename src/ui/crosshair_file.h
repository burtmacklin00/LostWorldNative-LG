// The players' own crosshair pictures: crosshair.png (player 1, and player 2
// when they have none of their own) and crosshair_p2.png beside
// LostWorld.exe. The renderer draws them centred on the gun instead of the
// built-in rings; the options menu picks them and shows them.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Where player 0 or 1's own picture lives (whether or not it exists)
std::string crosshair_path(int player);

// The picture player 0 or 1 is drawn with (player 2 falls back to
// crosshair.png); false when there is none or it does not load
bool crosshair_load(int player, int& w, int& h, std::vector<uint8_t>& rgba);

// Asks for a PNG with the Windows file dialog and copies it to
// crosshair_path(player); false when cancelled or the file is not a picture.
// The caller then pushes Frontend::Action::CrosshairChanged
bool crosshair_pick(int player);
