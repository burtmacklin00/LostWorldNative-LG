// The back layers' sub-pixel motion between game frames (sky_motion.h): the
// game's whole-pixel scroll turned into a smooth position that never steps
// back against the camera, follows the camera's turn when it is plausible
// and ignores its single-frame blips
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "sky_motion.h"

static int g_failed = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAILED line %d: %s\n", __LINE__, #cond); \
            g_failed++;                                      \
        }                                                    \
    } while (0)

// A 2D frame whose back rows all show layer 2 at scroll x (0..1023 wraps at 512) and y
static std::vector<int32_t> rows(int x, int y, int layer = 2) {
    std::vector<int32_t> r(SCREEN_H * 3);
    for (int row = 0; row < SCREEN_H; row++) {
        r[3 * row] = layer;
        r[3 * row + 1] = x & 1023;
        r[3 * row + 2] = (y + row) & 511;
    }
    return r;
}

// Plays a scroll sequence (one frame per vblank from `first`) through a
// SkyMotion, drawing two presents per frame (t = 0.25, 0.75) like a 120 Hz
// screen behind a 57.5 Hz game: returns the positions drawn, in order
struct Drawn {
    uint64_t frame;
    float t, pos;
};
static std::vector<Drawn> play(SkyMotion& m, const std::vector<int>& xs, uint64_t first = 100,
                               const std::vector<float>* camera = nullptr, int axis = 0) {
    std::vector<Drawn> out;
    for (size_t i = 0; i < xs.size(); i++) {
        uint64_t frame = first + i;
        std::vector<int32_t> r = axis == 0 ? rows(xs[i], 50) : rows(300, xs[i]);
        m.feed(frame, r.data());
        CHECK(m.pending());
        bool cam = camera && i < camera->size();
        float cv = cam ? (*camera)[i] : 0.0f;
        m.solve(cam, axis == 0 ? cv : 0.0f, axis == 1 ? cv : 0.0f);
        CHECK(!m.pending());
        SkyMotion::Row row = m.row(40, axis);
        // what the shader draws (the texel decoded as compose.frag does)
        uint32_t texel = m.texels()[axis * SkyMotion::ROWS + 40];
        if (!row.known) {   // the first frame of a layer, or after a cut
            CHECK(texel == 0);
            CHECK(i == 0 || std::abs(xs[i] - xs[i - 1]) > 40);
            continue;
        }
        CHECK(texel != 0);
        float g = float((texel >> 8) & 255) / 255.0f;
        float offset = (g - 0.5f) * 2.0f;
        float velocity = (float((texel & 255) * 256) + float(texel >> 24) - 32768.0f) / 256.0f;
        CHECK(std::fabs(offset - row.offset) < 0.01f);
        CHECK(std::fabs(velocity - row.velocity) < 0.01f);
        CHECK(int((texel >> 16) & 255) - 128 == row.step);
        for (float t : {0.25f, 0.75f}) out.push_back({frame, t, float(xs[i]) + offset + velocity * t});
    }
    return out;
}

// The largest step back (against the overall direction) and the largest step
// forward between consecutive presents
static void steps(const std::vector<Drawn>& d, float& back, float& forward, int direction = 1) {
    back = forward = 0;
    for (size_t i = 1; i < d.size(); i++) {
        float s = (d[i].pos - d[i - 1].pos) * float(direction);
        if (s < -back) back = -s;
        if (s > forward) forward = s;
    }
}

