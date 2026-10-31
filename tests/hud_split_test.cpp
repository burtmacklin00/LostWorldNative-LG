// hud_split: which 2D drawing moves out with each player's half in widescreen
#include "hud_split.h"

#include <cstdio>
#include <vector>

using namespace hud_split;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

struct Screen {
    std::vector<uint8_t> px = std::vector<uint8_t>(SCREEN_W * SCREEN_H, 0);
    void fill(int x0, int y0, int x1, int y1) {   // inclusive
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) px[y * SCREEN_W + x] = 1;
    }
    // Letters: blocks with gaps, like text
    void text(int x0, int y0, int w, int h) {
        for (int x = x0; x < x0 + w; x += 10) fill(x, y0, std::min(x + 7, x0 + w - 1), y0 + h - 1);
    }
    std::vector<uint32_t> rows() const {
        std::vector<uint32_t> r(SCREEN_H);
        rows_2d([&](int x, int y) { return px[y * SCREEN_W + x] != 0; }, r.data());
        return r;
    }
};

static int l0(uint32_t t) { return int(t >> 16 & 255) * 2; }
static int l1(uint32_t t) { return int(t >> 8 & 255) * 2; }
static int r0(uint32_t t) { return int(t & 255) * 2; }
static int r1(uint32_t t) { return int(t >> 24 & 255) * 2; }
static bool moves_left(uint32_t t, int x) { return x >= l0(t) && x < l1(t); }
static bool moves_right(uint32_t t, int x) { return x >= r0(t) && x < r1(t); }

int main() {
    {   // Both players' RELOAD with its three-line hint, the lives, CREDITS and a hit's points
        Screen s;
        s.text(40, 200, 170, 24);    // P1 RELOAD
        s.text(60, 230, 130, 8);     // SHOOT OUTSIDE
        s.text(115, 240, 16, 8);     // of (narrow, but part of the prompt)
        s.text(70, 250, 110, 8);     // THE SCREEN!
        s.text(286, 200, 170, 24);   // P2 RELOAD
        s.text(306, 230, 130, 8);
        s.text(361, 240, 16, 8);
        s.text(316, 250, 110, 8);
        s.fill(100, 340, 124, 360);  // P1 lives: two medkits
        s.fill(134, 340, 158, 360);
        s.fill(330, 340, 354, 360);  // P2 lives
        s.fill(364, 340, 388, 360);
        s.text(205, 365, 90, 10);    // CREDITS 0, across the middle
        s.text(60, 100, 24, 10);     // a hit's points (200), left side
        s.fill(30, 12, 120, 28);     // the score band, not ours
        std::vector<uint32_t> r = s.rows();
        check(moves_left(r[210], 40) && moves_left(r[210], 209), "P1 RELOAD moves left");
        check(moves_right(r[210], 286) && moves_right(r[210], 455), "P2 RELOAD moves right");
        check(moves_left(r[244], 115), "the narrow 'of' moves with its prompt");
        check(moves_left(r[350], 100) && moves_right(r[350], 388), "the lives move with their player");
        check(!moves_left(r[368], 205) && !moves_right(r[368], 294), "CREDITS stays");
        check(r[104] == 0, "a hit's points stay");
        check(r[20] == 0, "the score band is left to its own handling");
        check(l1(r[210]) <= MIDDLE_X0 && r0(r[210]) >= MIDDLE_X1, "nothing moves across the middle");
    }
    {   // A shot's popup under the scores (1 ECHO, 2 LONG RANGE): number and word stay together
        Screen s;
        s.text(140, 70, 8, 12);      // P1's number
        s.text(165, 70, 60, 12);     // ECHO, wide enough to be a prompt
        s.text(350, 70, 8, 12);      // P2's
        s.text(375, 70, 100, 12);    // LONG RANGE
        std::vector<uint32_t> r = s.rows();
        check(r[75] == 0, "a shot's popup stays where the game puts it");
    }
    {   // 2 players: CONTINUE! on player 2's side and its countdown 13 rows under it move together
        Screen s;
        s.text(336, 227, 112, 19);   // CONTINUE!
        s.fill(389, 259, 410, 284);  // 9
        auto r = s.rows();
        check(moves_right(r[236], 340) && moves_right(r[270], 400), "countdown follows CONTINUE!");
        Screen p;   // a narrow number alone stays (a hit's points)
        p.fill(389, 259, 410, 284);
        check(!moves_right(p.rows()[270], 400), "a lone number stays");
    }
    {   // A message across the middle (STAGE CLEAR) stays whole
        Screen s;
        s.text(150, 180, 200, 20);
        std::vector<uint32_t> r = s.rows();
        check(r[190] == 0, "a centred message stays");
    }
    {   // Only player 2 playing: only their prompt moves
        Screen s;
        s.text(286, 200, 170, 24);
        std::vector<uint32_t> r = s.rows();
        check(l1(r[210]) <= l0(r[210]) && moves_right(r[210], 300), "one player's prompt alone");
    }
    {   // A full-screen picture: nothing moves
        Screen s;
        for (int y = 40; y < 380; y++)
            for (int x = 0; x < SCREEN_W; x += 2) s.px[y * SCREEN_W + x] = 1;
        std::vector<uint32_t> r = s.rows();
        bool none = true;
        for (uint32_t t : r) none &= t == 0;
        check(none, "a full-screen picture stays");
    }
    {   // 3D boxes: grouped the same way
        std::vector<Box> b = {{40, 200, 120, 224}, {125, 200, 209, 224}, {300, 200, 455, 224}, {200, 300, 300, 310}, {60, 100, 80, 108}};
        std::vector<int> side = classify(b);
        check(side[0] == -1 && side[1] == -1, "3D: left prompt");
        check(side[2] == 1, "3D: right prompt");
        check(side[3] == 0 && side[4] == 0, "3D: centred and small stay");
    }
    printf(failures ? "%d failure(s)\n" : "hud_split: all passed\n", failures);
    return failures ? 1 : 0;
}
