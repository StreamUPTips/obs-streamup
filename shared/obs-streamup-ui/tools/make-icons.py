# StreamUP dock and toolbar icons: one bold Tabler-style outline glyph each
# (MIT, Tabler paths), optically centred in the 24 box from the glyph's own
# bounding box. Writes -dark (light ink) and -light (dark ink) files and a
# report of each glyph's offset before and after centring.
import os, sys
from svgpathtools import parse_path

out = sys.argv[1]  # e.g. python tools/make-icons.py icons
STROKE = 2.25
INK = {'dark': '#fefefe', 'light': '#171717'}
RED, BLUE, GREEN = '#e5484d', '#2f95ff', '#65c466'

def circle(cx, cy, r):
    return f'M{cx - r} {cy}a{r} {r} 0 1 0 {2 * r} 0a{r} {r} 0 1 0 {-2 * r} 0'

LOCK = ['M5 13a2 2 0 0 1 2 -2h10a2 2 0 0 1 2 2v6a2 2 0 0 1 -2 2h-10a2 2 0 0 1 -2 -2z', circle(12, 16, 1), 'M8 11v-4a4 4 0 1 1 8 0v4']
LOCK_OPEN = ['M5 13a2 2 0 0 1 2 -2h10a2 2 0 0 1 2 2v6a2 2 0 0 1 -2 2h-10a2 2 0 0 1 -2 -2z', circle(12, 16, 1), 'M8 11v-5a4 4 0 0 1 8 0']
SQUARE = 'M4 6a2 2 0 0 1 2 -2h12a2 2 0 0 1 2 2v12a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2z'
LOCK_SQUARE = [SQUARE, 'M8 12a1 1 0 0 1 1 -1h6a1 1 0 0 1 1 1v3a1 1 0 0 1 -1 1h-6a1 1 0 0 1 -1 -1z', 'M10 11v-2a2 2 0 1 1 4 0v2']
LOCK_SQUARE_OPEN = [SQUARE, 'M8 12a1 1 0 0 1 1 -1h6a1 1 0 0 1 1 1v3a1 1 0 0 1 -1 1h-6a1 1 0 0 1 -1 -1z', 'M10 11v-2a2 2 0 0 1 3.8 -.8']
GLOBE = [circle(12, 12, 9), 'M12 3a4.5 9 0 0 1 0 18a4.5 9 0 0 1 0 -18', 'M3.6 12h16.8']
HEADPHONES = ['M4 15a2 2 0 0 1 2 -2h1a2 2 0 0 1 2 2v3a2 2 0 0 1 -2 2h-1a2 2 0 0 1 -2 -2z', 'M15 15a2 2 0 0 1 2 -2h1a2 2 0 0 1 2 2v3a2 2 0 0 1 -2 2h-1a2 2 0 0 1 -2 -2z', 'M4 15v-3a8 8 0 0 1 16 0v3']
CAMERA = ['M5 7h1a2 2 0 0 0 2 -2a1 1 0 0 1 1 -1h6a1 1 0 0 1 1 1a2 2 0 0 0 2 2h1a2 2 0 0 1 2 2v9a2 2 0 0 1 -2 2h-14a2 2 0 0 1 -2 -2v-9a2 2 0 0 1 2 -2', circle(12, 13, 3)]
CAMERA_OFF = ['M8.297 4.289a.997 .997 0 0 1 .703 -.289h6a1 1 0 0 1 1 1a2 2 0 0 0 2 2h1a2 2 0 0 1 2 2v8m-1.187 2.828c-.249 .11 -.524 .172 -.813 .172h-14a2 2 0 0 1 -2 -2v-9a2 2 0 0 1 2 -2h1c.298 0 .58 -.065 .834 -.181', 'M10.422 10.448a3 3 0 1 0 4.15 4.098', 'M3 3l18 18']
REFRESH = ['M20 11a8.1 8.1 0 0 0 -15.5 -2m-.5 -4v4h4', 'M4 13a8.1 8.1 0 0 0 15.5 2m.5 4v-4h-4']
FOLDER_PLUS = ['M12 19h-7a2 2 0 0 1 -2 -2v-11a2 2 0 0 1 2 -2h4l3 3h7a2 2 0 0 1 2 2v3.5', 'M16 19h6', 'M19 16v6']
EYE = [circle(12, 12, 2), 'M21 12c-2.4 4 -5.4 6 -9 6c-3.6 0 -6.6 -2 -9 -6c2.4 -4 5.4 -6 9 -6c3.6 0 6.6 2 9 6']
EYE_OFF = ['M10.585 10.587a2 2 0 0 0 2.829 2.828', 'M16.681 16.673a8.717 8.717 0 0 1 -4.681 1.327c-3.6 0 -6.6 -2 -9 -6c1.272 -2.12 2.712 -3.678 4.32 -4.674m2.86 -1.146a9.055 9.055 0 0 1 1.82 -.18c3.6 0 6.6 2 9 6c-.666 1.11 -1.379 2.067 -2.138 2.87', 'M3 3l18 18']
COG = ['M10.325 4.317c.426 -1.756 2.924 -1.756 3.35 0a1.724 1.724 0 0 0 2.573 1.066c1.543 -.94 3.31 .826 2.37 2.37a1.724 1.724 0 0 0 1.065 2.572c1.756 .426 1.756 2.924 0 3.35a1.724 1.724 0 0 0 -1.066 2.573c.94 1.543 -.826 3.31 -2.37 2.37a1.724 1.724 0 0 0 -2.572 1.065c-.426 1.756 -2.924 1.756 -3.35 0a1.724 1.724 0 0 0 -2.573 -1.066c-1.543 .94 -3.31 -.826 -2.37 -2.37a1.724 1.724 0 0 0 -1.065 -2.572c-1.756 -.426 -1.756 -2.924 0 -3.35a1.724 1.724 0 0 0 1.066 -2.573c-.94 -1.543 .826 -3.31 2.37 -2.37c1 .608 2.296 .07 2.572 -1.065z', circle(12, 12, 3)]
COLUMNS = [SQUARE, 'M12 4v16']
PAUSE = ['M6 6a1 1 0 0 1 1 -1h2a1 1 0 0 1 1 1v12a1 1 0 0 1 -1 1h-2a1 1 0 0 1 -1 -1z', 'M14 6a1 1 0 0 1 1 -1h2a1 1 0 0 1 1 1v12a1 1 0 0 1 -1 1h-2a1 1 0 0 1 -1 -1z']
FLOPPY = ['M6 4h10l4 4v10a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2v-12a2 2 0 0 1 2 -2', circle(12, 14, 2), 'M14 4l0 4l-6 0l0 -4']
HISTORY = ['M12 8l0 4l2 2', 'M3.05 11a9 9 0 1 1 .5 4m-.5 5v-5h5']
VIDEO = ['M15 10l4.553 -2.276a1 1 0 0 1 1.447 .894v6.764a1 1 0 0 1 -1.447 .894l-4.553 -2.276v-4z', 'M3 8a2 2 0 0 1 2 -2h8a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-8a2 2 0 0 1 -2 -2z']
SLIDERS = [circle(14, 6, 2), 'M4 6l8 0', 'M16 6l4 0', circle(8, 12, 2), 'M4 12l2 0', 'M10 12l10 0', circle(17, 18, 2), 'M4 18l11 0', 'M19 18l1 0']
BROADCAST = ['M18.364 19.364a9 9 0 1 0 -12.728 0', 'M15.536 16.536a5 5 0 1 0 -7.072 0', circle(12, 13, 1)]
RING = [circle(12, 12, 9)]

