// The SCSP's part of the game states (scsp.h): a chip put back from its
// state goes on exactly as the one it was taken from, both from the state as
// it is saved now (field by field) and, while the class still has that
// layout, from a chip copied byte for byte (how states were saved before)
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "scsp.h"

static int g_failed = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAILED line %d: %s\n", __LINE__, #cond); \
            g_failed++;                                      \
        }                                                    \
    } while (0)

constexpr uint32_t RAM_SIZE = 0x100000;

// Samples rendered by a chip, as raw floats, and what the 68000 would read
// as the timers tick: the interrupt level, the timers, the pending
// interrupts, the slot monitor and now and then a MIDI byte
static std::vector<float> play(Scsp& c, int n) {
    std::vector<float> out;
    for (int i = 0; i < n; i++) {
        float l, r;
        c.render(l, r);
        out.push_back(l), out.push_back(r);
        if (i % 64) continue;
        c.tick_timers();
        out.push_back(float(c.irq_level()));
        for (uint32_t a : {0x418u, 0x41Au, 0x41Cu, 0x420u, 0x408u}) out.push_back(float(c.read16(a)));
        if (i % 512 == 0) out.push_back(float(c.read16(0x404)));
    }
    return out;
}

// A chip with sound going on: random samples, every slot keyed on with
// random registers, a DSP program, timers and MIDI bytes waiting
static void busy(Scsp& c, std::vector<uint8_t>& ram, uint32_t seed) {
    std::mt19937 r(seed);
    for (auto& b : ram) b = uint8_t(r());
    c.init(ram.data(), RAM_SIZE, true);
    for (uint32_t a = 0x700; a < 0x800; a += 2) c.write16(a, uint16_t(r()));
    for (uint32_t step = 0; step < 128; step++)   // a DSP program reading only valid inputs (IRA 0..0x31)
        for (uint32_t k = 0; k < 4; k++) {
            uint16_t w = uint16_t(r());
            if (k == 1) w = uint16_t((w & ~(0x3F << 6)) | (r() % 0x32) << 6);
            c.write16(0x800 + 8 * step + 2 * k, w);   // (step 126's first word starts the DSP)
        }
    c.write16(0x400, 0x000F);   // master volume
    for (int s = 0; s < 32; s++)
        for (int w = 1; w < 16; w++) c.write16(uint32_t(s * 0x20 + w * 2), uint16_t(r()));
    for (int s = 0; s < 32; s++) c.write16(uint32_t(s * 0x20), uint16_t(0x0800 | (r() & 0x07FF)));
    c.write16(0, 0x1800 | (r() & 0x07FF));   // KYONEX
    c.write16(0x418, 0x0010), c.write16(0x41A, 0x0020), c.write16(0x41C, 0x0700);   // timers
    c.write16(0x41E, 0x07FF);   // their interrupts
    c.write16(0x408, 0x2800);   // the slot monitor on slot 5
    for (int i = 0; i < 20; i++) c.midi_in(uint8_t(r()));
    play(c, 3001);   // (odd: the slot stack pointer is not back at 0)
}

int main() {
    std::vector<uint8_t> ram(RAM_SIZE);
    Scsp a;
    busy(a, ram, 1);

    // Saved now: a fresh chip on the same RAM, its state put back
    std::vector<uint8_t> st;
    a.save_state(st);
    CHECK(st.size() == Scsp::state_size());
    std::vector<uint8_t> ram_b = ram;
    Scsp b;
    b.init(ram_b.data(), RAM_SIZE, true);
    const uint8_t* p = st.data();
    b.load_state(p);
    CHECK(p == st.data() + st.size());
    std::vector<uint8_t> again;
    b.save_state(again);
    CHECK(again == st);
    std::vector<float> sa = play(a, 20000), sb = play(b, 20000);
    CHECK(sa == sb);
    bool sound = false;
    for (float v : sa) sound |= v != 0;
    CHECK(sound);   // (the test plays something)
    CHECK(a.irq_level() == b.irq_level() && a.midi_pending() == b.midi_pending());

    // Saved before: the chip's bytes, while Scsp is laid out as it was then
    if (Scsp::legacy_size() == sizeof(Scsp)) {
        Scsp c;
        busy(c, ram, 2);
        std::vector<uint8_t> raw(sizeof(Scsp));
        memcpy(raw.data(), static_cast<const void*>(&c), sizeof(Scsp));
        std::vector<uint8_t> ram_d = ram;
        Scsp d;
        d.init(ram_d.data(), RAM_SIZE, true);
        d.load_legacy(raw.data());
        std::vector<uint8_t> sc, sd;
        c.save_state(sc), d.save_state(sd);
        CHECK(sc == sd);
        CHECK(play(c, 20000) == play(d, 20000));
    } else {
        printf("Scsp has changed since the states without a version: their layout is no longer checked here\n");
    }

    if (g_failed) printf("%d checks failed\n", g_failed);
    else printf("scsp state: all checks passed\n");
    return g_failed ? 1 : 0;
}
