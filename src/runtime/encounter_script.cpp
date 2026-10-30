#include "encounter_script.h"

namespace script {

int op_args(int32_t op) {
    switch (op) {
    case -1: case -12: case -13: case -14: case -15: case -22: case -29: case -30: case -31: case -32: case -33:
    case -998: case -999: return 0;
    case -5: case -8: case -10: case -19: case -20: case -21: case -23: case -27: case -28: return 1;
    case -17: case -24: case -26: return 2;
    case -9: case -25: return 3;
    case -11: return 4;
    default: return -1;
    }
}

bool is_enemy_fn(uint32_t fn) {
    switch (fn) {
    case FN_RAPTOR: case 0x61AF8: case 0x627E8: case 0x46B50: case 0x536CC: case FN_PLACED: case FN_PLACED_XZ:
    case 0x71914: return true;
    default: return false;
    }
}

bool read(Word word, uint32_t pc, Item& it) {
    int32_t op = word(pc);
    uint32_t len;
    if (op < 0) {
        int n = op_args(op);
        if (n < 0) return false;
        len = 1 + uint32_t(n);
    } else {
        uint32_t nargs = uint32_t(word(pc + 2));
        if (nargs > 61) return false;
        len = 3 + nargs;
    }
    it.pc = pc;
    it.w.resize(len);
    for (uint32_t k = 0; k < len; k++) it.w[k] = word(pc + k);
    return true;
}

uint32_t find(Word word, int id) {
    Item it;
    for (uint32_t i = 0; i < WORDS; i += uint32_t(it.w.size())) {
        if (!read(word, i, it)) return NOT_FOUND;
        if (it.op() == -10 && it.w[1] == id) return i;
    }
    return NOT_FOUND;
}

}  // namespace script
