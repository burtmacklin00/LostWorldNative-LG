"""Redraws a title of the 2D layer (STAGE 1, the stage name below the route
map...) with the Jurassic Park font, as a 2D replacement (src/video/pack2d.h).

Usage:
  python tools/make_title.py out/2d/capture_<frame> X Y W H "TEXT" name [--font file.ttf] [--box]

X Y W H is the rectangle in 2D pixels (496x384 screen), widened to whole 8x8
cells, around the title in a capture (F12 in the game, or --shot N). The new
text fills the same box as the original letters and takes its colours from
them: the body, the line inside the letters and the drop shadow. Digits, which
the Jurassic Park font draws as claw marks, come from Saira ExtraCondensed.
--box: dark letters on a coloured label (the results screen's HITS, HIT RATE...);
the label is redrawn too, without a shadow.
Writes textures/2d/<name>.txt and textures/2d/<name>.png. Needs Pillow and numpy.
"""
import os
import sys
from collections import Counter

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

CELLS_X, CELLS_Y = 62, 48
SCALE = 8      # picture pixels per 2D pixel
SS = 2         # supersampling of the font rasteriser
FONTS = "third_party/fonts"

args = sys.argv[1:]
font_file = os.path.join(FONTS, "Jurassic_Park.ttf")
box_mode = "--box" in args
if box_mode:
    args.remove("--box")
if "--font" in args:
    k = args.index("--font")
    font_file = args[k + 1]
    del args[k:k + 2]
if len(args) != 7:
    sys.exit(__doc__)
