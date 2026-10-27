import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
SS = 4
FD = "/usr/share/fonts/truetype/dejavu"
out = sys.argv[1]
def font(sz, bold=True):
    return ImageFont.truetype(os.path.join(FD, "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"), sz * SS)
RUST = (176, 74, 32); EMBER = (235, 128, 48); DARK = (20, 14, 11); MID = (46, 32, 24); TEXT = (236, 226, 214); DIM = (168, 150, 132)

def background(w, h):
    im = Image.new("RGB", (w * SS, h * SS), DARK)
    d = ImageDraw.Draw(im)
    # vertical gradient
    for y in range(h * SS):
        t = y / (h * SS)
        c = tuple(int(DARK[k] + (MID[k] - DARK[k]) * (1 - abs(t - 0.45) * 1.8 if abs(t - 0.45) < 0.55 else 0)) for k in range(3))
        d.line([(0, y), (w * SS, y)], fill=c)
    # diagonal plate lines like riveted metal
    step = 48 * SS
    for x in range(-h * SS, w * SS, step):
        d.line([(x, h * SS), (x + h * SS, 0)], fill=(34, 24, 18), width=2 * SS)
    return im, d

def emblem(d, cx, cy, r):
    # an original angular emblem: a broken hexagon with a slash, not the Quake logo
    import math
    pts = [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
    w = max(SS, int(r * 0.14))
    for i in range(6):
        if i == 1:
            continue
        d.line([pts[i], pts[(i + 1) % 6]], fill=RUST, width=w)
    for p in pts:
        d.ellipse([p[0] - w * 0.5, p[1] - w * 0.5, p[0] + w * 0.5, p[1] + w * 0.5], fill=RUST)
    d.polygon([(cx - r * 0.18, cy + r * 0.62), (cx + r * 0.42, cy - r * 0.55), (cx + r * 0.62, cy - r * 0.45),
               (cx + r * 0.02, cy + r * 0.72)], fill=EMBER)

def center(d, xy, s, f, fill):
    bb = d.textbbox((0, 0), s, font=f)
    d.text((xy[0] - (bb[2] - bb[0]) / 2 - bb[0], xy[1] - (bb[3] - bb[1]) / 2 - bb[1]), s, font=f, fill=fill)

def save(im, w, h, p):
    im.resize((w, h), Image.LANCZOS).convert("P", palette=Image.ADAPTIVE, colors=256).save(p, optimize=True)

im, d = background(128, 128)
emblem(d, 64 * SS, 50 * SS, 30 * SS)
center(d, (64 * SS, 106 * SS), "vkQuake2", font(17), TEXT)
save(im, 128, 128, os.path.join(out, "icon0.png"))

im, d = background(840, 500)
emblem(d, 420 * SS, 190 * SS, 110 * SS)
center(d, (420 * SS, 375 * SS), "vkQuake2", font(48), TEXT)
center(d, (420 * SS, 430 * SS), "The Vulkan renderer of Quake II, running on VkVita", font(20, False), DIM)
save(im, 840, 500, os.path.join(out, "livearea/contents/bg.png"))

im, d = background(280, 158)
d.rectangle([0, 152 * SS, 280 * SS, 158 * SS], fill=RUST)
emblem(d, 62 * SS, 76 * SS, 38 * SS)
d.text((120 * SS, 44 * SS), "vkQuake2", font=font(26), fill=TEXT)
d.text((122 * SS, 84 * SS), "Vulkan on PS Vita", font=font(13, False), fill=DIM)
save(im, 280, 158, os.path.join(out, "livearea/contents/startup.png"))

open(os.path.join(out, "livearea/contents/template.xml"), "w").write("""<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
""")
