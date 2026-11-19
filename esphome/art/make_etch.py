# Draws the light dotted "etch" behind the clock on the Sticky's Today page: rolling hills, a few houses
# and trees (no sun: it sat behind the weather text).
# Usage: uv run --with pillow python art/make_etch.py art   -> etch_land.png (800x422), etch_port.png (480x742)
# Want your own picture? Any black-on-transparent PNG of the same size works; set etch_land / etch_port in my-home.yaml.
import sys, os, math
from PIL import Image, ImageDraw

def layers(w, h, base):
    """The scene as silhouettes, back to front, each with how dense its inside dots are (0 = outline only)."""
    u = w / 800  # 1 unit = 1 px on the landscape screen
    def hill(x, amp, freq, phase, lift):
        return base - lift - amp * (0.5 + 0.5 * math.sin(x / (freq * u) + phase))
    out = []
    def layer(fill):
        m = Image.new('1', (w, h), 0); out.append((m, fill)); return ImageDraw.Draw(m)
    back = lambda x: hill(x, 60, 130, 0.6, 70)
    front = lambda x: hill(x, 36, 95, 2.4, 0)
    d = layer(9)
    d.polygon([(0, h)] + [(x, int(back(x))) for x in range(0, w + 1, 4)] + [(w, h)], fill=1)
    # round trees standing on the back hill
    d = layer(3)
    for fx, tr in ((0.10, 24), (0.17, 18), (0.55, 22), (0.92, 26)):
        x0 = int(w * fx); gy = int(back(x0)) + int(4 * u); tr = int(tr * u)
        d.rectangle([x0 - int(3 * u), gy - int(28 * u), x0 + int(3 * u), gy], fill=1)
        d.ellipse([x0 - tr, gy - int(28 * u) - 2 * tr, x0 + tr, gy - int(28 * u)], fill=1)
    d = layer(9)
    d.polygon([(0, h)] + [(x, int(front(x))) for x in range(0, w + 1, 4)] + [(w, h)], fill=1)
    # houses on the front hill: walls and a pitched roof, each its own layer so neighbours keep their outlines
    for fx, bw, bh in ((0.28, 62, 44), (0.40, 50, 36), (0.66, 68, 48)):
        d = layer(3)
        bw, bh = int(bw * u), int(bh * u)
        x0 = int(w * fx); gy = int(front(x0 + bw / 2)) + int(8 * u)
        d.rectangle([x0, gy - bh, x0 + bw, gy], fill=1)
        d.polygon([(x0 - int(8 * u), gy - bh), (x0 + bw // 2, gy - bh - int(28 * u)), (x0 + bw + int(8 * u), gy - bh)], fill=1)
        d.rectangle([x0 + bw // 4, gy - bh + int(12 * u), x0 + bw // 4 + int(12 * u), gy - bh + int(24 * u)], fill=0)
    return out

def etch(mask, fill, out):
    # dotted style: thick dots on the outline, sparse dots inside; a nearer layer hides what is behind it
    w, h = out.size; px = out.load(); mk = mask.load()
    for y in range(h):
        for x in range(w):
            if mk[x, y]: px[x, y] = 1
    step = 5
    for y in range(0, h, step):
        off = (y // step) % 2 * (step // 2)
        for x in range(off, w, step):
            inside = mk[x, y]
            edge = inside and any(not mk[min(w - 1, max(0, x + dx)), min(h - 1, max(0, y + dy))] for dx, dy in ((-step, 0), (step, 0), (0, -step), (0, step)))
            if edge:
                for a in range(2):
                    for b in range(2): px[min(w - 1, x + a), min(h - 1, y + b)] = 0
            elif inside and fill and (x // step + y // step) % fill == 0:
                px[x, y] = 0

od = sys.argv[1]
for name, w, h in (('land', 800, 422), ('port', 480, 742)):
    e = Image.new('1', (w, h), 1)
    for m, fill in layers(w, h, h - 52): etch(m, fill, e)
    # WHY: ESPHome draws only the opaque pixels of a chroma-key binary image, so the dots are opaque and the rest clear
    rgba = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    rgba.paste((0, 0, 0, 255), mask=e.point(lambda v: 255 if v == 0 else 0).convert('L'))
    rgba.save(os.path.join(od, f'etch_{name}.png'))
print('ok')
