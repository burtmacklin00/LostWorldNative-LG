"""Lists the encounter scripts of The Lost World (survival mode research).

The encounter machine (task fn_0003951C, started by the zone trigger
fn_00039428 with the encounter number) runs a stream of 32-bit words at
0x1268B8. See docs/game_ram.md.

Usage: python tools/encounters.py [data/crom.bin]
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from analyze import Memory  # noqa: E402

SCRIPT = 0x1268B8
WORDS = 0x13DF
# Negative opcodes and how many words follow them
OPS = {-1: 0, -8: 1, -9: 3, -10: 1, -11: 4, -12: 0, -13: 0, -14: 0, -15: 0, -17: 2, -19: 1, -20: 1,
       -5: 1, -21: 1, -22: 0, -23: 1, -24: 2, -25: 3, -26: 2, -27: 1, -28: 1, -29: 0, -30: 0, -31: 0, -32: 0,
       -33: 0, -998: 0, -999: 0}


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def parse(mem):
    w = [s32(mem.r32(SCRIPT + 4 * i)) for i in range(WORDS)]
    encs = collections.OrderedDict()
    cur = None
    i = 0
    while i < WORDS:
        op = w[i]
        if op == -10:
            cur = w[i + 1]
            encs[cur] = []
        if op < 0:
            n = OPS.get(op)
            if n is None:
                encs.setdefault(cur, []).append((i, "?", [op]))
                i += 1
                continue
            encs.setdefault(cur, []).append((i, op, w[i + 1:i + 1 + n]))
            i += 1 + n
        else:
            fn, nargs = w[i + 1] & 0xFFFFFFFF, w[i + 2]
            encs.setdefault(cur, []).append((i, "call", [op, fn] + w[i + 3:i + 3 + nargs]))
            i += 3 + nargs
    return encs


def main():
    mem = Memory(open(sys.argv[1] if len(sys.argv) > 1 else "data/crom.bin", "rb").read())
    encs = parse(mem)
    fns = collections.Counter()
    for e, items in encs.items():
        print(f"encounter {e}:")
        for i, kind, args in items:
            if kind == "call":
                fns[args[1]] += 1
                print(f"  [{i}] +{args[0]} fn_{args[1]:08X}({', '.join(str(a) for a in args[2:])})")
            else:
                print(f"  [{i}] op {kind} {' '.join(str(a) for a in args)}")
    print("functions:", ", ".join(f"{f:X}*{n}" for f, n in fns.most_common()))


if __name__ == "__main__":
    main()