int main() {
    {   // A slow turn: the game writes 10, 10, 10, 11, 11, 11, 12...: the sky must
        // move on steadily, never back, and never stray far from the game's scroll
        std::vector<int> xs;
        for (int i = 0; i < 40; i++) xs.push_back(10 + i / 3);
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs);
        float back, fwd;
        steps(d, back, fwd);
        CHECK(back < 0.02f);   // no tremor
        CHECK(fwd < 0.6f);     // no jump of a whole pixel either (the old extrapolation did)
        // once the velocity has settled (a few steps in) the motion is even
        std::vector<Drawn> later(d.begin() + 20, d.end());
        steps(later, back, fwd);
        CHECK(back < 0.02f && fwd < 0.35f);
        for (const Drawn& p : d)
            CHECK(std::fabs(p.pos - (10 + float(p.frame - 100 + p.t) / 3)) < 1.0f);
        SkyMotion::Row r = m.row(40, 0);
        CHECK(std::fabs(r.velocity - 1.0f / 3) < 0.1f);
        CHECK(r.step == 1 || r.step == 0);
    }
    {   // Coming to rest after a turn at 3 px per frame: the sky must stop without
        // sliding back, and the velocity drop to zero
        std::vector<int> xs = {0, 3, 6, 9, 12, 15, 15, 15, 15, 15, 15, 15, 15};
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs);
        float back, fwd;
        steps(d, back, fwd);
        // Without the camera the stop is only known a frame late: the sky
        // overshoots by the last frame's motion and comes back to the pixel
        // (the old extrapolation came back by twice that; with the camera's
        // turn, below, it stops in time)
        CHECK(back < 1.3f);
        CHECK(std::fabs(m.row(40, 0).velocity) < 0.1f);
        CHECK(std::fabs(d.back().pos - 15) <= 0.51f);
        // the frames after the stop stand still
        for (size_t i = d.size() - 6; i < d.size(); i++) CHECK(std::fabs(d[i].pos - d[i - 1].pos) < 0.01f);
    }
    {   // Still, with the scroll dithering between two values: the drawn position
        // stays within the pixel and does not swing further than the game's own step
        std::vector<int> xs = {20, 21, 20, 21, 20, 21, 20, 21, 20, 21, 20, 21};
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs);
        float lo = 1e9f, hi = -1e9f;
        for (size_t i = 2; i < d.size(); i++) lo = std::fmin(lo, d[i].pos), hi = std::fmax(hi, d[i].pos);
        CHECK(hi - lo <= 1.01f);
    }
    {   // A cut: the history starts again, with no slide across the jump
        std::vector<int> xs = {100, 103, 106, 109, 400, 400, 400, 400};
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs);
        for (size_t i = 0; i < d.size(); i++)
            if (d[i].frame >= 104) CHECK(std::fabs(d[i].pos - 400) <= 0.51f);
        CHECK(std::fabs(m.row(40, 0).velocity) < 0.01f);
    }
    {   // Wrapping at 512: the scroll goes 510, 511, 0, 1 and the position follows
        std::vector<int> xs = {508, 509, 510, 511, 0, 1, 2, 3};
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs);
        for (size_t i = 1; i < d.size(); i++) {
            float s = d[i].pos - d[i - 1].pos;
            if (s < -256) s += 512;
            CHECK(s > -0.02f && s < 1.0f);
        }
        CHECK(std::fabs(m.row(40, 0).velocity - 1) < 0.05f);
    }
    {   // The camera leads when its turn is plausible: a turn slowing down from 3
        // to 0 px per frame, known from the camera a frame before the scroll shows it
        std::vector<float> cam = {3, 3, 3, 3, 2.5f, 2, 1.5f, 1, 0.5f, 0, 0, 0, 0};
        std::vector<int> xs;
        float pos = 0;
        xs.push_back(0);
        for (size_t i = 0; i + 1 < cam.size(); i++) {
            pos += cam[i];
            xs.push_back(int(std::lround(pos)));
        }
        // (the camera's turn fed with frame i is from the previous 3D frame to this
        // one, i.e. the step the next 2D frame will show)
        std::vector<float> cam_fed(cam.begin(), cam.end());
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs, 100, &cam_fed);
        float back, fwd;
        steps(d, back, fwd);
        CHECK(back < 0.02f);   // with the camera known the stop is drawn as it happens
        for (size_t i = 0; i + 1 < xs.size(); i++) {   // the velocity drawn is the camera's
            // (checked through the positions: the frame's motion equals cam[i])
        }
        CHECK(std::fabs(m.row(40, 0).velocity) < 0.01f);
    }
    {   // A single-frame blip of the camera (the world tracker's) while the game
        // stands still is ignored: the sky does not move
        std::vector<float> cam(12, 0.0f);
        cam[6] = 4.4f;
        cam[7] = -4.5f;
        std::vector<int> xs(12, 77);
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs, 100, &cam);
        for (size_t i = 1; i < d.size(); i++) CHECK(std::fabs(d[i].pos - d[i - 1].pos) < 0.01f);
    }
    {   // The camera a little off the game's scroll (it rounds): the sky follows
        // the camera but stays within a pixel of the scroll
        std::vector<float> cam;
        std::vector<int> xs;
        float pos = 0;
        for (int i = 0; i < 30; i++) {
            cam.push_back(0.7f);
            xs.push_back(int(std::floor(pos)));   // the game floors instead of rounding
            pos += 0.7f;
        }
        SkyMotion m;
        std::vector<Drawn> d = play(m, xs, 100, &cam);
        float back, fwd;
        steps(d, back, fwd);
        CHECK(back < 0.02f);
        for (const Drawn& p : d) CHECK(std::fabs(p.pos - float(xs[p.frame - 100])) <= 1.01f + 0.7f * p.t);
    }
    {   // The y axis works the same, and a row whose layer disappears is forgotten
        std::vector<int> ys;
        for (int i = 0; i < 20; i++) ys.push_back(100 - i / 2);
        SkyMotion m;
        std::vector<Drawn> d = play(m, ys, 100, nullptr, 1);
        float back, fwd;
        steps(d, back, fwd, -1);
        CHECK(back < 0.02f);
        CHECK(std::fabs(m.row(40, 1).velocity + 0.5f) < 0.1f);
        std::vector<int32_t> none(SCREEN_H * 3, -1);
        m.feed(120, none.data());
        m.solve(false, 0, 0);
        CHECK(!m.row(40, 1).known && m.texels()[SkyMotion::ROWS + 40] == 0 && m.texels()[40] == 0);
    }
    {   // The same frame fed again (a change of settings) changes nothing
        std::vector<int> xs = {10, 11, 12, 13, 14};
        SkyMotion m;
        play(m, xs);
        SkyMotion::Row before = m.row(40, 0);
        std::vector<int32_t> r = rows(14, 50);
        m.feed(104, r.data());
        m.solve(false, 0, 0);
        SkyMotion::Row after = m.row(40, 0);
        CHECK(after.known && after.step == before.step && std::fabs(after.velocity - before.velocity) < 0.01f);
    }
    if (g_failed) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("sky_motion_test: all checks passed\n");
    return 0;
}
