"""Redraws the texts of a 2D backdrop (a layer below the 3D image, see
src/video/sky_pack.h) with vector fonts, at 4x resolution.

Usage:
  python tools/make_backdrop.py tools/backdrops/<screen>.txt out/sky/<hash>.png [...]

Each dump of the backdrop (F12 or --dump-textures save them to out/sky) is
enlarged 4x and every text listed in the .txt file is erased and drawn again
with its font, stretched over the box of the original letters and in their
colour; "image X Y W H file" lines paste a redrawn picture over a box first.
After a "ref out/sky/<hash>.png" line, the next texts and pictures are only
used on the dumps that show exactly the same pixels in their box as that one
(the screen changes as it goes: panels that switch off, others that stay).
Colours: green, white, light (white or pale), yellow, red; "colour/RRGGBB" adds an
outline of that colour.
The result goes to textures/sky/<hash>.png. Fonts come from
third_party/fonts; "file.ttf:900" picks the weight of a variable font.

--variants X0,X1:X2[,X3...] W Y H also makes the backdrops the game shows with
any of several same-sized boxes highlighted (the difficulty bars light up for
the choice and under the gun): the rows Y..Y+H of the box at X0 (highlighted
in the first dump) and of the box at X1 (not highlighted) are copied into
every position in every combination, in every dump whose boxes are all lit or
dark, and each picture gets the hash the game will compute for it.
--texture: the dumps are 3D textures (out/textures/<hash>.png, --dump-textures);
each is redrawn 8x into textures/<hash>.png, which the texture pack uses.
--pack2d X Y W H name: instead, a 2D replacement (src/video/pack2d.h) for a
rectangle of a capture of the foreground layer (give out/2d/capture_<frame>
without extension; capture_<frame>_back for the layers below the 3D image,
written to textures/2d_back); the .txt coordinates are then screen pixels.
Needs Pillow and numpy.
"""
import os
import sys
from collections import Counter

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import lwhash  # tools/ is on sys.path when run as `python tools/<script>.py`

FONTS = "third_party/fonts"
OUT = "textures/sky"
SCALE = 4
SS = 4

args = sys.argv[1:]
texture = "--texture" in args   # a 3D texture dump (out/textures/<hash>.png): 8x into textures/
if texture:
    args.remove("--texture")
    SCALE = 8
pack2d = None
if "--pack2d" in args:   # --pack2d X Y W H name: a picture of the foreground layer (pack2d.h)
    k = args.index("--pack2d")
    pack2d = args[k + 1:k + 6]
    del args[k:k + 6]
variants = None
if "--variants" in args:
    k = args.index("--variants")
    variants = args[k + 1:k + 5]
    del args[k:k + 5]
if len(args) < 2:
    sys.exit(__doc__)
spec_path, dumps = args[0], args[1:]

items, images = [], []
ref = None   # pixels the boxes must show for the next lines to apply (None = always)
for line in open(spec_path, encoding="utf-8"):
    if not line.strip() or line.startswith("#"):
        continue
    if line.startswith("ref "):
        ref = np.asarray(Image.open(line.split(None, 1)[1].strip()).convert("RGBA")).copy()
        continue
    if line.startswith("image "):   # image X Y W H file: a redrawn picture pasted over that box
        _, x, y, w, h, path = line.split(None, 5)
        images.append((int(x), int(y), int(w), int(h), path.strip(), ref))
        continue
    x, y, w, h, colour, font, text = line.split(None, 6)
    items.append((int(x), int(y), int(w), int(h), colour, font, text.strip(), ref))


def shows(src, ref, x, y, w, h):
    """The box shows what it does in the reference dump"""
    return ref is None or np.array_equal(src[y:y + h, x:x + w], ref[y:y + h, x:x + w])


def plane_hash(rgba):
    """Hash of a backdrop as SkyPack computes it (FNV-1a over 0xAARRGGBB)"""
    a = rgba.astype(np.uint64)
    v = ((a[..., 3] << 24) | (a[..., 0] << 16) | (a[..., 1] << 8) | a[..., 2]).reshape(-1)
    return lwhash.fnv1a_words(v.tolist())


