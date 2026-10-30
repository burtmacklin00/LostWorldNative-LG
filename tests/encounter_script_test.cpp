// The encounter script reader (encounter_script.h) on a made-up script: the
// header search, the walk of one encounter to its end, what stops it, and the
// opcode table against tools/encounters.py's (argument: that file)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "encounter_script.h"

static int g_failed = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAILED line %d: %s\n", __LINE__, #cond); \
            g_failed++;                                      \
        }                                                    \
    } while (0)

static std::vector<int32_t> g_words;
static int32_t word(uint32_t pc) { return pc < g_words.size() ? g_words[pc] : -1; }

// The script: words, then -1 (wait) up to its full length
static void load(std::vector<int32_t> w) {
    w.resize(script::WORDS, -1);
    g_words = w;
}

static std::vector<std::vector<int32_t>> walked(uint32_t h, bool* ok = nullptr, int32_t* bad = nullptr) {
    std::vector<std::vector<int32_t>> items;
    bool r = script::walk(word, h, [&](const script::Item& it) { items.push_back(it.w); }, bad);
    if (ok) *ok = r;
    return items;
}

// OPS = {-1: 0, ...} in tools/encounters.py: opcode -> words after it
static std::map<int, int> python_ops(const char* path) {
    std::map<int, int> ops;
    FILE* f = fopen(path, "rb");
    if (!f) return ops;
    std::string s;
    char buf[4096];
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) s.append(buf, n);
    fclose(f);
    size_t a = s.find("OPS = {"), b = a == std::string::npos ? a : s.find('}', a);
    if (b == std::string::npos) return ops;
    for (const char* p = s.c_str() + a + 7; p < s.c_str() + b;) {
        char* end;
        long op = strtol(p, &end, 10);
        if (end == p) {
            p++;
            continue;
        }
        p = end;
        while (*p == ' ' || *p == ':') p++;
        ops[int(op)] = int(strtol(p, &end, 10));
        p = end;
    }
    return ops;
}

int main(int argc, char** argv) {
    // Encounter 2: a camera turn, a raptor call, a wait for it all and its end;
    // encounter 5: a call of something else, a header past the end of 7
    load({-10, 2, -9, 10, 0, -20, 0, int32_t(script::FN_RAPTOR), 3, 1, 4, 2, -1, -999,
          -10, 5, 30, 0x12340, 0, -21, 60, -998, -10, 7, -1, -999});
    CHECK(script::find(word, 2) == 0);
    CHECK(script::find(word, 5) == 14);
    CHECK(script::find(word, 7) == 22);
    CHECK(script::find(word, 3) == script::NOT_FOUND);

    script::Item it;
    CHECK(script::read(word, 2, it) && it.pc == 2 && it.w.size() == 4 && it.op() == -9 && !it.call());
    CHECK(script::read(word, 6, it) && it.call() && it.fn() == script::FN_RAPTOR && it.enemy() && it.w.size() == 6);
    CHECK(script::read(word, 13, it) && it.end());

    bool ok = false;
    auto two = walked(0, &ok);
    CHECK(ok && two.size() == 5);   // header, turn, call, wait, end
    CHECK(two.size() == 5 && two[0] == (std::vector<int32_t>{-10, 2}) && two[4] == (std::vector<int32_t>{-999}));
    auto five = walked(14, &ok);
    CHECK(ok && five.size() == 4 && five[1][1] == 0x12340 && five[3][0] == -998);

    // No end operation: the walk stops before the next header
    load({-10, 2, -1, -10, 3, -999});
    CHECK(walked(0, &ok).size() == 2 && ok);

    // An unknown operation stops it (and the header search before it), and so does a call too long
    int32_t bad = 0;
    load({-10, 2, -1, -77, -999, -10, 3, -999});
    CHECK(walked(0, &ok, &bad).size() == 2 && !ok && bad == -77);
    CHECK(script::find(word, 3) == script::NOT_FOUND);
    load({-10, 2, 0, int32_t(script::FN_RAPTOR), 62, -999});
    CHECK(walked(0, &ok, &bad).size() == 1 && !ok && bad == 0);
    load({-10, 2, 0, int32_t(script::FN_RAPTOR), -2, -999});
    CHECK(!script::read(word, 2, it));

    // The random numbers: the LCG both modes have always used
    uint32_t s = 1;
    CHECK(script::rand_below(s, 100) == ((1u * 1103515245u + 12345u) >> 16) % 100);
    CHECK(script::rand_below(s, 0) == 0);

    // The same opcode table as the Python lister
    if (argc > 1) {
        std::map<int, int> py = python_ops(argv[1]);
        CHECK(py.size() > 20);
        for (auto& [op, n] : py) {
            if (script::op_args(op) != n) printf("op %d: %d words here, %d in encounters.py\n", op, script::op_args(op), n);
            CHECK(script::op_args(op) == n);
        }
        for (int op = -1000; op < 0; op++)
            if (script::op_args(op) >= 0) CHECK(py.count(op));
    }

    if (g_failed) printf("%d checks failed\n", g_failed);
    else printf("encounter script: all checks passed\n");
    return g_failed ? 1 : 0;
}
