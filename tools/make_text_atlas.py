"""Builds the high-resolution text glyphs (src/video/text_hd.h): the game's
pixel-art letters and digits are redrawn with free (SIL OFL) vector fonts.

Usage:
  python tools/make_text_atlas.py [snapshot.bin] [--preview out/text_preview.png]

The snapshot (F9 in the game, out/snap/*.bin) provides the tile patterns of
the fonts, which the game loads at boot and never moves. Each glyph listed
below is drawn with its font at 8x, fitted to the box of the original pixel
art, and cut into the game's 8x8 tiles. Writes:
  textures/text/atlas.png   per tile: R = letter, G = outline/shadow,
                            B = top-to-bottom position for the gradient
  textures/text/glyphs.txt  pattern, hash of its pixels, atlas slot and the
                            palette entries (nibbles) of the three colours
The colours are not stored: the game picks them from its palette at run time,
so fades and highlighted menu lines still work. Needs Pillow and numpy.
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import lwhash  # tools/ is on sys.path when run as `python tools/<script>.py`

FONTS = "third_party/fonts"
OUT = "textures/text"
SCALE = 8          # atlas pixels per original pixel
SS = 4             # supersampling of the font rasteriser
PAD = 1            # original pixels of margin around each tile in the atlas
SLOT = (8 + 2 * PAD) * SCALE
COLS = 16          # slots per atlas row (shader: ATLAS_COLS)
VRAM_SIZE = 0x190000

args = sys.argv[1:]
preview = None
if "--preview" in args:
    k = args.index("--preview")
    preview = args[k + 1]
    del args[k:k + 2]
snap = args[0] if args else "out/snap/jungle_3901.bin"
vram = open(snap, "rb").read()[-VRAM_SIZE:]


def pattern(p):
    """8x8 nibbles (4 bpp) of tile pattern p"""
    rows = []
    for y in range(8):
        w = struct.unpack_from("<I", vram, p * 32 + y * 4)[0]
        rows.append([(w >> ((7 - x) * 4)) & 15 for x in range(8)])
    return np.array(rows)


def pattern_hash(p):
    return lwhash.fnv1a_bytes(vram[p * 32:p * 32 + 32])


def entry_base(p):
    """Palette base of a 4 bpp tile: name table entries hold the pattern number
    shifted right by one (its low bit in bit 15), and their bits 4-14 select
    the colours"""
    return (p >> 1) & 0x7FF0


def palette_luma(base, n):
    c = struct.unpack_from("<I", vram, 0x100000 + (base | n) * 4)[0]
    r, g, b = (c & 31), (c >> 5) & 31, (c >> 10) & 31
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


# ---- Glyphs ----------------------------------------------------------------
# A glyph is a character and its grid of tile patterns (rows of columns).
# Sets share a font, a size and an edge style:
#   flat     one colour (menu font)
#   shadow   drop shadow below and to the right (yellow captions)
#   outline  dark outline all around (score digits)

def grid(*rows):
    return [list(r) for r in rows]


menu = [(chr(c), grid([c])) for c in range(0x21, 0x7F)]

yellow = [(str(d), grid([0x560 + 0x60 * d], [0x561 + 0x60 * d])) for d in range(10)]
# Captions are spelled with these letters (top tile, bottom tile); some come in
# several copies with slightly different shading
for ch, top, bottom in [
        ("C", 0x201, 0x206), ("O", 0x202, 0x207), ("I", 0x203, 0x208), ("N", 0x204, 0x209), ("S", 0x205, 0x20A),
        ("C", 0x221, 0x225), ("O", 0x222, 0x226), ("I", 0x223, 0x227), ("N", 0x224, 0x228),
        ("C", 0x241, 0x247), ("R", 0x242, 0x248), ("E", 0x243, 0x249), ("D", 0x244, 0x24A), ("I", 0x245, 0x24B),
        ("T", 0x246, 0x24C),
        ("C", 0x261, 0x267), ("R", 0x262, 0x268), ("E", 0x263, 0x269), ("D", 0x264, 0x26A), ("I", 0x265, 0x26B),
        ("T", 0x266, 0x26C),
        ("F", 0x281, 0x287), ("R", 0x282, 0x288), ("E", 0x281, 0x289), ("P", 0x283, 0x28A), ("L", 0x284, 0x28B),
        ("A", 0x285, 0x28C), ("Y", 0x286, 0x28D),
        ("I", 0x2A1, 0x2A7), ("N", 0x2A2, 0x2A8), ("S", 0x2A3, 0x2A9), ("E", 0x2A4, 0x2AA), ("R", 0x2A5, 0x2AB),
        ("T", 0x2A6, 0x2AC),
        ("I", 0x2C0, 0x2C9), ("N", 0x2C1, 0x2CA), ("S", 0x2C2, 0x2CB), ("E", 0x2C3, 0x2CC), ("R", 0x2C4, 0x2CD),
        ("T", 0x2C5, 0x2CE), ("M", 0x2C7, 0x2CF), ("O", 0x2C8, 0x2D0)]:
    yellow.append((ch, grid([top], [bottom])))


def big_digit(base):
    return grid([base, base + 1], [base + 2, base + 3], [base + 4, base + 5])


score = [(str(d), big_digit(0x5A0 + 0x60 * d)) for d in range(10)]
score.append(("0", big_digit(0x920)))   # unlit leading zero

SETS = [
    {"name": "menu", "font": "ShareTechMono-Regular.ttf", "edge": "flat", "glyphs": menu},
    {"name": "yellow", "font": "SairaExtraCondensed-Bold.ttf", "edge": "shadow", "glyphs": yellow},
    {"name": "score", "font": "BarlowCondensed-Bold.ttf", "edge": "outline", "glyphs": score},
]
for s in SETS:
    s["font"] = os.environ.get("LW_FONT_" + s["name"].upper(), s["font"])


# ---- Analysis of the original pixel art ------------------------------------

def original(glyph):
    """Nibbles of the whole glyph and the palette base of its first tile"""
    tiles = glyph[1]
    img = np.vstack([np.hstack([pattern(p) for p in row]) for row in tiles])
    return img, entry_base(tiles[0][0])


def roles(img, base, edge_style):
    """Palette nibbles of the gradient's top and bottom and of the edge"""
    used = [n for n in range(1, 16) if (img == n).any()]
    counts = {n: int((img == n).sum()) for n in used}
    edge = 0
    if edge_style != "flat" and len(used) > 1:
        edge = min(used, key=lambda n: palette_luma(base, n))
    body = [n for n in used if n != edge]
    total = sum(counts[n] for n in body)
    bottom = max(body, key=lambda n: counts[n])
    top = max((n for n in body if counts[n] * 8 >= total), key=lambda n: palette_luma(base, n))
    return top, bottom, edge


