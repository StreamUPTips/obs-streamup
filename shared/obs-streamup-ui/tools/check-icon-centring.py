# Offline check: rasterise each icon (no window, no Qt) at 18 px and 20 px with
# 8x supersampling, measure the inked bounding box and its centre offset.
import os, re, sys
from svgpathtools import parse_path
from PIL import Image, ImageDraw, ImageFont
src, out = sys.argv[1], sys.argv[2]
names = sorted({f.rsplit('-', 1)[0] for f in os.listdir(src) if f.endswith('-dark.svg')})
SS = 8
def render(svgtext, px):
    stroke = float(re.search(r'stroke-width="([\d.]+)"', svgtext).group(1))
    t = re.search(r'translate\(([-\d.]+) ([-\d.]+)\)', svgtext)
    dx, dy = (float(t.group(1)), float(t.group(2))) if t else (0, 0)
    k = px * SS / 24
    im = Image.new('L', (px * SS, px * SS), 0); d = ImageDraw.Draw(im)
    w = max(1, round(stroke * k))
    for pd in re.findall(r'<path d="([^"]+)"', svgtext):
        for seg in parse_path(pd).continuous_subpaths():
            pts = [seg.point(i / 200) for i in range(201)]
            xy = [((p.real + dx) * k, (p.imag + dy) * k) for p in pts]
            d.line(xy, fill=255, width=w, joint='curve')
            for x, y in (xy[0], xy[-1]):
                d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=255)
    for cx, cy, r in re.findall(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"', svgtext):
        cx, cy, r = (float(cx) + dx) * k, (float(cy) + dy) * k, float(r) * k
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    return im.resize((px, px), Image.LANCZOS)
rows = []
for n in names:
    s = open(os.path.join(src, n + '-dark.svg')).read()
    if "stroke-width" not in s: continue
    line = [n]
    for px in (18, 20):
        im = render(s, px)
        bb = im.point(lambda v: 255 if v > 40 else 0).getbbox()
        cx, cy = (bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2
        line.append(f'{px}px bbox {bb} centre off ({cx - px/2:+.1f},{cy - px/2:+.1f})')
        if px == 18: tile18 = im
        else: tile20 = im
    rows.append((n, tile18, tile20, line))
    print(' | '.join(line))
cols = 4; cw, ch = 300, 70
sheet = Image.new('RGB', (cw * cols, ch * ((len(rows) + cols - 1) // cols)), (23, 23, 30)); d = ImageDraw.Draw(sheet)
for i, (n, a, b, _) in enumerate(rows):
    x, y = (i % cols) * cw, (i // cols) * ch
    for j, t in enumerate((a, b)):
        bg = Image.new('RGB', t.size, (46, 46, 46)); bg.paste((250, 250, 250), mask=t)
        sheet.paste(bg.resize((t.width * 2, t.height * 2), Image.NEAREST), (x + 10 + j * 50, y + 10))
        sheet.paste(bg, (x + 110 + j * 26, y + 20))
    d.text((x + 160, y + 25), n[:24], fill=(200, 200, 210))
sheet.save(out); print(out)
