#include "r3d_screen.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "hud_split.h"

// The cutscene letterbox is two flat black quads drawn in an overlay viewport
// (higher priority than the scene) across the top and bottom of the screen.
// They are collapsed to nothing so the scene shows through, which also lets the
// widescreen view use the whole window height.
// Screen bounding box of a triangle in native pixels: x0, y0, x1, y1
static bool screen_bbox(const R3DViewport& v, const R3DVertex* tri, float* b) {
    b[0] = b[1] = 1e9f;
    b[2] = b[3] = -1e9f;
    const float* m = v.proj;
    for (int k = 0; k < 3; k++) {
        const float* p = tri[k].pos;
        float cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
        if (cw <= 1e-6f) return false;
        float sx = v.x + ((m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / cw + 1) / 2 * v.w;
        float sy = v.y + ((m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / cw + 1) / 2 * v.h;
        b[0] = std::min(b[0], sx), b[2] = std::max(b[2], sx), b[1] = std::min(b[1], sy), b[3] = std::max(b[3], sy);
    }
    return true;
}

// The ammo counter is 3D geometry in an overlay viewport, drawn inside the
// left (or right) margin of the 4:3 screen. Those triangles are tagged so the
// vertex shader moves them out to the edges of a widescreen view. The gun
// sight lives in the same viewport but much closer to the camera (z = 1), and
// must stay where the gun points, so near geometry is left alone.
void r3d_anchor_hud(R3DScene& out) {
    if (out.viewports.empty()) return;
    int scene_priority = out.viewports.front().priority;
    const float band = 72.0f;   // native pixels from each side of the 4:3 screen
    for (const R3DViewport& v : out.viewports) {
        if (v.priority == scene_priority) continue;
        for (int pass = 0; pass < 2; pass++) {
            uint32_t first = pass ? v.trans_first : v.opaque_first, count = pass ? v.trans_count : v.opaque_count;
            for (uint32_t t = 0; t + 2 < count; t += 3) {
                R3DVertex* tri = &out.vertices[first + t];
                float b[4];
                if (-tri[0].pos[2] < 10.0f || !screen_bbox(v, tri, b)) continue;
                uint32_t flag = b[2] < v.x + band ? R3D_HUD_LEFT : b[0] > v.x + v.w - band ? R3D_HUD_RIGHT : 0;
                for (int k = 0; k < 3; k++) tri[k].flags |= flag;
            }
        }
    }
    // The players' prompts drawn in 3D (hud_split.h): a group wholly on one
    // side of the middle and wider than a hit's points goes out by half the
    // side band, to the middle of that player's half of the wide picture
    for (const R3DViewport& v : out.viewports) {
        if (v.priority == scene_priority || v.w <= 0) continue;
        std::vector<hud_split::Box> boxes;
        std::vector<R3DVertex*> tris;
        for (int pass = 0; pass < 2; pass++) {
            uint32_t first = pass ? v.trans_first : v.opaque_first, count = pass ? v.trans_count : v.opaque_count;
            for (uint32_t t = 0; t + 2 < count && tris.size() < 4096; t += 3) {
                R3DVertex* tri = &out.vertices[first + t];
                float b[4];
                if (-tri[0].pos[2] < 10.0f || (tri[0].flags & (R3D_HUD_LEFT | R3D_HUD_RIGHT)) || !screen_bbox(v, tri, b))
                    continue;
                float k = 496.0f / v.w;   // the 4:3 screen's 2D pixels
                hud_split::Box box{int((b[0] - v.x) * k), int(b[1] - v.y), int((b[2] - v.x) * k), int(b[3] - v.y)};
                if (box.x1 < 0 || box.x0 >= 496 || box.y1 < 0 || box.y0 >= 384) continue;
                boxes.push_back(box);
                tris.push_back(tri);
            }
        }
        std::vector<int> side = hud_split::classify(boxes);
        for (size_t i = 0; i < tris.size(); i++) {
            if (!side[i]) continue;
            uint32_t flag = R3D_HUD_HALF | (side[i] < 0 ? R3D_HUD_LEFT : R3D_HUD_RIGHT);
            for (int k = 0; k < 3; k++) tris[i][k].flags |= flag;
        }
    }
}

void r3d_strip_letterbox(R3DScene& out) {
    if (out.viewports.empty()) return;
    int scene_priority = out.viewports.front().priority;
    for (const R3DViewport& v : out.viewports) {
        if (v.priority == scene_priority) continue;
        for (uint32_t t = 0; t + 2 < v.opaque_count; t += 3) {
            R3DVertex* tri = &out.vertices[v.opaque_first + t];
            if ((tri[0].color & 0xFFFFFF) != 0 || (tri[0].flags & R3D_TEXTURED)) continue;
            float b[4];
            if (!screen_bbox(v, tri, b)) continue;
            float x0 = b[0], y0 = b[1], x1 = b[2], y1 = b[3];
            bool wide = x1 - x0 >= 0.9f * v.w;
            bool band = y1 - y0 < 0.5f * v.h;   // full-screen fades are kept
            bool edge = y0 <= v.y + 1 || y1 >= v.y + v.h - 1;
            if (!(wide && band && edge)) continue;
            for (int k = 1; k < 3; k++) memcpy(tri[k].pos, tri[0].pos, sizeof(tri[0].pos));
        }
    }
}
