#include "hud_split.h"

#include <algorithm>
#include <numeric>

namespace hud_split {

static int find(std::vector<int>& parent, int i) {
    while (parent[i] != i) i = parent[i] = parent[parent[i]];
    return i;
}

std::vector<int> classify(const std::vector<Box>& boxes) {
    const int n = int(boxes.size());
    std::vector<int> parent(n), order(n);
    std::iota(parent.begin(), parent.end(), 0);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return boxes[a].y0 < boxes[b].y0; });
    // Sweep down the screen: each box joins those above it that are still
    // within JOIN_Y rows and overlap it (give or take JOIN_X columns)
    std::vector<int> active;
    for (int i : order) {
        const Box& b = boxes[i];
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&](int a) { return boxes[a].y1 < b.y0 - JOIN_Y; }),
                     active.end());
        for (int a : active) {
            const Box& o = boxes[a];
            if (o.x0 <= b.x1 + JOIN_X && b.x0 <= o.x1 + JOIN_X) parent[find(parent, a)] = find(parent, i);
        }
        active.push_back(i);
    }
    // Each group's extent decides its side
    std::vector<Box> group(n, Box{1 << 30, 1 << 30, -(1 << 30), -(1 << 30)});
    for (int i = 0; i < n; i++) {
        Box& g = group[find(parent, i)];
        const Box& b = boxes[i];
        g.x0 = std::min(g.x0, b.x0), g.y0 = std::min(g.y0, b.y0);
        g.x1 = std::max(g.x1, b.x1), g.y1 = std::max(g.y1, b.y1);
    }
    // Each group's side: wide enough, below the popups, on one side of the middle
    std::vector<int> group_side(n, 0);
    for (int r = 0; r < n; r++) {
        if (find(parent, r) != r) continue;
        const Box& g = group[r];
        if (g.x1 - g.x0 + 1 < (g.y0 >= LIVES_ROW ? MIN_WIDTH_LIVES : MIN_WIDTH)) continue;
        if (g.y1 < POPUP_ROWS) continue;   // a shot's popup under the scores: where the game puts it
        group_side[r] = g.x1 < MIDDLE_X0 ? -1 : g.x0 >= MIDDLE_X1 ? 1 : 0;
    }
    // A narrow group just under a moving one and centred within it (the
    // countdown under CONTINUE!, 2 players) goes with it, staying centred
    for (int r = 0; r < n; r++) {
        if (find(parent, r) != r || group_side[r] != 0) continue;
        const Box& g = group[r];
        if (g.y0 < POPUP_ROWS || g.y0 >= LIVES_ROW) continue;
        int cx = (g.x0 + g.x1) / 2;
        for (int a = 0; a < n; a++) {
            if (a == r || find(parent, a) != a || group_side[a] == 0) continue;
            const Box& o = group[a];
            if (o.y1 < g.y0 && g.y0 - o.y1 <= FOLLOW_GAP && cx >= o.x0 && cx <= o.x1 && g.x1 - g.x0 < o.x1 - o.x0) {
                group_side[r] = group_side[a];
                break;
            }
        }
    }
    std::vector<int> side(n, 0);
    for (int i = 0; i < n; i++) side[i] = group_side[find(parent, i)];
    return side;
}

void rows_from_boxes(const std::vector<Box>& boxes, const std::vector<int>& side, uint32_t* out_rows) {
    // Per row, the columns of each side's moving drawing (a little wider, so
    // the smoothing filter keeps its neighbours), never across the middle
    int lo[2][SCREEN_H], hi[2][SCREEN_H];
    for (int s = 0; s < 2; s++)
        for (int y = 0; y < SCREEN_H; y++) lo[s][y] = SCREEN_W, hi[s][y] = 0;
    for (size_t i = 0; i < boxes.size(); i++) {
        if (!side[i]) continue;
        const Box& b = boxes[i];
        int s = side[i] > 0;
        int x0 = std::max(b.x0 - 2, s ? MIDDLE_X1 : 0), x1 = std::min(b.x1 + 3, s ? SCREEN_W : MIDDLE_X0);
        for (int y = std::max(b.y0, TOP_ROWS); y <= std::min(b.y1, SCREEN_H - 1); y++)
            lo[s][y] = std::min(lo[s][y], x0), hi[s][y] = std::max(hi[s][y], x1);
    }
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t texel = 0;
        for (int s = 0; s < 2; s++) {
            if (y < TOP_ROWS || hi[s][y] <= lo[s][y]) continue;
            uint32_t a = uint32_t(lo[s][y] / 2), b = uint32_t((hi[s][y] + 1) / 2);
            // BGRA texel (memory order B, G, R, A): R = l0, G = l1, B = r0, A = r1
            texel |= s ? (a | b << 24) : (a << 16 | b << 8);
        }
        out_rows[y] = texel;
    }
}

}   // namespace hud_split
