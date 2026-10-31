// The full-window BGRA picture (premultiplied alpha) that the menus, the
// survival screens and the achievement notices draw on the CPU, and the
// drawing they share. video/ui_overlay.h puts each picture on screen.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "gdi_text.h"

namespace ui {

inline float chan(uint32_t rgb, int shift) { return float((rgb >> shift) & 0xFF) / 255.0f; }

// Between two colours (0xRRGGBB), t 0..1
inline uint32_t mix(uint32_t a, uint32_t b, float t) {
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        float v = chan(a, s) + (chan(b, s) - chan(a, s)) * t;
        r |= uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f) << s;
    }
    return r;
}

// Integer "over" on premultiplied BGRA (it runs for every pixel of panels and text); a: 0..256
inline uint32_t over(uint32_t d, uint32_t rgb, int a) {
    int k = 256 - a;
    uint32_t rb = (((rgb & 0xFF00FFu) * a + (d & 0xFF00FFu) * k) >> 8) & 0xFF00FFu;
    uint32_t g = (((rgb & 0x00FF00u) * a + (d & 0x00FF00u) * k) >> 8) & 0x00FF00u;
    uint32_t dst_a = d >> 24;
    // 253 / 254: the backdrop's logo / jungle (ui.frag animates those);
    // anything drawn over them is the menu's own, left still (255)
    uint32_t da = dst_a == 253 || dst_a == 254 ? 255u : ((255u * a + dst_a * k) >> 8);
    return rb | g | (da << 24);
}

}  // namespace ui

class UiCanvas {
protected:
    uint32_t* px_ = nullptr;
    int w_ = 0, h_ = 0;

    void blend(int x, int y, uint32_t rgb, float a) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_ || a <= 0) return;
        uint32_t& d = px_[size_t(y) * w_ + x];
        d = ui::over(d, rgb, int(std::min(a, 1.0f) * 256.0f + 0.5f));
    }
    void fill(int x0, int y0, int x1, int y1, uint32_t rgb, float alpha) {
        x0 = std::max(x0, 0), y0 = std::max(y0, 0), x1 = std::min(x1, w_), y1 = std::min(y1, h_);
        int a = int(std::clamp(alpha, 0.0f, 1.0f) * 256.0f + 0.5f);
        for (int y = y0; y < y1; y++) {
            uint32_t* row = px_ + size_t(y) * w_;
            for (int x = x0; x < x1; x++) row[x] = ui::over(row[x], rgb, a);
        }
    }
    // A text with the top of its box at y (align 0 left of x, 1 centred, 2
    // right), from the top colour to the bottom one, over a dark drop shadow.
    // Returns its width
    int shadowed_text(const GdiText::Mask& m, int px, int x, int y, int align, uint32_t top, uint32_t bottom,
                      float alpha) {
        int x0 = x - (align == 1 ? m.text_w / 2 : align == 2 ? m.text_w : 0) - m.pad;
        int y0 = y - m.pad;
        int sh = std::max(1, px / 18);   // drop shadow
        for (int j = 0; j < m.h; j++)
            for (int i = 0; i < m.w; i++)
                if (uint8_t c = m.a[size_t(j) * m.w + i])
                    blend(x0 + i + sh, y0 + j + sh, 0x000000, float(c) / 255.0f * 0.8f * alpha);
        for (int j = 0; j < m.h; j++) {
            uint32_t row = ui::mix(top, bottom, std::clamp(float(j - m.pad) / float(std::max(1, m.ascent)), 0.0f, 1.0f));
            for (int i = 0; i < m.w; i++)
                if (uint8_t c = m.a[size_t(j) * m.w + i]) blend(x0 + i, y0 + j, row, float(c) / 255.0f * alpha);
        }
        return m.text_w;
    }
};
