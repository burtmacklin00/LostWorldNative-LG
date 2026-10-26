"""Finds the gun crosshair in screenshots taken with different --aim values.

All screenshots must show the same frame (e.g. the stage select screen with
--auto coin@1100,start@1200 --shot 1500); the crosshair is the dark red dot
that is only in one of them. Buttons that light up under the dot are also
red, so only small round blobs of the dot's colour count. Prints the dot
centre (in Model 3 screen pixels, 496x384, the 4:3 area centred in the
window) for each file.

Usage: python tools/find_crosshair.py out/aim_40_40.bmp out/aim_128_128.bmp ...
"""
import struct
import sys


def load(path):
    d = open(path, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    px = d[off:off + w * abs(h) * 4]
    if h > 0:   # bottom-up rows
        px = b"".join(px[y * w * 4:(y + 1) * w * 4] for y in reversed(range(h)))
    return w, abs(h), px


def is_dot(p):   # BGR: dark red, no green or blue
    return 150 < p[2] < 215 and p[1] < 40 and p[0] < 40


def main():
    shots = [load(p) for p in sys.argv[1:]]
    w, h = shots[0][0], shots[0][1]
    scale = min(w / 496, h / 384)
    ox, oy = (w - 496 * scale) / 2, (h - 384 * scale) / 2
    radius = 6 * scale   # the dot is about 9 game pixels across
    for idx, (_, _, px) in enumerate(shots):
        blobs = []   # [sum x, sum y, count, min x, max x, min y, max y]
        for y in range(h):
            row = y * w * 4
            for x in range(w):
                o = row + x * 4
                p = px[o:o + 3]
                if not is_dot(p) or any(other[o:o + 3] == p for i, (_, _, other) in enumerate(shots) if i != idx):
                    continue
                for b in blobs:
                    if abs(b[0] / b[2] - x) < radius and abs(b[1] / b[2] - y) < radius:
                        b[0] += x; b[1] += y; b[2] += 1
                        b[3], b[4], b[5], b[6] = min(b[3], x), max(b[4], x), min(b[5], y), max(b[6], y)
                        break
                else:
                    blobs.append([x, y, 1, x, x, y, y])
        # Round and about the dot's size: as wide as tall, mostly filled
        dots = [b for b in blobs if b[4] - b[3] > 3 * scale and abs((b[4] - b[3]) - (b[6] - b[5])) <= 2 * scale
                and b[2] > 0.5 * (b[4] - b[3] + 1) * (b[6] - b[5] + 1)]
        if dots:
            b = max(dots, key=lambda b: b[2])
            cx, cy = b[0] / b[2], b[1] / b[2]
            print(f"{sys.argv[1 + idx]}: ({(cx - ox) / scale:.1f}, {(cy - oy) / scale:.1f})  [{b[2]} px]")
        else:
            print(f"{sys.argv[1 + idx]}: not found")


if __name__ == "__main__":
    main()
