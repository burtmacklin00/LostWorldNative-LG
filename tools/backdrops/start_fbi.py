"""The FBI screen shown at boot as a 2D replacement: the seal redrawn by
Sergi (out/start/new_seal.png, with its own background, which is removed) in
the original's box and the slogan in Archivo Black. From an F12 capture:
  python tools/backdrops/start_fbi.py out/2d/capture_<frame>
Writes textures/2d/start_fbi.txt and .png. Needs Pillow and numpy.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 4   # picture pixels per 2D pixel
BACK = (139, 172, 189)
base = sys.argv[1] if len(sys.argv) > 1 else "out/2d/capture_1289"
cap = np.asarray(Image.open(base + ".png").convert("RGBA")).astype(int)
canvas = Image.new("RGBA", (496 * S, 384 * S), BACK + (255,))

# Seal: its own background is made see-through from the corners
seal = Image.open("out/start/new_seal.png").convert("RGB")
px = np.asarray(seal).astype(int)
mask = Image.fromarray(((np.abs(px - px[5, 5]).sum(2) > 36) * 255).astype(np.uint8)).copy()
for corner in [(0, 0), (seal.width - 1, 0), (0, seal.height - 1), (seal.width - 1, seal.height - 1)]:
    ImageDraw.floodfill(mask, corner, 128)
m = np.asarray(mask)
alpha = np.asarray(Image.fromarray(np.where(m == 128, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)))
art = Image.fromarray(np.dstack([np.asarray(seal), alpha]), "RGBA")
ys, xs = np.nonzero(m != 128)
art = art.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
# ...over the box of the original seal (everything above the slogan that is not background)
drawn = np.abs(cap[..., :3] - np.array(BACK)).sum(2) > 30
ys, xs = np.nonzero(drawn[:300])
x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
canvas.alpha_composite(art.resize(((x1 - x0) * S, (y1 - y0) * S), Image.LANCZOS), (x0 * S, y0 * S))

# Slogan over the original yellow letters, with a soft shadow below and right
yellow = (cap[..., 0] == 255) & (cap[..., 1] == 222) & (cap[..., 2] == 0)
ys, xs = np.nonzero(yellow[300:])
bx0, by0, bx1, by1 = xs.min(), ys.min() + 300, xs.max() + 1, ys.max() + 301
font = ImageFont.truetype("third_party/fonts/ArchivoBlack-Regular.ttf", 400)
text = '"WINNERS DON\'T USE DRUGS"'
bb = font.getbbox(text, anchor="ls")
letters = Image.new("L", (bb[2] - bb[0] + 20, bb[3] - bb[1] + 20), 0)
ImageDraw.Draw(letters).text((10 - bb[0], 10 - bb[1]), text, font=font, fill=255, anchor="ls")
letters = letters.crop(letters.getbbox()).resize(((bx1 - bx0) * S, (by1 - by0) * S), Image.LANCZOS)
for colour, offset in (((110, 128, 145, 255), S), ((255, 222, 0, 255), 0)):
    layer = Image.new("RGBA", letters.size, colour)
    layer.putalpha(letters)
    canvas.alpha_composite(layer, (bx0 * S + offset, by0 * S + offset))
canvas.save("textures/2d/start_fbi.png")

# What the game looks for: the whole screen (the texts' layers when F12 saved them)
cells = base + "_layer_a.cells"
with open("textures/2d/start_fbi.txt", "w") as f:
    f.write(f"# FBI screen redrawn from {base}\nrect 0 0 62 48\n")
    for row in open(cells):
        for h in row.split():
            f.write(h + "\n")
print("textures/2d/start_fbi.txt and .png")