base, text, name = args[0], args[5], args[6]
x, y, w, h = (int(v) for v in args[1:5])
cx0, cy0 = x // 8, y // 8
cx1, cy1 = min(CELLS_X, (x + w + 7) // 8), min(CELLS_Y, (y + h + 7) // 8)

# What the game looks for on screen, as tools/make_2d_replacement.py
# The texts' own layers when the capture has them (F12 saves <base>_layer_a): the
# picture behind them can come and go without hiding the replacement
cells_file = base + "_layer_a.cells" if os.path.exists(base + "_layer_a.cells") else base + ".cells"
rows = [line.split() for line in open(cells_file)]
os.makedirs("textures/2d", exist_ok=True)
with open(f"textures/2d/{name}.txt", "w") as f:
    f.write(f"# Title \"{text}\" made from {base}\n")
    f.write(f"rect {cx0} {cy0} {cx1 - cx0} {cy1 - cy0}\n")
    f.write("cover\n")   # a title: the whole box is the new picture's, whatever is drawn there
    f.write("match 95\n")   # STAGE1 is most of STAGE5: nearly every cell, the digit's too
    for cy in range(cy0, cy1):
        for cx in range(cx0, cx1):
            f.write(rows[cy][cx] + "\n")

# ---- The original letters --------------------------------------------------
rgba_in = np.asarray(Image.open(base + ".png").convert("RGBA").crop((cx0 * 8, cy0 * 8, cx1 * 8, cy1 * 8)))
img = rgba_in[..., :3]
H, W = img.shape[:2]
key = img[..., 0].astype(np.int32) << 16 | img[..., 1].astype(np.int32) << 8 | img[..., 2]
key[rgba_in[..., 3] == 0] = -1
label = None
if box_mode:
    # The label is the most common colour; the letters, everything else inside it
    label_key = Counter(key[key >= 0].tolist()).most_common(1)[0][0]
    ly, lx = np.nonzero(key == label_key)
    label = lx.min(), ly.min(), lx.max() + 1, ly.max() + 1
    ink = np.zeros_like(key, bool)
    ink[label[1]:label[3], label[0]:label[2]] = True
    ink &= (key != label_key) & (key >= 0)
else:
    # The titles are white, red and orange (exactly these colours) over a
    # picture; any other reddish or white colour if those are missing
    ink = np.isin(key, [0xFFFFFF, 0xFF0000, 0xFFA400, 0xFF9400])
    if ink.sum() < 50:
        r, g, b = (img[..., i].astype(np.int32) for i in range(3))
        ink = (r > b + 40) | ((r > 220) & (g > 220) & (b > 220))
ys, xs = np.nonzero(ink)
box = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
colours = Counter(key[ink].tolist())


def rgb(k):
    return np.array([(k >> 16) & 255, (k >> 8) & 255, k & 255], np.float32)


def luma(k):
    return 0.2126 * ((k >> 16) & 255) + 0.7152 * ((k >> 8) & 255) + 0.0722 * (k & 255)


def saturation(k):
    c = rgb(k)
    return (c.max() - c.min()) / max(c.max(), 1)


common = [k for k, n in colours.most_common() if n * 50 >= ink.sum()]
body = (min if box_mode else max)(common, key=luma)
rest = [k for k in common if k != body]
if box_mode:   # no shadow: the line inside is the commonest of the rest
    inner = max(rest, key=lambda k: colours[k]) if rest else body
    rest = []
else:
    inner = max(rest, key=lambda k: saturation(k) * (255 - ((k >> 8) & 255))) if rest else body
    rest = [k for k in rest if k != inner]
shadow = max(rest, key=lambda k: colours[k]) if rest else 0x000000
# Shadow offset: how far the shadow colour reaches beyond the body, down and right
sh_dx = sh_dy = 0 if box_mode else 1
if rest:
    sy, sx = np.nonzero(key == shadow)
    by, bx = np.nonzero(key == body)
    sh_dx = max(1, int(round(sx.max() - bx.max())))
    sh_dy = max(1, int(round(sy.max() - by.max())))
print(f"body #{body:06x}, inside #{inner:06x}, shadow #{shadow:06x} ({sh_dx},{sh_dy})")

# ---- New letters -------------------------------------------------------------
k = SCALE * SS
title_font = ImageFont.truetype(font_file, 100 * k)
digit_font = ImageFont.truetype(os.path.join(FONTS, "SairaExtraCondensed-Bold.ttf"), 100 * k)


def font_for(ch):
    # the font draws digits as claw marks and the full stop as its logo
    return title_font if ch.isalpha() else digit_font


# Height of the letters (without the shadow) in the original
target_h = (box[3] - box[1] - sh_dy) * k
target_w = (box[2] - box[0] - sh_dx) * k
cap = -title_font.getbbox("H", anchor="ls")[1]
dcap = -digit_font.getbbox("8", anchor="ls")[1]

# Text laid out at 100k size, each character with its font, then scaled to the box
pieces, pen = [], 0.0
for ch in text:
    f = font_for(ch)
    scale = 1.0 if f is title_font else cap / dcap   # digits as tall as the capitals
    if ch == " ":
        pen += title_font.getlength(" ")
        continue
    pieces.append((ch, f, scale, pen))
    pen += f.getlength(ch) * scale + (cap * 0.06 if f is digit_font else 0)
layout = Image.new("L", (int(pen) + 4 * k, int(cap * 1.4) + 4 * k), 0)
digits = Image.new("L", layout.size, 0)
for ch, f, scale, px in pieces:
    if scale == 1.0:
        ImageDraw.Draw(layout).text((2 * k + px, 2 * k + cap), ch, font=f, fill=255, anchor="ls")
    else:
        bb = f.getbbox(ch, anchor="ls")
        tmp = Image.new("L", (bb[2] - bb[0] + 4, bb[3] - bb[1] + 4), 0)
        ImageDraw.Draw(tmp).text((2 - bb[0], 2 - bb[1]), ch, font=f, fill=255, anchor="ls")
        tmp = tmp.resize((max(1, int(tmp.width * scale)), max(1, int(tmp.height * scale))), Image.LANCZOS)
        pos = (int(2 * k + px + bb[0] * scale), int(2 * k + cap + bb[1] * scale))
        layout.paste(255, pos, tmp)
        digits.paste(255, pos, tmp)
bbox = layout.getbbox()
layout, digits = layout.crop(bbox), digits.crop(bbox)
# The outlines are told apart from the counters at a size where the gap inside
# the letters (thin) and the counters (wide) differ clearly, then scaled down
A_CAP = 240
an = A_CAP / cap
layout = layout.resize((max(1, int(layout.width * an)), max(1, int(layout.height * an))), Image.LANCZOS)
digits = digits.resize(layout.size, Image.LANCZOS)

# Body (the letters' strokes) and the inside of their outlines
strokes = np.asarray(layout, np.float32) / 255
solid = np.pad((np.asarray(layout) > 127).astype(np.uint8) * 255, 1)
filled = Image.fromarray(solid).copy()   # fromarray shares a read-only buffer
ImageDraw.floodfill(filled, (0, 0), 128)   # outside
filled = np.asarray(filled)[1:-1, 1:-1] != 128
holes = Image.fromarray(((filled & (solid[1:-1, 1:-1] == 0)) * 255).astype(np.uint8))
# Only the thin gap of the outlined letters; real counters (the middle of an
# O or an A) stay see-through
# (each hole is measured by how many 1-pixel erosions it survives: the gaps
# about 3-5 at this size, the counters 12 or more)
depth = np.zeros(filled.shape, np.int32)
eroded = holes
for i in range(1, 12):
    eroded = eroded.filter(ImageFilter.MinFilter(3))
    m = np.asarray(eroded) > 0
    if not m.any():
        break
    depth[m] = i
wide = np.zeros(filled.shape, bool)
work = holes.copy()
hole_id = 1
while True:
    ys, xs = np.nonzero(np.asarray(work) == 255)
    if len(ys) == 0 or hole_id == 255:
        break
    ImageDraw.floodfill(work, (int(xs[0]), int(ys[0])), hole_id)
    m = np.asarray(work) == hole_id
    if depth[m].max() >= 8:
        wide |= m
    hole_id += 1
inside = np.clip(filled.astype(np.float32) - strokes, 0, 1) * ~wide
# Digits have no outline of their own: their middle gets the inside colour
d = np.asarray(digits, np.float32) / 255
thin = np.asarray(digits.filter(ImageFilter.MinFilter(2 * int(A_CAP * 0.065) + 1)), np.float32) / 255
inside = np.maximum(inside, np.minimum(thin, d))
cover = np.maximum(strokes, filled.astype(np.float32) * ~wide)


def to_target(m):
    img = Image.fromarray((m * 255).clip(0, 255).astype(np.uint8))
    return np.asarray(img.resize((int(target_w), int(target_h)), Image.LANCZOS), np.float32) / 255


strokes, inside, cover = to_target(strokes), to_target(inside), to_target(cover)

canvas_h, canvas_w = H * k, W * k
ox, oy = box[0] * k, box[1] * k


def place(a, dx=0, dy=0):
    out = np.zeros((canvas_h, canvas_w), np.float32)
    y0, x0 = oy + dy, ox + dx
    hh, ww = min(a.shape[0], canvas_h - y0), min(a.shape[1], canvas_w - x0)
    out[y0:y0 + hh, x0:x0 + ww] = a[:hh, :ww]
    return out


layers = [
    (place(strokes), rgb(body)),
    (place(inside), rgb(inner)),
]
if not box_mode:
    layers.insert(0, (place(cover, sh_dx * k, sh_dy * k), rgb(shadow)))
else:   # the label, with its corners cut like the original
    plate = np.zeros((canvas_h, canvas_w), np.float32)
    plate[label[1] * k:label[3] * k, label[0] * k:label[2] * k] = 1
    for cx_, cy_ in ((label[0], label[1]), (label[2] - 1, label[1]), (label[0], label[3] - 1),
                     (label[2] - 1, label[3] - 1)):
        plate[cy_ * k:(cy_ + 1) * k, cx_ * k:(cx_ + 1) * k] = 0
    layers.insert(0, (plate, rgb(label_key)))
colour = np.zeros((canvas_h, canvas_w, 3), np.float32)
alpha = np.zeros((canvas_h, canvas_w), np.float32)
for a, c in layers:   # straight alpha "over"
    out_a = a + alpha * (1 - a)
    colour = (c * a[..., None] + colour * (alpha * (1 - a))[..., None]) / np.maximum(out_a, 1e-6)[..., None]
    alpha = out_a
rgba = np.dstack([colour, alpha * 255]).clip(0, 255).astype(np.uint8)
picture = Image.fromarray(rgba, "RGBA").resize((W * SCALE, H * SCALE), Image.LANCZOS)
picture.save(f"textures/2d/{name}.png")
print(f"textures/2d/{name}.txt and .png: cells {cx0},{cy0} {cx1 - cx0}x{cy1 - cy0}, {picture.width}x{picture.height}")