def load_font(spec, size):
    name, _, weight = spec.partition(":")
    font = ImageFont.truetype(os.path.join(FONTS, name), size)
    if weight:
        font.set_variation_by_axes([int(weight)])
    return font


def ink_mask(rgba, colour):
    r, g, b, a = (rgba[..., i].astype(np.int32) for i in range(4))
    if colour.startswith("red"):
        return (a > 0) & (r > 180) & (g < 110) & (b < 110)
    if colour.startswith("yellow"):
        return (a > 0) & (r > 150) & (g > 100) & (b < 60)
    if colour.startswith("light"):   # white or pale letters
        return (a > 0) & (r > 150) & (g > 150) & (b > 150)
    if colour.startswith("white"):
        return (a > 0) & (r > 150) & (g > 150) & (b > 150) & (abs(r - b) < 40)
    return (a > 0) & (g > 120) & (r < 140) & (b < 140) & (g > r + 60)


def redraw(src):
    H, W = src.shape[:2]
    # Enlarged with premultiplied alpha, so transparent pixels do not darken edges
    pre = src.astype(np.float32)
    pre[..., :3] *= pre[..., 3:4] / 255
    big = np.asarray(Image.fromarray(pre.clip(0, 255).astype(np.uint8), "RGBA").resize(
        (W * SCALE, H * SCALE), Image.LANCZOS), np.float32).copy()
    redrawn = np.zeros((H, W), bool)   # covered by a new picture: no old letters to erase
    for x, y, w, h, path, ref in images:
        if not shows(src, ref, x, y, w, h):
            continue
        if not os.path.exists(path):
            print(f"  missing {path}: keeping the original")
            continue
        art = np.asarray(Image.open(path).convert("RGBA").resize((w * SCALE, h * SCALE), Image.LANCZOS),
                         np.float32).copy()
        art[..., :3] *= art[..., 3:4] / 255
        big[y * SCALE:(y + h) * SCALE, x * SCALE:(x + w) * SCALE] = art
        redrawn[y:y + h, x:x + w] = True
    for x, y, w, h, colour, font_spec, text, ref in items:
        # the same letters (even if lit or dimmed) as in the reference dump
        if not shows(src, ref, x, y, w, h) and not np.array_equal(
                ink_mask(src[y:y + h, x:x + w], colour), ink_mask(ref[y:y + h, x:x + w], colour)):
            continue
        region = src[y:y + h, x:x + w]
        ink = ink_mask(region, colour)
        if not ink.any():
            print(f"  no letters at {x},{y} ({text})")
            continue
        # "white/000000": with an outline of that colour
        outline, ring_px = None, 0.5   # "colour/RRGGBB/W": outline W original pixels wide
        if "/" in colour:
            o = int(colour.split("/")[1], 16)
            if colour.count("/") > 1:
                ring_px = float(colour.split("/")[2])
            outline = np.array([o >> 16, (o >> 8) & 255, o & 255, 255], np.float32)
        keys = [tuple(p) for p in region[ink].tolist()]
        ink_colour = np.array(Counter(keys).most_common(1)[0][0], np.float32)
        ink_colour[:3] *= ink_colour[3] / 255
        # Erase the old letters (and their anti-aliasing and outline) with the
        # colour behind them, row by row (backgrounds can be gradients)
        grow = 7 if outline is not None else 5
        old = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(grow))) > 0
        if outline is not None:   # the old outline and shadow too: dark pixels near the letters
            near = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9))) > 0
            dark = (region[..., :3].astype(np.int32).max(axis=2) < 90) & (region[..., 3] > 0)
            old |= near & dark
            if not (region[..., 3] > 0).all():   # letters on see-through: all their shading goes
                old |= near & (region[..., 3] > 0)
        target = big[y * SCALE:(y + h) * SCALE, x * SCALE:(x + w) * SCALE]
        keep = ~np.asarray(Image.fromarray(redrawn[y:y + h, x:x + w]).resize((w * SCALE, h * SCALE), Image.NEAREST))
        # (opaque pixels only when there are any: the box behind the text, not the
        # see-through screen beside it)
        opaque = region[..., 3] > 0
        # grey frames next to the letters are not part of them
        rgb = region[..., :3].astype(np.int32)
        grey = (rgb.max(axis=2) - rgb.min(axis=2) < 24) & (rgb.min(axis=2) > 100) & ~ink
        old &= ~grey & opaque   # nothing is painted where the screen was see-through
        see_through = not opaque.all()
        if see_through:   # letters on see-through (their own layer): the whole box is theirs,
            old = np.ones_like(opaque)   # blur of the enlarged letters around them included
        cols = np.nonzero(ink.any(axis=0))[0]
        span = np.zeros(w, bool)
        span[cols.min():cols.max() + 1] = True
        between = Counter(tuple(p) for p in region[:, span][~old[:, span] & opaque[:, span]].tolist()).most_common(1)
        everywhere = Counter(tuple(p) for p in region[~old & opaque].tolist()).most_common(1) or \
            Counter(tuple(p) for p in region[~old].tolist()).most_common(1)
        for row in range(h):
            if not old[row].any():
                continue
            # between the letters' first and last column: not the frame around them
            inside = ~old[row] & opaque[row] & span
            back = (Counter(tuple(p) for p in region[row][inside].tolist()).most_common(1) if inside.sum() >= 4
                    else None) or between or everywhere
            back_colour = np.zeros(4, np.float32)   # letters on see-through leave nothing
            if not see_through:
                back_colour = np.array(back[0][0], np.float32)
                back_colour[:3] *= back_colour[3] / 255
            erase = np.repeat(np.repeat(old[row:row + 1], SCALE, 0), SCALE, 1) & keep[row * SCALE:(row + 1) * SCALE]
            target[row * SCALE:(row + 1) * SCALE][erase] = back_colour
        # New letters over the original letters' box
        ys, xs = np.nonzero(ink)
        bx0, by0, bx1, by1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        font = load_font(font_spec, 200)
        bb = font.getbbox(text, anchor="ls")
        layer = Image.new("L", (bb[2] - bb[0] + 8, bb[3] - bb[1] + 8), 0)
        ImageDraw.Draw(layer).text((4 - bb[0], 4 - bb[1]), text, font=font, fill=255, anchor="ls")
        layer = layer.crop(layer.getbbox())
        tw, th = (bx1 - bx0) * SCALE, (by1 - by0) * SCALE
        cover = np.asarray(layer.resize((tw * SS, th * SS), Image.LANCZOS).resize((tw, th), Image.BOX),
                           np.float32)[..., None] / 255
        # with room around the letters for their outline (it may reach past the box)
        mg = int(ring_px * SCALE * 2) + 2 if outline is not None else 0
        cover = np.pad(cover, ((mg, mg), (mg, mg), (0, 0)))
        gx0, gy0 = (x + bx0) * SCALE - mg, (y + by0) * SCALE - mg
        cx0, cy0 = max(gx0, 0), max(gy0, 0)
        cx1, cy1 = min(gx0 + cover.shape[1], big.shape[1]), min(gy0 + cover.shape[0], big.shape[0])
        cover = cover[cy0 - gy0:cy1 - gy0, cx0 - gx0:cx1 - gx0]
        spot = big[cy0:cy1, cx0:cx1]
        if outline is not None:   # all around, under the letters
            m = Image.fromarray((cover[..., 0] * 255).astype(np.uint8))
            # round: the letters blurred by the outline width, then made solid
            blur = np.asarray(m.filter(ImageFilter.GaussianBlur(ring_px * SCALE * 0.55)), np.float32) / 255
            ring = np.clip((blur - 0.03) / 0.12, 0, 1)[..., None]
            spot[:] = spot * (1 - ring) + outline * ring
        spot[:] = spot * (1 - cover) + ink_colour * cover
    out = big.copy()
    alpha = out[..., 3:4]
    out[..., :3] = np.where(alpha > 0, out[..., :3] * 255 / np.maximum(alpha, 1), 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), "RGBA")


