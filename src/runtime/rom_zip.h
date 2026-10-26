// Reads The Lost World's memory regions straight from the original
// lostwsga.zip (MAME/Supermodel set), as tools/romtool.py does for data/:
// each chip is found by its CRC, so the names inside the zip do not matter.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct RomSet {
    std::vector<uint8_t> crom, banked_crom, vrom, sound_program, sound_samples;
};

// One chip of a region: its data and where its first byte goes
struct RomPart {
    uint32_t offset;
    std::vector<uint8_t> data;
};

// Lays the chips of a region out the way the board sees them: `chunk` bytes
// of each chip in turn every `stride` bytes, 16-bit byte-swapped with `swap`
std::vector<uint8_t> interleave_region(const std::vector<RomPart>& parts, int stride, int chunk, bool swap);

// Fills `out` from the zip. On failure returns false with the reason in
// `error` (the zip cannot be opened, a chip is missing or its CRC is wrong)
bool load_rom_zip(const std::string& zip_path, RomSet& out, std::string& error);