def body_box(img, edge):
    ys, xs = np.nonzero((img != 0) & (img != edge))
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


# ---- Drawing ---------------------------------------------------------------

def dilate(mask, r):
    """Round dilation of a coverage mask by r pixels"""
    if r <= 0:
        return mask
    out = mask.copy()
    ir = int(np.ceil(r))
    for dy in range(-ir, ir + 1):
        for dx in range(-ir, ir + 1):
            d = np.hypot(dx, dy)
            if d > r + 0.5:
                continue
            w = min(1.0, r + 0.5 - d)
            out = np.maximum(out, w * np.roll(np.roll(mask, dy, 0), dx, 1))
    return out


def shift(mask, dx, dy):
    return np.roll(np.roll(mask, dy, 0), dx, 1)


def render_set(s):
    font_path = os.path.join(FONTS, s["font"])
    analysed = []
    for g in s["glyphs"]:
        img, base = original(g)
        top, bottom, edge = roles(img, base, s["edge"])
        analysed.append((g, img, base, (top, bottom, edge), body_box(img, edge)))
    # One size for the whole set: capitals and digits as tall as the original's
    caps = [a for a in analysed if a[0][0].isupper() or a[0][0].isdigit()]
    cap_top = min(a[4][1] for a in caps)
    baseline = max(a[4][3] for a in caps)
    cap_h = baseline - cap_top
    probe = ImageFont.truetype(font_path, 1000)
    ref = probe.getbbox("H", anchor="ls")   # ref[1]: top of the capitals, negative
    size = cap_h * SCALE * SS * 1000.0 / -ref[1]
    font = ImageFont.truetype(font_path, max(1, int(round(size))))

    out = []
    for g, img, base, rl, box in analysed:
        ch = g[0]
        th, tw = img.shape
        k = SCALE * SS
        W, H = (tw + 2 * PAD) * k, (th + 2 * PAD) * k
        # Letter drawn alone, then placed: same baseline for the set, centred
        # on the original letter and narrowed if it is wider than it
        bb = font.getbbox(ch, anchor="ls")
        gw, gh = bb[2] - bb[0], bb[3] - bb[1]
        layer = Image.new("L", (gw + 4 * k, gh + 4 * k), 0)
        ImageDraw.Draw(layer).text((2 * k - bb[0], 2 * k - bb[1]), ch, font=font, fill=255, anchor="ls")
        glyph = np.asarray(layer, dtype=np.float32) / 255.0
        ox, oy = 2 * k, 2 * k   # where the glyph's bbox starts inside `layer`
        orig_w = (box[2] - box[0]) * k
        sx = 1.0
        if gw > orig_w * 1.04:
            sx = orig_w * 1.04 / gw
        if sx != 1.0:
            nw = max(1, int(round(layer.width * sx)))
            glyph = np.asarray(layer.resize((nw, layer.height), Image.LANCZOS), dtype=np.float32) / 255.0
            ox *= sx
            gw *= sx
        canvas = np.zeros((H, W), np.float32)
        cx = (PAD + (box[0] + box[2]) / 2.0) * k
        by = (PAD + baseline) * k
        x0 = int(round(cx - gw / 2.0 - ox))
        y0 = int(round(by + bb[1] - oy))
        gh_, gw_ = glyph.shape
        ys0, xs0 = max(0, -y0), max(0, -x0)
        ys1, xs1 = min(gh_, H - y0), min(gw_, W - x0)
        canvas[y0 + ys0:y0 + ys1, x0 + xs0:x0 + xs1] = glyph[ys0:ys1, xs0:xs1]
        # Down to atlas resolution, then the edge around it
        W, H = W // SS, H // SS
        letter = canvas.reshape(H, SS, W, SS).mean(axis=(1, 3))
        if s["edge"] == "outline":
            edge = dilate(letter, 0.85 * SCALE)
        elif s["edge"] == "shadow":
            edge = shift(dilate(letter, 0.3 * SCALE), int(0.7 * SCALE), int(0.9 * SCALE))
        else:
            edge = np.zeros_like(letter)
        # Gradient position: 0 at the top of the capitals, 1 at the baseline
        ys = ((np.arange(H, dtype=np.float32) + 0.5) / SCALE - PAD - cap_top) / max(cap_h, 1)
        grad = np.repeat(np.clip(ys, 0, 1)[:, None], W, 1)
        rgb = np.dstack([letter, edge, grad])
        out.append((g, rl, (rgb * 255 + 0.5).astype(np.uint8)))
    return out