if pack2d:   # one picture of a capture's rectangle (whole cells), with what the game looks for
    base = dumps[0]
    px, py, pw, ph = (int(v) for v in pack2d[:4])
    name = pack2d[4]
    cx0, cy0 = px // 8, py // 8
    cx1, cy1 = min(62, (px + pw + 7) // 8), min(48, (py + ph + 7) // 8)
    # the texts' own layers when the capture has them (F12 saves <base>_layer_a)
    cells_file = base + "_layer_a.cells" if os.path.exists(base + "_layer_a.cells") else base + ".cells"
    rows = [line.split() for line in open(cells_file)]
    # captures of the layers below the 3D image (<base>_back) go to their own folder
    out_dir = "textures/2d_back" if base.endswith("_back") else "textures/2d"
    os.makedirs(out_dir, exist_ok=True)
    with open(f"{out_dir}/{name}.txt", "w") as f:
        f.write(f"# Caption redrawn with {spec_path} from {base}\n")
        f.write(f"rect {cx0} {cy0} {cx1 - cx0} {cy1 - cy0}\n")
        f.write("cover\n")   # a text: glints or blinking over it do not bring the old letters back
        for cy in range(cy0, cy1):
            for cx in range(cx0, cx1):
                f.write(rows[cy][cx] + "\n")
    # Below the 3D image only the texts' own layers: the picture behind them
    # stays the game's (Pack2D puts it back where the replacement goes)
    picture = base + "_layer_a.png" if base.endswith("_back") and os.path.exists(base + "_layer_a.png") else base + ".png"
    src = np.asarray(Image.open(picture).convert("RGBA")).copy()
    items = [(x - cx0 * 8, y - cy0 * 8, w, h, c, fnt, t, None) for x, y, w, h, c, fnt, t, _ in items]
    redraw(src[cy0 * 8:cy1 * 8, cx0 * 8:cx1 * 8]).save(f"{out_dir}/{name}.png")
    print(f"{out_dir}/{name}.txt and .png: cells {cx0},{cy0} {cx1 - cx0}x{cy1 - cy0}")
    sys.exit(0)

if texture:   # same name as the dump: the texture pack picks it up by its hash
    for d in dumps:
        redraw(np.asarray(Image.open(d).convert("RGBA")).copy()).save("textures/" + os.path.basename(d))
        print("textures/" + os.path.basename(d))
    sys.exit(0)

sources = [np.asarray(Image.open(d).convert("RGBA")).copy() for d in dumps]
if variants:   # the same screen with the highlight in each other box
    xs, w, y, h = variants[0].split(":"), int(variants[1]), int(variants[2]), int(variants[3])
    lit_x, dark_x = (int(v) for v in xs[0].split(","))
    others = [int(v) for v in xs[1].split(",")]
    first = sources[0]
    lit = first[y:y + h, lit_x:lit_x + w].copy()
    dark = first[y:y + h, dark_x:dark_x + w].copy()
    boxes = [lit_x] + others
    for base in list(sources):
        if not all(np.array_equal(base[y:y + h, p:p + w], lit) or np.array_equal(base[y:y + h, p:p + w], dark)
                   for p in boxes):
            continue
        for combo in range(1 << len(boxes)):   # every box lit or not (aim and choice)
            v = base.copy()
            for i, p in enumerate(boxes):
                v[y:y + h, p:p + w] = lit if combo >> i & 1 else dark
            sources.append(v)

os.makedirs(OUT, exist_ok=True)
done = set()
for src in sources:
    h = plane_hash(src)
    if h in done:
        continue
    done.add(h)
    redraw(src).save(f"{OUT}/{h:016x}.png")
    print(f"{OUT}/{h:016x}.png")
