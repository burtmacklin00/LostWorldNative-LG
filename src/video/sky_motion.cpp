#include "sky_motion.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr double HALF = 0.501;         // the true position is within this of the integer written (rounding)
constexpr double PHASE_CAMERA = 1.0;   // ...and the drawn one within this of it while the camera leads
constexpr double CAMERA_TOLERANCE = 2.0;   // px/frame the camera may be outside the corridor's velocities
constexpr int CUT = 40;                // a larger step in one frame is a cut: the history starts again
constexpr int WRAP = 512;              // the layers' pictures wrap every 512 pixels

inline int wrap(int d) { return ((d + WRAP / 2) & (WRAP - 1)) - WRAP / 2; }

}   // namespace

void SkyMotion::reset(Track& t) {
    t.n = 0;
    t.drawn = false;
}

void SkyMotion::feed(uint64_t frame, const int32_t* back_rows) {
    bool again = frame == frame_;   // the same frame composed again (a change of settings)
    bool jump = frame_ != ~0ull && (frame < frame_ || frame > frame_ + 8);   // a saved state, a new scene
    for (int y = 0; y < ROWS; y++) {
        Track& t = rows_[y];
        int layer = back_rows[3 * y], x = back_rows[3 * y + 1], py = back_rows[3 * y + 2] - y;
        if (again && layer == t.layer && t.n > 0) continue;
        if (layer < 0) {
            t.layer = -1;
            reset(t);
            continue;
        }
        int d[2] = {0, 0};
        if (layer != t.layer || t.n == 0 || jump) {
            t.layer = layer;
            reset(t);
        } else {
            d[0] = wrap(x - t.last[0]);
            d[1] = wrap(py - t.last[1]);
            if (std::abs(d[0]) > CUT || std::abs(d[1]) > CUT) {
                reset(t);
                d[0] = d[1] = 0;
            }
        }
        t.last[0] = x;
        t.last[1] = py;
        if (t.n == HISTORY) {
            memmove(t.frames, t.frames + 1, sizeof(t.frames[0]) * (HISTORY - 1));
            memmove(t.pos, t.pos + 1, sizeof(t.pos[0]) * (HISTORY - 1));
            t.n--;
        }
        t.frames[t.n] = frame;
        for (int i = 0; i < 2; i++) t.pos[t.n][i] = t.n ? t.pos[t.n - 1][i] + d[i] : 0;
        t.n++;
    }
    frame_ = frame;
    pending_ = true;
}

// One row along one axis: the corridor's velocities, the velocity drawn and
// the position at the frame (see the header)
void SkyMotion::solve_axis(Track& t, int axis, bool camera, float cam_v) {
    Row& out = t.out[axis];
    const int n = t.n;
    const uint64_t kn = t.frames[n - 1];
    const double sn = t.pos[n - 1][axis];
    // Velocities with which one line passes within HALF of every sample of
    // the longest run ending at the newest: for each pair of samples i, j
    // (j older than i by dd frames) v * dd <= s_i - s_j + 2 HALF and
    // v * dd >= s_i - s_j - 2 HALF
    double vlo = -1e9, vhi = 1e9;
    int used = 1;   // samples in the run (newest first)
    for (int j = n - 2; j >= 0; j--) {
        double lo = vlo, hi = vhi;
        double dj = double(kn - t.frames[j]), sj = t.pos[j][axis];
        for (int i = n - 1; i > j; i--) {
            double di = double(kn - t.frames[i]), si = t.pos[i][axis];
            double dd = dj - di;
            hi = std::min(hi, (si - sj + 2 * HALF) / dd);
            lo = std::max(lo, (si - sj - 2 * HALF) / dd);
        }
        if (lo > hi) break;
        vlo = lo;
        vhi = hi;
        used++;
    }
    bool corridor = used > 1;
    if (!corridor) vlo = vhi = camera ? cam_v : 0.0;
    // The velocity drawn: the camera's when plausible, else the middle
    double v;
    bool by_camera = camera && std::fabs(cam_v - std::clamp(double(cam_v), vlo, vhi)) <= CAMERA_TOLERANCE;
    if (by_camera) v = cam_v;
    else v = (vlo + vhi) / 2;
    // Where the position at this frame may be with that velocity
    double alo = sn - HALF, ahi = sn + HALF;
    if (corridor && v >= vlo && v <= vhi)
        for (int j = n - used; j < n - 1; j++) {
            double dj = double(kn - t.frames[j]), sj = t.pos[j][axis];
            alo = std::max(alo, sj - HALF + v * dj);
            ahi = std::min(ahi, sj + HALF + v * dj);
        }
    if (by_camera) {   // the game's scroll only keeps the sky from drifting away
        alo = std::min(alo, sn - PHASE_CAMERA);
        ahi = std::max(ahi, sn + PHASE_CAMERA);
    }
    // ...and where it goes on from the previous frame, if that is allowed
    bool cont = t.drawn && t.k < kn && kn - t.k <= HISTORY;
    double from = cont ? t.a[axis] + t.v[axis] * double(kn - t.k) : t.drawn && t.k == kn ? t.a[axis] : sn;
    double a = std::clamp(from, alo, ahi);
    double prev_a = t.a[axis];
    t.a[axis] = a;
    t.v[axis] = v;
    out.known = n >= 2;
    out.step = n >= 2 ? t.pos[n - 1][axis] - t.pos[n - 2][axis] : 0;
    out.move = cont ? float(a - prev_a) : 0.0f;
    out.offset = float(a - sn);
    out.velocity = float(v);
}

void SkyMotion::solve(bool camera, float cam_dx, float cam_dy) {
    pending_ = false;
    const float cam_v[2] = {cam_dx, cam_dy};
    for (int y = 0; y < ROWS; y++) {
        Track& t = rows_[y];
        for (int axis = 0; axis < 2; axis++) {
            uint32_t& texel = texels_[axis * ROWS + y];
            if (t.n == 0) {
                t.out[axis] = Row{};
                texel = 0;
                continue;
            }
            solve_axis(t, axis, camera, cam_v[axis]);
            const Row& o = t.out[axis];
            if (!o.known) {
                texel = 0;
                continue;
            }
            int r = std::clamp(o.step + 128, 1, 255);
            int g = std::clamp(int(std::lround((o.offset / 2 + 0.5f) * 255)), 0, 255);
            int v16 = std::clamp(int(std::lround(o.velocity * 256)) + 32768, 0, 65535);
            texel = uint32_t(v16 & 255) << 24 | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(v16 >> 8);
        }
        if (t.n) {
            t.drawn = true;
            t.k = t.frames[t.n - 1];
        }
    }
}

SkyMotion::Row SkyMotion::row(int y, int axis) const {
    if (y < 0 || y >= ROWS || rows_[y].n == 0) return Row{};
    return rows_[y].out[axis];
}
