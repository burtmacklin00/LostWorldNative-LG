"""Builds res/lostworld.ico from res/logo.png (the Lost World emblem on a dark background).

The emblem (circle + banner) is cut out on a transparent background. The 256/64/48
sizes show the whole emblem; the 32/24/16 ones crop to the T-Rex head so it stays
readable in Explorer lists and the taskbar.
Usage: python tools/make_icon.py
"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "res", "logo.png")
DST = os.path.join(ROOT, "res", "lostworld.ico")

# Emblem geometry measured on logo.png (602x468)
CIRCLE = (301, 234.5, 218)       # center x, center y, radius (outer edge of the orange ring)
BANNER = (13, 233, 590, 349)     # left, top, right, bottom of the title banner
HEAD = (262, 17, 482, 237)       # square around the T-Rex head, above the banner, for the small sizes


def emblem_mask(size, ss=4):
    w, h = size
    m = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(m)
    cx, cy, r = CIRCLE
    d.ellipse([(cx - r) * ss, (cy - r) * ss, (cx + r) * ss, (cy + r) * ss], fill=255)
    d.rectangle([v * ss for v in BANNER], fill=255)
    return m.resize(size, Image.LANCZOS)


def main():
    logo = Image.open(SRC).convert("RGBA")
    logo.putalpha(emblem_mask(logo.size))

    # Whole emblem, centered in a square
    side = max(logo.size)
    full = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    full.paste(logo, ((side - logo.width) // 2, (side - logo.height) // 2))

    # T-Rex head, keeping the round edge of the emblem and cut at the banner top
    head = logo.crop(HEAD)

    images = []
    for s in (256, 64, 48):
        images.append(full.resize((s, s), Image.LANCZOS))
    for s in (32, 24, 16):
        images.append(head.resize((s, s), Image.LANCZOS))
    images[0].save(DST, format="ICO", sizes=[im.size for im in images], append_images=images[1:])
    print("Wrote", DST)


if __name__ == "__main__":
    main()
