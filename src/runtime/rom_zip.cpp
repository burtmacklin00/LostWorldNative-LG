#include "rom_zip.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>

#include "miniz.h"

namespace {

struct Chip {
    const char* name;
    uint32_t crc;
    uint32_t offset;
};
struct Region {
    std::vector<uint8_t> RomSet::*field;
    int stride, chunk;
    bool swap;
    std::vector<Chip> chips;
};

// The same table as tools/romtool.py
const std::vector<Region>& regions() {
    static const std::vector<Region> r = {
        {&RomSet::crom, 8, 2, true,
         {{"epr-19936.20", 0x2F1CA664, 0}, {"epr-19937.19", 0x9DBF5712, 2},
          {"epr-19938.18", 0x38AFE27A, 4}, {"epr-19939.17", 0x8788B939, 6}}},
        {&RomSet::banked_crom, 8, 2, true,
         {{"mpr-19921.4", 0x9AF3227F, 0x0000000}, {"mpr-19920.3", 0x8DF33574, 0x0000002},
          {"mpr-19919.2", 0xFF119949, 0x0000004}, {"mpr-19918.1", 0x95B690E9, 0x0000006},
          {"mpr-19925.8", 0xCFA4BB49, 0x1000000}, {"mpr-19924.7", 0x4EE3DDC5, 0x1000002},
          {"mpr-19923.6", 0xED515CB2, 0x1000004}, {"mpr-19922.5", 0x4DFD7FC6, 0x1000006},
          {"mpr-19929.12", 0x16491F63, 0x2000000}, {"mpr-19928.11", 0x9AFD5D4A, 0x2000002},
          {"mpr-19927.10", 0x0C96EF11, 0x2000004}, {"mpr-19926.9", 0x05A232E0, 0x2000006},
          {"mpr-19933.16", 0x8E2ACD3B, 0x3000000}, {"mpr-19932.15", 0x04389385, 0x3000002},
          {"mpr-19931.14", 0x448A5007, 0x3000004}, {"mpr-19930.13", 0xB598C2F2, 0x3000006}}},
        {&RomSet::vrom, 32, 2, false,
         {{"mpr-19902.26", 0x178BD471, 0}, {"mpr-19903.27", 0xFE575871, 2},
          {"mpr-19904.28", 0x57971D7D, 4}, {"mpr-19905.29", 0x6FA122EE, 6},
          {"mpr-19906.30", 0xA5B16DD9, 8}, {"mpr-19907.31", 0x84A425CD, 10},
          {"mpr-19908.32", 0x7702AA7C, 12}, {"mpr-19909.33", 0x8FCA65F9, 14},
          {"mpr-19910.34", 0x1EF585E2, 16}, {"mpr-19911.35", 0xCA26A48D, 18},
          {"mpr-19912.36", 0xFFE000E0, 20}, {"mpr-19913.37", 0xC003049E, 22},
          {"mpr-19914.38", 0x3C21A953, 24}, {"mpr-19915.39", 0xFD0F2A2B, 26},
          {"mpr-19916.40", 0x10B0C52E, 28}, {"mpr-19917.41", 0x3035833B, 30}}},
        {&RomSet::sound_program, 1, 1, true, {{"epr-19940.21", 0xB06FFE5F, 0}}},
        {&RomSet::sound_samples, 1, 1, true,
         {{"mpr-19934.22", 0xC7D8E194, 0x000000}, {"mpr-19935.24", 0x91C1B618, 0x400000}}},
    };
    return r;
}

std::string crc_text(uint32_t crc) {
    char t[16];
    snprintf(t, sizeof(t), "%08x", crc);
    return t;
}

}  // namespace

std::vector<uint8_t> interleave_region(const std::vector<RomPart>& parts, int stride, int chunk, bool swap) {
    size_t size = 0;
    for (const RomPart& p : parts)
        size = std::max(size, p.offset - p.offset % stride + p.data.size() * stride / chunk);
    std::vector<uint8_t> out(size);
    for (const RomPart& p : parts) {
        std::vector<uint8_t> data = p.data;
        if (swap)   // the two bytes of each 16-bit word
            for (size_t i = 0; i + 1 < data.size(); i += 2) std::swap(data[i], data[i + 1]);
        size_t base = p.offset - p.offset % stride, lane = p.offset % stride;
        for (size_t i = 0; i < data.size(); i += chunk)
            std::copy_n(data.begin() + i, std::min<size_t>(chunk, data.size() - i),
                        out.begin() + base + (i / chunk) * stride + lane);
    }
    return out;
}

bool load_rom_zip(const std::string& zip_path, RomSet& out, std::string& error) {
    // Opened with fopen, like the rest of the program's files: miniz's own
    // open takes the path as UTF-8, and the command line is in the ANSI code page
    FILE* f = fopen(zip_path.c_str(), "rb");
    if (!f) {
        error = "cannot open " + zip_path;
        return false;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_cfile(&zip, f, 0, 0)) {
        fclose(f);
        error = zip_path + " is not a valid zip";
        return false;
    }
    std::map<uint32_t, mz_uint> by_crc;   // chip CRC -> index in the zip
    for (mz_uint i = 0, n = mz_zip_reader_get_num_files(&zip); i < n; i++) {
        mz_zip_archive_file_stat st;
        if (mz_zip_reader_file_stat(&zip, i, &st) && !st.m_is_directory) by_crc.emplace(st.m_crc32, i);
    }
    bool ok = true;
    for (const Region& r : regions()) {
        std::vector<RomPart> parts;
        for (const Chip& c : r.chips) {
            auto it = by_crc.find(c.crc);
            if (it == by_crc.end()) {
                error = std::string("missing ") + c.name + " (crc " + crc_text(c.crc) + ") in " + zip_path +
                        ": not The Lost World ROM (lostwsga) or incomplete";
                ok = false;
                break;
            }
            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&zip, it->second, &size, 0);
            if (!data || mz_crc32(MZ_CRC32_INIT, static_cast<const uint8_t*>(data), size) != c.crc) {
                mz_free(data);
                error = std::string("wrong CRC for ") + c.name + " inside " + zip_path;
                ok = false;
                break;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            parts.push_back({c.offset, std::vector<uint8_t>(bytes, bytes + size)});
            mz_free(data);
        }
        if (!ok) break;
        out.*r.field = interleave_region(parts, r.stride, r.chunk, r.swap);
    }
    mz_zip_reader_end(&zip);
    fclose(f);
    return ok;
}
