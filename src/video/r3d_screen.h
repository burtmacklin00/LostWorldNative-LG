// Screen-space heuristics applied to a built scene: geometry the game draws
// over the 3D image that a widescreen window treats differently.
#pragma once
#include "r3d_scene.h"

// Collapses the black cinematic bars the game draws over the scene
void r3d_strip_letterbox(R3DScene& out);
// Marks the 3D HUD elements at the sides of the screen (ammo) so the
// renderer can move them to the edges of a widescreen window
// split_prompts: also move the players' prompts to the middle of each player's
// half of a wide picture (hud_split.h); only while someone plays, so the
// attract mode, the ranking, game over and the name entry never move
void r3d_anchor_hud(R3DScene& out, bool split_prompts);
