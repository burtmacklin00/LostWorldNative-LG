// High-resolution text: the game's pixel-art letters and digits (menus,
// captions, score) are drawn from vector fonts instead of being upscaled.
//
// Files in textures/text/ (made by tools/make_text_atlas.py):
//   atlas.png   one slot per tile pattern: R = letter coverage, G = outline or
//               shadow coverage, B = position in the top-to-bottom gradient
//   glyphs.txt  pattern, hash of its pixels, atlas slot and the palette
//               nibbles of the gradient's top and bottom and of the edge
// A tile is replaced only while its pattern still holds the pixels the atlas
// was made from. Colours come from the game's palette every frame.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class TextHD {
public:
    static constexpr int CELLS_X = 62, CELLS_Y = 48;   // 496x384 in 8x8 cells
    // Per cell, 4 BGRA texels: [0] atlas slot (low byte in R, high byte in G,
    // A = 255 when the cell is text), [1] gradient top, [2] gradient bottom,
    // [3] edge colour (A = 0 when that colour is transparent)
    static constexpr int CELL_TEXELS = 4;
    // One more row after the cells lists the 2D replacement pictures on screen
    // (Pack2D::apply): texel 0 R = how many, then per picture a texel with its
    // rectangle in cells (R x0, G y0, B x1, A y1) and one with its slot in R
    static constexpr int ROWS = CELLS_Y + 1, ROW_TEXELS = CELLS_X * CELL_TEXELS;
    static constexpr int ATLAS_COLS = 16;   // tools/make_text_atlas.py COLS

    struct Glyph {
        uint64_t hash = 0;
        uint16_t slot = 0;
        uint8_t top = 0, bottom = 0, edge = 0;   // palette nibbles
        bool used = false;
    };

    bool init(const std::string& dir);
    bool empty() const { return count_ == 0; }
    // Glyph for a 4 bpp pattern, if its pixels in `vram` are the expected ones
    const Glyph* find(uint32_t pattern, const uint8_t* vram) const;

    int atlas_w = 0, atlas_h = 0;
    std::vector<uint8_t> atlas_rgba;

private:
    std::vector<Glyph> glyphs_;   // indexed by pattern number
    int count_ = 0;
};