ICONS = {
    'all-scene-source-locked': LOCK,
    'all-scene-source-unlocked': LOCK_OPEN,
    'current-scene-source-locked': LOCK_SQUARE,
    'current-scene-source-unlocked': LOCK_SQUARE_OPEN,
    'refresh-browser-sources': GLOBE,
    'refresh-audio-monitoring': HEADPHONES,
    'camera': CAMERA,
    'video-capture-device-activate': CAMERA,
    'video-capture-device-deactivate': CAMERA_OFF,
    'video-capture-device-refresh': REFRESH,
    'add-sources-to-group': FOLDER_PLUS,
    'visible': EYE,
    'visible-off': EYE_OFF,
    'settings': COG,
    'studio-mode': COLUMNS,
    'pause': PAUSE,
    'save-replay': FLOPPY,
    'replay-buffer-off': HISTORY,
    'virtual-camera': VIDEO,
    'virtual-camera-settings': SLIDERS,
    'streaming-inactive': BROADCAST,
}
STATE = {
    'record-off': (RED, RING, ''),
    'record-on': (RED, RING, '<circle cx="12" cy="12" r="4.5" fill="%s" stroke="none"/>' % RED),
    'streaming': (BLUE, BROADCAST, ''),
    'virtual-camera-on': (GREEN, VIDEO, ''),
    'replay-buffer-on': (RED, HISTORY, ''),
}

def bbox(paths):
    xs0, xs1, ys0, ys1 = [], [], [], []
    for d in paths:
        x0, x1, y0, y1 = parse_path(d).bbox()
        xs0.append(x0); xs1.append(x1); ys0.append(y0); ys1.append(y1)
    h = STROKE / 2  # round caps and joins reach half the stroke past the path
    return min(xs0) - h, max(xs1) + h, min(ys0) - h, max(ys1) + h

def svg(ink, paths, extra='', dx=0.0, dy=0.0):
    body = ''.join(f'<path d="{d}"/>' for d in paths) + extra
    g = f'<g transform="translate({dx:.3f} {dy:.3f})">{body}</g>' if (dx or dy) else body
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" '
            f'stroke="{ink}" stroke-width="{STROKE}" stroke-linecap="round" stroke-linejoin="round">{g}</svg>\n')

report = []
def emit(name, paths, ink_for, extra=''):
    x0, x1, y0, y1 = bbox(paths)
    dx, dy = 12 - (x0 + x1) / 2, 12 - (y0 + y1) / 2
    for k in INK:
        open(os.path.join(out, f'{name}-{k}.svg'), 'w', newline='\n').write(svg(ink_for(k), paths, extra, dx, dy))
    report.append((name, x0, x1, y0, y1, dx, dy))

for name, paths in ICONS.items():
    emit(name, paths, lambda k: INK[k])
for name, (ink, paths, extra) in STATE.items():
    emit(name, paths, lambda k, ink=ink: ink, extra)

print(f'{"icon":34} {"bbox x":>13} {"bbox y":>13} {"was off (x,y) in 24 box":>26} {"at 18 px":>14}')
for name, x0, x1, y0, y1, dx, dy in report:
    print(f'{name:34} {x0:6.2f}-{x1:5.2f} {y0:6.2f}-{y1:5.2f} {-dx:12.2f},{-dy:6.2f}   {-dx*0.75:6.2f},{-dy*0.75:5.2f}')
