#include "text_hd.h"

#include "../common/fnv1a.h"

#include <cstdio>

#include "image_io.h"

namespace {

uint64_t pattern_hash(const uint8_t* p) { return fnv1a_bytes(p, 32); }
}  // namespace

bool TextHD::init(const std::string& dir) {
    FILE* f = fopen((dir + "/glyphs.txt").c_str(), "r");
    if (!f) return false;
    if (!png_load(dir + "/atlas.png", atlas_w, atlas_h, atlas_rgba)) {
        fprintf(stderr, "[text] could not read %s/atlas.png\n", dir.c_str());
        fclose(f);
        return false;
    }
    glyphs_.assign(0x8000, Glyph{});
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned pattern, slot, top, bottom, edge;
        unsigned long long hash;
        if (line[0] == '#' ||
            sscanf(line, "%x %llx %u %x %x %x", &pattern, &hash, &slot, &top, &bottom, &edge) != 6 ||
            pattern >= glyphs_.size())
            continue;
        Glyph& g = glyphs_[pattern];
        g.hash = hash;
        g.slot = uint16_t(slot);
        g.top = uint8_t(top);
        g.bottom = uint8_t(bottom);
        g.edge = uint8_t(edge);
        g.used = true;
        count_++;
    }
    fclose(f);
    if (count_) printf("[text] %d high-resolution text patterns\n", count_);
    return count_ > 0;
}

const TextHD::Glyph* TextHD::find(uint32_t pattern, const uint8_t* vram) const {
    if (pattern >= glyphs_.size() || !glyphs_[pattern].used) return nullptr;
    const Glyph& g = glyphs_[pattern];
    return pattern_hash(vram + pattern * 32) == g.hash ? &g : nullptr;
}