# ---- Atlas -------------------------------------------------------------------

entries = {}   # pattern -> (hash, tile image, roles)
for s in SETS:
    for g, rl, big in render_set(s):
        for ty, row in enumerate(g[1]):
            for tx, p in enumerate(row):
                if p in entries:
                    continue
                x, y = tx * 8 * SCALE, ty * 8 * SCALE
                tile = big[y:y + SLOT, x:x + SLOT]
                entries[p] = (pattern_hash(p), tile, rl)

rows = (len(entries) + COLS - 1) // COLS
atlas = np.zeros((rows * SLOT, COLS * SLOT, 4), np.uint8)
atlas[..., 3] = 255
os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "glyphs.txt"), "w") as f:
    f.write(f"# High-resolution text glyphs made from {snap}\n")
    f.write("# pattern hash slot top bottom edge\n")
    for slot, (p, (h, tile, rl)) in enumerate(sorted(entries.items())):
        sy, sx = divmod(slot, COLS)
        atlas[sy * SLOT:(sy + 1) * SLOT, sx * SLOT:(sx + 1) * SLOT, :3] = tile
        f.write(f"{p:04x} {h:016x} {slot} {rl[0]:x} {rl[1]:x} {rl[2]:x}\n")
Image.fromarray(atlas, "RGBA").save(os.path.join(OUT, "atlas.png"))
print(f"{OUT}/atlas.png: {len(entries)} patterns, {COLS * SLOT}x{rows * SLOT}")

if preview:
    # Every set as the game would show it, coloured with the snapshot's palette
    def colour(base, n):
        c = struct.unpack_from("<I", vram, 0x100000 + (base | n) * 4)[0]
        return np.array([(c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31],
                        np.float32)
    lines = []
    for s in SETS:
        rendered = render_set(s)
        width = sum(len(g[1][0]) * 8 * SCALE for g, _, _ in rendered)
        height = max(len(g[1]) * 8 * SCALE for g, _, _ in rendered)
        line = np.zeros((height, width, 3), np.float32) + np.array([30, 34, 70], np.float32)
        x = 0
        for g, rl, big in rendered:
            base = entry_base(g[1][0][0])
            th, tw = len(g[1]) * 8 * SCALE, len(g[1][0]) * 8 * SCALE
            px = big[PAD * SCALE:PAD * SCALE + th, PAD * SCALE:PAD * SCALE + tw].astype(np.float32) / 255.0
            top, bottom, edge = (colour(base, n) for n in rl)
            fill = top * (1 - px[..., 2:3]) + bottom * px[..., 2:3]
            region = line[:th, x:x + tw]
            if rl[2]:
                region[:] = region * (1 - px[..., 1:2]) + edge * px[..., 1:2]
            region[:] = region * (1 - px[..., 0:1]) + fill * px[..., 0:1]
            x += tw
        lines.append(line)
    width = max(l.shape[1] for l in lines)
    sheet = np.vstack([np.pad(l, ((4, 4), (0, width - l.shape[1]), (0, 0))) for l in lines])
    Image.fromarray(sheet.clip(0, 255).astype(np.uint8), "RGB").save(preview)
    print("Preview:", preview)
