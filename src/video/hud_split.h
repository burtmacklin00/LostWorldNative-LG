// Widescreen: each player's prompts (RELOAD / SHOOT OUTSIDE OF THE SCREEN!,
// the lives, CONTINUE, INSERT COIN) are drawn by the game centred in that
// player's half of the 4:3 screen. On a wider window they are moved out by
// half the side band, so they stay centred in that player's half of the whole
// picture, as they are on the 4:3 screen.
//
// What moves is found by shape, the same way for the 2D layer and for the 3D
// HUD: a group of drawing that stays wholly on one side of the screen's
// middle and is wider than the points the game pops up where an enemy was hit
// (those stay where the game puts them). Anything across the middle (CREDITS,
// STAGE CLEAR, the results, a boss's name) stays where it is.
#pragma once
#include <cstdint>
#include <vector>

namespace hud_split {

constexpr int SCREEN_W = 496, SCREEN_H = 384;
constexpr int TOP_ROWS = 32;            // compose.frag HUD_TOP_ROWS: the score band, handled on its own
constexpr int MIDDLE_X0 = 244, MIDDLE_X1 = 252;   // a group touching these columns belongs to both players
constexpr int MIN_WIDTH = 48;           // narrower groups are the points popped up at a hit
// Above this row (under the scores) the game pops up what a shot was worth:
// a player's number and a word (HIT!, ECHO, LONG RANGE, SNIPE SHOT!, BINGO!!)
// drawn apart, the word flashing. They stay where the game puts them: moving
// only the word would pull it off its number. The prompts are all lower
// (SHOOT OUTSIDE OF THE SCREEN! from row ~200, RELOAD, CONTINUE, the lives)
constexpr int POPUP_ROWS = 120;
constexpr int LIVES_ROW = 320;          // the lives row at the bottom: a single heart (32 px) still moves
constexpr int MIN_WIDTH_LIVES = 16;
constexpr int JOIN_X = 16, JOIN_Y = 8;  // gaps that still make one group (letters, lines of one prompt)

struct Box { int x0, y0, x1, y1; };     // inclusive, 2D pixels of the 4:3 screen

// Which side each box moves to: -1 left, +1 right, 0 stays. Boxes that touch
// (within JOIN_X / JOIN_Y) form one group and move together.
std::vector<int> classify(const std::vector<Box>& boxes);

// The 2D foreground: `solid(x, y)` says whether that pixel holds drawing.
// Per row from TOP_ROWS on, the source columns that move left [l0, l1) and
// right [r0, r1), packed for the GPU (sky_coords' last row, see compose.frag):
// one BGRA texel per row, R = l0 / 2, G = l1 / 2, B = r0 / 2, A = r1 / 2
// (empty ranges are 0, 0). Rows above TOP_ROWS get 0.
template <class Solid>
void rows_2d(Solid solid, uint32_t* out_rows);

// ---- implementation of the template ----------------------------------------

void rows_from_boxes(const std::vector<Box>& boxes, const std::vector<int>& side, uint32_t* out_rows);

template <class Solid>
void rows_2d(Solid solid, uint32_t* out_rows) {
    // Row runs of drawing, merged into boxes per band of rows
    std::vector<Box> boxes;
    for (int y = TOP_ROWS; y < SCREEN_H; y++) {
        int run = -1;
        for (int x = 0; x <= SCREEN_W; x++) {
            bool on = x < SCREEN_W && solid(x, y);
            if (on && run < 0) run = x;
            if (!on && run >= 0) {
                boxes.push_back({run, y, x - 1, y});
                run = -1;
            }
        }
        if (boxes.size() > 20000) break;   // a full-screen picture: nothing to move
    }
    if (boxes.size() > 20000) {
        for (int y = 0; y < SCREEN_H; y++) out_rows[y] = 0;
        return;
    }
    rows_from_boxes(boxes, classify(boxes), out_rows);
}

}   // namespace hud_split
