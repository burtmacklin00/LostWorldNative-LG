// Several mice at once, through raw input: each one reports on its own, so a
// second mouse (or a light gun that shows up as a mouse) can be player 2's gun
// while the first stays player 1's. Until a second mouse joins, the game uses
// the ordinary pointer and nothing here changes how player 1 plays.
#pragma once
#include <windows.h>

namespace raw_mice {

void init(HWND hwnd);              // registers the window for raw mouse input
void on_input(HRAWINPUT input);    // WM_INPUT

// Each poll: whether a click from a mouse other than player 1's may make it
// player 2's (player 1 plays and player 2 does not), and the window's client
// area, which keeps the mice inside it and confines the pointer while two play
void update(HWND hwnd, bool join_allowed, bool in_game);
bool two_mice();                   // a second mouse is player 2's gun
void reset();                      // back to one mouse for player 1 (the option turned off)

struct Gun {
    POINT pos;          // client pixels
    bool trigger, reload;
    bool clicked;       // the left button went down since the last call
};
Gun gun(int player, HWND hwnd);

}  // namespace raw_mice
