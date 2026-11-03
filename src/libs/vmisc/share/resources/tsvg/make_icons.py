#!/usr/bin/env python3
"""Seamly icon set: components, combination rules and generator. The rules are in STYLE.md next to this file.

Every icon is a recipe built from the primitives, base glyphs and badges below. Running the script writes one
.tsvg per recipe into seamly/:

    python3 make_icons.py
"""
import itertools
import math
import os

LINE, ACCENT, BACKGROUND = '#000', '#00ff7f', '#fff'
STROKE = 1.25
DOT, RESULT = 1.5, 2   # point radius, created point radius
DASH = (2, 1.5)   # construction lines: dash, gap
BADGE_CENTER, BADGE_RADIUS, BADGE_GAP = 15, 4, 1
ids = itertools.count()


def num(value):
    """Shortest text for a coordinate."""
    text = f'{value:.2f}'.rstrip('0').rstrip('.')
    return '0' if text == '-0' else text


# --- primitives --------------------------------------------------------------------------------------------------

def path(d, accent=False, dashed=False, fill=False):
    """Stroked path in the line color, the accent, or dashed for construction lines."""
    attrs = ''
    if accent:
        attrs += f' stroke="{ACCENT}"'
    if dashed:
        attrs += f' stroke-dasharray="{num(DASH[0])} {num(DASH[1])}" stroke-linecap="butt"'
    if fill:
        attrs += f' fill="{ACCENT if accent else LINE}"'
    return f'<path d="{d}"{attrs}/>'


def line(x1, y1, x2, y2, **kw):
    return path(f'M{num(x1)} {num(y1)}L{num(x2)} {num(y2)}', **kw)


def polyline(*points, closed=False, **kw):
    d = 'M' + 'L'.join(f'{num(x)} {num(y)}' for x, y in points) + ('Z' if closed else '')
    return path(d, **kw)


def rect(x, y, w, h, r=1.5, **kw):
    return path(f'M{num(x + r)} {num(y)}H{num(x + w - r)}a{num(r)} {num(r)} 0 0 1 {num(r)} {num(r)}'
                f'V{num(y + h - r)}a{num(r)} {num(r)} 0 0 1 {num(-r)} {num(r)}H{num(x + r)}'
                f'a{num(r)} {num(r)} 0 0 1 {num(-r)} {num(-r)}V{num(y + r)}a{num(r)} {num(r)} 0 0 1 {num(r)} {num(-r)}Z',
                **kw)


def circle(cx, cy, r, **kw):
    return path(f'M{num(cx - r)} {num(cy)}a{num(r)} {num(r)} 0 1 0 {num(2 * r)} 0a{num(r)} {num(r)} 0 1 0 {num(-2 * r)} 0Z',
                **kw)


def polar(cx, cy, r, angle):
    """Point on a circle, angle in degrees, clockwise from 3 o'clock (SVG's y axis points down)."""
    return cx + r * math.cos(math.radians(angle)), cy + r * math.sin(math.radians(angle))


def arc_d(cx, cy, r, start, end):
    x1, y1 = polar(cx, cy, r, start)
    x2, y2 = polar(cx, cy, r, end)
    large = 1 if (end - start) % 360 > 180 else 0
    return f'M{num(x1)} {num(y1)}A{num(r)} {num(r)} 0 {large} 1 {num(x2)} {num(y2)}'


def arc(cx, cy, r, start, end, **kw):
    """Clockwise arc from start to end angle."""
    return path(arc_d(cx, cy, r, start, end), **kw)


def dot(x, y, r=None):
    """An existing point."""
    return f'<circle cx="{num(x)}" cy="{num(y)}" r="{num(r or DOT)}" fill="{LINE}" stroke="none"/>'


def handle(x, y):
    """A Bézier control point."""
    return dot(x, y, DOT * 0.75)


def accent_dot(x, y, r=1.25):
    return f'<circle cx="{num(x)}" cy="{num(y)}" r="{num(r)}" fill="{ACCENT}" stroke="none"/>'


def result(x, y):
    """The point a tool creates."""
    return f'<circle cx="{num(x)}" cy="{num(y)}" r="{num(RESULT)}" fill="{ACCENT}" stroke="none"/>'


def head(x, y, angle, size=2.75, **kw):
    """Open arrowhead with its tip at x, y, pointing along angle. Every arrow in the set uses it."""
    a = polar(x, y, size, angle + 180 - 45)
    b = polar(x, y, size, angle + 180 + 45)
    return polyline(a, (x, y), b, **kw)


def arrow(x1, y1, x2, y2, size=2.75, **kw):
    angle = math.degrees(math.atan2(y2 - y1, x2 - x1))
    return line(x1, y1, x2, y2, **kw) + head(x2, y2, angle, size, **kw)


def arc_arrow(cx, cy, r, start, end, size=2.5, **kw):
    """Clockwise arc with an arrowhead at its end."""
    x, y = polar(cx, cy, r, end)
    bx, by = polar(cx, cy, r, end - math.degrees(size * 0.7 / r))
    return arc(cx, cy, r, start, end, **kw) + head(x, y, math.degrees(math.atan2(y - by, x - bx)), size, **kw)


def chevron(cx, cy, angle, size=3.5):
    a = polar(cx, cy, size, angle + 135)
    b = polar(cx, cy, size, angle - 135)
    return polyline(a, (cx, cy), b)


def text(x, y, value, size=7, weight=600):
    """Letters, only where the letters are the symbol (units, PDF)."""
    return (f'<text x="{num(x)}" y="{num(y)}" font-family="sans-serif" font-size="{num(size)}" font-weight="{weight}" '
            f'text-anchor="middle" fill="{LINE}" stroke="none">{value}</text>')


def smooth(points):
    """Curve through the points (Catmull-Rom as cubic Béziers), like Seamly's curve-through-points tools."""
    pts = [points[0]] + list(points) + [points[-1]]
    d = f'M{num(points[0][0])} {num(points[0][1])}'
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        c1 = (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6)
        d += f'C{num(c1[0])} {num(c1[1])} {num(c2[0])} {num(c2[1])} {num(p2[0])} {num(p2[1])}'
    return d


def bezier_points(p0, p1, p2, p3, n=200):
    return [tuple((1 - t) ** 3 * a + 3 * (1 - t) ** 2 * t * b + 3 * (1 - t) * t ** 2 * c + t ** 3 * e
                  for a, b, c, e in zip(p0, p1, p2, p3)) for t in (i / n for i in range(n + 1))]


def crossings(poly, a, b):
    """Where the polyline poly crosses the infinite line a-b."""
    side = [(b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]) for x, y in poly]
    return [intersection(poly[i], poly[i + 1], a, b) for i in range(len(poly) - 1) if side[i] * side[i + 1] < 0]


def circle_crossings(c1, r1, c2, r2):
    d = math.dist(c1, c2)
    a = (r1 * r1 - r2 * r2 + d * d) / (2 * d)
    h = math.sqrt(r1 * r1 - a * a)
    mx, my = c1[0] + a * (c2[0] - c1[0]) / d, c1[1] + a * (c2[1] - c1[1]) / d
    return ((mx + h * (c2[1] - c1[1]) / d, my - h * (c2[0] - c1[0]) / d),
            (mx - h * (c2[1] - c1[1]) / d, my + h * (c2[0] - c1[0]) / d))


def line_circle(a, b, c, r):
    """Where the line a-b meets the circle c, r, the crossing nearer b first."""
    poly = [(c[0] + r * math.cos(t / 100), c[1] + r * math.sin(t / 100)) for t in range(0, 629)]
    return sorted(crossings(poly, a, b), key=lambda p: math.dist(p, b))


def intersection(p1, p2, p3, p4):
    """Where line p1-p2 meets line p3-p4."""
    (x1, y1), (x2, y2), (x3, y3), (x4, y4) = p1, p2, p3, p4
    d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / d
    return x1 + t * (x2 - x1), y1 + t * (y2 - y1)


def tangent_point(px, py, cx, cy, r, side=1):
    """Point where a tangent from p touches the circle c, r; side picks one of the two."""
    d = math.hypot(px - cx, py - cy)
    base = math.degrees(math.atan2(py - cy, px - cx))
    return polar(cx, cy, r, base + side * math.degrees(math.acos(r / d)))


def masked(content, holes):
    """Content with circular holes cut out of it, holes = [(cx, cy, r)]. Inside a mask only white and
    rgb(0,0,0) are safe: the icon engine recolors #000 and #fff."""
    mid = f'h{next(ids)}'
    cuts = ''.join(f'<circle cx="{num(x)}" cy="{num(y)}" r="{num(r)}" fill="rgb(0,0,0)" stroke="none"/>' for x, y, r in holes)
    return (f'<mask id="{mid}"><rect x="-1" y="-1" width="22" height="22" fill="white" stroke="none"/>{cuts}</mask>'
            f'<g mask="url(#{mid})">{content}</g>')


def cut(x, y):
    """A created point that splits the geometry under it: a gap around the point shows the cut."""
    return (x, y, RESULT + 1.25)


# --- base glyphs: Seamly's nouns -----------------------------------------------------------------------------------

PIECE = [(3.75, 7), (3.75, 17.25), (16.25, 17.25), (16.25, 9.75), (14.25, 3.75), (8, 2.75)]


def piece_d(scale=1.0, cx=10, cy=10):
    """Outline of a pattern piece (a bodice front block), scaled by scale and centered on cx, cy."""
    a, b, c, d, e, f = [(cx + (x - 10) * scale, cy + (y - 10) * scale) for x, y in PIECE]
    s = scale
    return (f'M{num(a[0])} {num(a[1])}V{num(b[1] - s)}q0 {num(s)} {num(s)} {num(s)}H{num(c[0] - s)}'
            f'q{num(s)} 0 {num(s)} {num(-s)}V{num(d[1])}'
            f'C{num(d[0] - 2.5 * s)} {num(d[1] - 0.25 * s)} {num(e[0] - 1.25 * s)} {num(e[1] + 2.5 * s)} {num(e[0])} {num(e[1])}'
            f'L{num(f[0])} {num(f[1])}C{num(f[0] - 0.25 * s)} {num(f[1] + 2.75 * s)} {num(a[0] + 2.5 * s)} {num(a[1])} '
            f'{num(a[0])} {num(a[1])}Z')


def piece(scale=1.0, cx=10, cy=10, rotate=0, **kw):
    p = path(piece_d(scale, cx, cy), **kw)
    return f'<g transform="rotate({rotate} {num(cx)} {num(cy)})">{p}</g>' if rotate else p


def file_glyph():
    return path('M5.75 2.75h5.5l4 4v9.5a1 1 0 0 1-1 1h-8.5a1 1 0 0 1-1-1v-12.5a1 1 0 0 1 1-1z'
                'M11 2.75v3a1 1 0 0 0 1 1h3.25')


def folder_glyph():
    return path('M2.75 15.25v-10.5a1 1 0 0 1 1-1h4l1.75 2h6.75a1 1 0 0 1 1 1v1.5'
                'M2.75 15.25l2.5-6.25a1 1 0 0 1 .95-.75h11.3a.75.75 0 0 1 .7 1l-2.45 6a1 1 0 0 1-.95.75h-11.05')


def sheet():
    """Layout sheet with pieces nested on it, one turned round."""
    return rect(2.75, 3.75, 14.5, 12.5, r=1) + piece(0.66, 7.1, 10) + piece(0.66, 12.9, 10, rotate=180)


def set_square():
    """Draft block: a set square."""
    return path('M3.75 2.75v14.5h14.5z') + path('M6.75 10v4.25h4.25z')


def magnifier(cx=8.5, cy=8.5, r=5.75, end=17.25):
    hx, hy = polar(cx, cy, r, 45)
    return circle(cx, cy, r) + line(hx, hy, end, end)


def eye(cx=10, cy=10, w=7.25, h=4.75, pupil=2.25):
    return (path(f'M{num(cx - w)} {num(cy)}Q{num(cx)} {num(cy - 2 * h)} {num(cx + w)} {num(cy)}'
                 f'Q{num(cx)} {num(cy + 2 * h)} {num(cx - w)} {num(cy)}Z') + circle(cx, cy, pupil))


def lock_glyph(open_=False):
    shackle = 'M7 9.25v-2.75a3 3 0 0 1 5.75-1.25' if open_ else 'M7 9.25v-2.75a3 3 0 0 1 6 0v2.75'
    return rect(4.75, 9.25, 10.5, 8, r=1.5) + path(shackle) + line(10, 12.5, 10, 14)


def gear(cx=10, cy=10, r=4.75, teeth=8, depth=1.75):
    pts = []
    for i in range(teeth * 4):
        angle = i * 360 / (teeth * 4) - 90
        rr = r + depth if i % 4 in (1, 2) else r
        pts.append(polar(cx, cy, rr, angle))
    return polyline(*pts, closed=True) + circle(cx, cy, r * 0.42)


def person(cx=10, top=2.75, height=14.5, **kw):
    """Head and shoulders."""
    s = height / 14.5
    return (circle(cx, top + 3.25 * s, 3.25 * s, **kw)
            + path(f'M{num(cx - 6 * s)} {num(top + height)}c0-3.6 {num(2.7 * s)} {num(-6 * s)} {num(6 * s)} {num(-6 * s)}'
                   f's{num(6 * s)} {num(2.4 * s)} {num(6 * s)} {num(6 * s)}', **kw))


def mannequin():
    """Dress form: a torso with shoulders, waist and hips, on a pole with a foot."""
    return (path('M7.5 3.25c-1.5.5-2.75 1.25-2.75 2.75 0 2 1.75 2.75 1.75 4.5 0 1.5-1.5 2-1.5 3.5h10'
                 'c0-1.5-1.5-2-1.5-3.5 0-1.75 1.75-2.5 1.75-4.5 0-1.5-1.25-2.25-2.75-2.75')
            + path('M7.5 3.25q2.5 1.25 5 0') + line(10, 14, 10, 17.25) + line(7, 17.25, 13, 17.25))


def roll():
    """A roll of paper with its sheet hanging down."""
    return (path('M4.75 2.75a2 2.5 0 1 0 0 5h10.5a2 2.5 0 1 0 0-5z') + path('M15.25 2.75a2 2.5 0 1 1 0 5')
            + path('M5.5 7.75v9.5h8.75v-9.5'))


def lens(content, cx, cy, r):
    """Content seen through a magnifier lens: clipped to the inside of the circle."""
    mid = f'l{next(ids)}'
    return (f'<mask id="{mid}"><circle cx="{num(cx)}" cy="{num(cy)}" r="{num(r)}" fill="white" stroke="none"/></mask>'
            f'<g mask="url(#{mid})">{content}</g>')


def spectrum_paths(name):
    """The drawing of a Spectrum icon (spectrum/, Apache-2.0), to combine with Seamly's own parts."""
    here = os.path.dirname(os.path.abspath(__file__))
    import re
    return ''.join(re.findall(r'<path[^>]*/>', open(os.path.join(here, 'spectrum', name + '.tsvg')).read()))


def units(label):
    """Spectrum's ruler with the unit written in the free corner."""
    return (spectrum_paths('Ruler').replace('<path ', '<path stroke="none" ')
            + text(14.25, 17.25, label, 5.5, 700))


def tape():
    """Tape measure: the case and its tape."""
    return (circle(8.25, 8.25, 5.5) + circle(8.25, 8.25, 1.25)
            + path('M8.25 13.75h8a1 1 0 0 1 1 1v1.5a1 1 0 0 1-1 1h-8M11.5 17.25v-1.5M14.5 17.25v-1.5'))


def tag():
    """Label tag."""
    return path('M2.75 4v5.1a1 1 0 0 0 .3.7l7.4 7.4a1 1 0 0 0 1.4 0l5.1-5.1a1 1 0 0 0 0-1.4l-7.4-7.4a1 1 0 0 0-.7-.3h-5.1'
                'a1 1 0 0 0-1 1z') + circle(6.5, 6.5, 1)


def clock(cx=10, cy=10, r=7.25):
    return circle(cx, cy, r) + polyline((cx, cy - r * 0.55), (cx, cy), (cx + r * 0.4, cy + r * 0.3))


def printer():
    return (path('M5.75 7.75v-4a1 1 0 0 1 1-1h6.5a1 1 0 0 1 1 1v4')
            + path('M5.25 14.25h-1.5a1 1 0 0 1-1-1v-4.5a1 1 0 0 1 1-1h12.5a1 1 0 0 1 1 1v4.5a1 1 0 0 1-1 1h-1.5')
            + rect(5.75, 11.25, 8.5, 6, r=1))


def floppy():
    return (path('M3.75 2.75h10l3.5 3.5v10a1 1 0 0 1-1 1h-12.5a1 1 0 0 1-1-1v-12.5a1 1 0 0 1 1-1z')
            + path('M6.25 2.75v3.5a.5.5 0 0 0 .5.5h5.5a.5.5 0 0 0 .5-.5v-3.5')
            + path('M5.75 17.25v-5a1 1 0 0 1 1-1h6.5a1 1 0 0 1 1 1v5'))


def image_glyph():
    return rect(2.75, 3.75, 14.5, 12.5) + path('M2.75 13.5l4-4 3.25 3.25 2-2 5.25 5') + circle(12.75, 7.5, 1.25)


def keyboard():
    keys = ''.join(f'M{num(x)} {num(y)}h.01' for y in (8.25, 11) for x in (5.75, 8.25, 10.75, 13.25))
    return rect(2.75, 5, 14.5, 10) + path(keys) + line(7, 12.75 + .5, 13, 12.75 + .5)


def monitor():
    return rect(2.75, 3.25, 14.5, 10) + line(10, 13.25, 10, 16.5) + line(6.75, 16.75, 13.25, 16.75)


def page(landscape=False):
    w, h = (14.5, 10.5) if landscape else (10.5, 14.5)
    x, y = 10 - w / 2, 10 - h / 2
    return path(f'M{num(x + 1)} {num(y)}h{num(w - 4.5)}l3.5 3.5v{num(h - 4.5)}a1 1 0 0 1-1 1h{num(-(w - 2))}'
                f'a1 1 0 0 1-1-1v{num(-(h - 2))}a1 1 0 0 1 1-1z')


def ruler():
    ticks = ''.join(f'M{num(x)} 11.75v{num(-2.25 if i % 2 == 0 else -1.25)}' for i, x in enumerate((5, 7.5, 10, 12.5, 15)))
    return rect(2.75, 11.75, 14.5, 5.5, r=1) + path(ticks.replace('11.75v', '17.25v').replace('v-', 'v-'))


def refresh(cx=10, cy=10, r=6.25):
    return (arc(cx, cy, r, 200, 330) + head(*polar(cx, cy, r, 330), 60, 2.5)
            + arc(cx, cy, r, 20, 150) + head(*polar(cx, cy, r, 150), 240, 2.5))


def letter_a(x=3, y=15.75, h=10.75):
    """A capital A, for the text-size icons."""
    w = h * 0.4
    return path(f'M{num(x)} {num(y)}L{num(x + w)} {num(y - h)}L{num(x + 2 * w)} {num(y)}'
                f'M{num(x + w * 0.45)} {num(y - h * 0.33)}H{num(x + w * 1.55)}')


def curve_d(x1=3.75, y1=15.75, x2=16.25, y2=4.25):
    """A soft S curve between two points."""
    return (f'M{num(x1)} {num(y1)}C{num(x1 + 5)} {num(y1 + 0.5)} {num(x2 - 7.5)} {num(y1 - 2)} '
            f'{num((x1 + x2) / 2)} {num((y1 + y2) / 2)}S{num(x2 - 5)} {num(y2 - 0.5)} {num(x2)} {num(y2)}')


# S curve through three points, for the tools that take several points
S_PTS = [(3.5, 15.5), (10, 10), (16.5, 4.5)]
S_PATH = 'M3.5 15.5C3.5 11 7 12.5 10 10S16.5 9 16.5 4.5'

BASES = {
    'piece': piece, 'file': file_glyph, 'folder': folder_glyph, 'sheet': sheet, 'draft': set_square,
    'tag': tag, 'magnifier': magnifier, 'gear': gear, 'person': person, 'image': image_glyph,
    'curve': lambda: path(curve_d()),
    'point': lambda: dot(10, 10) + path('M10 3.75v2.5M10 13.75v2.5M3.75 10h2.5M13.75 10h2.5'),
    'line': lambda: line(3.75, 16.25, 16.25, 3.75) + dot(3.75, 16.25) + dot(16.25, 3.75),
    'arc': lambda: arc(5.25, 14.75, 11, -90, 0) + dot(5.25, 14.75),
    'grainline': lambda: arrow(10, 10, 10, 2.75) + arrow(10, 10, 10, 17.25),
    'group': lambda: rect(2.75, 2.75, 14.5, 14.5) + rect(5.75, 5.75, 5, 5, r=1) + circle(12.25, 12.25, 2.5),
    'measurements': tape,
}


# --- badges: Seamly's verbs, bottom right ---------------------------------------------------------------------------

def solid(glyph):
    """Filled disk with the glyph knocked out of it."""
    c, r, mid = BADGE_CENTER, BADGE_RADIUS, f'k{next(ids)}'
    return (f'<mask id="{mid}"><circle cx="{c}" cy="{c}" r="{num(r)}" fill="white" stroke="none"/>'
            f'<path d="{glyph}" fill="none" stroke="rgb(0,0,0)" stroke-width="1.25" stroke-linecap="round"/></mask>'
            f'<rect x="10" y="10" width="10" height="10" fill="{LINE}" stroke="none" mask="url(#{mid})"/>')


BADGES = {
    # solid: states and quantities
    'add': lambda: solid('M15 13.25v3.5M13.25 15h3.5'),
    'remove': lambda: solid('M13.25 15h3.5'),
    'clear': lambda: solid('M13.75 13.75l2.5 2.5M16.25 13.75l-2.5 2.5'),
    'check': lambda: solid('M13.4 15.1l1.1 1.1 2.1-2.4'),
    # line: actions
    'edit': lambda: path('M11.75 18.25l.4-2 4.4-4.4a1.2 1.2 0 0 1 1.7 1.7l-4.4 4.4z'),
    'settings': lambda: gear(15, 15, 2.25, 6, 1),
    'search': lambda: circle(14.25, 14.25, 2.5) + line(16.1, 16.1, 18.5, 18.5),
    'export': lambda: arrow(12, 18, 18, 12, 2.5),
    'import': lambda: arrow(18, 12, 12.5, 17.5, 2.5),
    'back': lambda: path('M13.25 11.5l-1.75 1.75 1.75 1.75M11.5 13.25h3.75a2.5 2.5 0 0 1 0 5h-1.75'),
    'lock': lambda: rect(11.5, 14.5, 7, 4.5, r=1) + path('M13.25 14.5v-1a1.75 1.75 0 0 1 3.5 0v1'),
    'visible': lambda: eye(15, 15, 4, 2.5, 1.1),
    'time': lambda: clock(15, 15, 3.75),
    'person': lambda: person(15, 11, 8),
    'ruler': lambda: rect(11, 12.75, 8, 4.5, r=.75) + path('M13 12.75v1.5M15 12.75v2.25M17 12.75v1.5'),
    'piece': lambda: piece(0.5, 15, 15),
}


def icon(*parts, badge=None, gap=BADGE_GAP):
    """An icon: base parts, and an optional badge with a gap cut around it."""
    body = ''.join(parts)
    if badge:
        body = masked(body, [(BADGE_CENTER, BADGE_CENTER, BADGE_RADIUS + gap)]) + BADGES[badge]()
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20">'
            f'<g fill="none" stroke="{LINE}" stroke-width="{num(STROKE)}" stroke-linecap="round" stroke-linejoin="round">'
            f'{body}</g></svg>\n')


# --- drafting recipes -----------------------------------------------------------------------------------------------
# Inputs in the line color, what the tool creates (point, line, curve) in the accent, helper lines dotted.

A, B = (3.75, 16.25), (16.25, 3.75)   # the standard diagonal: lower left to upper right


def two_lines():
    """Two lines crossing at Seamly's angle, not a square X."""
    p1, p2, p3, p4 = (3.25, 4.5), (16.75, 14.75), (3.75, 15.75), (16.25, 3.5)
    return line(*p1, *p2) + line(*p3, *p4) + dot(*p1) + dot(*p2) + dot(*p3) + dot(*p4) + result(*intersection(p1, p2, p3, p4))


def height_icon():
    """A point, and the foot of its perpendicular on a line: the line is solid up to the foot, then dashed."""
    a, b, p = (3.75, 16.25), (16.25, 3.75), (4.25, 4.25)
    t = ((p[0] - a[0]) * (b[0] - a[0]) + (p[1] - a[1]) * (b[1] - a[1])) / math.dist(a, b) ** 2
    f = (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))
    return (line(*a, *f, accent=True) + line(*f, *b, dashed=True) + line(*p, *f, accent=True, dashed=True)
            + dot(*a) + dot(*b) + dot(*p) + result(*f))


def axis_icon():
    """A line, and an axis from a point at an angle, dashed in the accent up to where it meets the line."""
    a, b, p = (6, 3.25), (6, 16.75), (16.5, 7)
    x = intersection(a, b, p, (2.75, 12.25))
    return line(*a, *b) + line(*p, *x, accent=True, dashed=True) + dot(*a) + dot(*b) + dot(*p) + result(*x)


WAVE = [(3.5, 16.25), (7, 4), (13, 16), (16.5, 3.75)]   # Seamly's curve through points: a wave over four points
WAVE_MID = (10, 10)


def curves_icon():
    """Two curves crossing twice: the chosen crossing in the accent, the other one plain (Seamly showed it green)."""
    c1 = bezier_points((5, 2.75), (15.5, 7), (15.5, 13), (5, 17.25))
    c2 = bezier_points((15, 2.75), (4.5, 7), (4.5, 13), (15, 17.25))
    xs = sorted(crossings(c1, (10, 0), (10, 20)), key=lambda q: q[1])
    return (path('M5 2.75C15.5 7 15.5 13 5 17.25') + path('M15 2.75C4.5 7 4.5 13 15 17.25')
            + result(*xs[0]) + dot(*xs[-1]))


def curve_axis_icon():
    """An S curve and an axis from a point, dashed in the accent right across, as Seamly draws it."""
    poly = bezier_points((6.5, 2.75), (16.5, 6), (3.5, 14), (13.5, 17.25))
    p, far = (16.75, 15.5), (2.75, 3.5)
    x = sorted(crossings(poly, p, far), key=lambda q: math.dist(q, p))[0]
    return path('M6.5 2.75C16.5 6 3.5 14 13.5 17.25') + line(*p, *far, accent=True, dashed=True) + dot(*p) + result(*x)


def arc_line_icon():
    c, r = (13.5, 4.25), 8.5
    a, b = (3, 16.75), (16.75, 9.25)
    x = line_circle(a, b, c, r)[-1]
    return (arc(*c, r, 95, 200) + line(*a, *b) + line(*c, *x, accent=True, dashed=True)
            + dot(*a) + dot(*b) + dot(*c) + result(*x))


def circle_tangent_icon():
    """A point outside a circle and the two tangents from it, as Seamly draws it."""
    c, r, p = (7.75, 10), 4.5, (17, 10)
    t1, t2 = tangent_point(*p, *c, r, 1), tangent_point(*p, *c, r, -1)
    return (circle(*c, r, dashed=True) + line(*p, *t1, accent=True, dashed=True) + line(*p, *t2, accent=True, dashed=True)
            + dot(*c) + dot(*p) + result(*t1) + result(*t2))


def arc_tangent_icon():
    c, r, p = (5, 14.5), 9.5, (17.25, 15.25)
    t = tangent_point(*p, *c, r, -1)
    far = (p[0] + (t[0] - p[0]) * 1.45, p[1] + (t[1] - p[1]) * 1.45)
    return arc(*c, r, -95, 10) + line(*p, *far, accent=True, dashed=True) + dot(*p) + result(*t)


def mirror(axis):
    left = polyline((8, 4.75), (8, 15.25), (2.75, 15.25), closed=True)
    right = polyline((12, 4.75), (12, 15.25), (17.25, 15.25), closed=True, accent=True)
    axes = {'line': line(10, 2.75, 10, 17.25) + handle(10, 2.75) + handle(10, 17.25),
            'axis': line(10, 2.75, 10, 17.25, dashed=True) + handle(10, 10), None: ''}
    return left + right + axes[axis]


def union_path():
    """Outline of a square merged with a circle over its lower right corner, as one path."""
    x0, y0, x1, y1, r = 2.75, 3.75, 12.25, 13.25, 1.5
    c, cr = (12.5, 12.75), 4.75
    top = (x1, c[1] - math.sqrt(cr * cr - (x1 - c[0]) ** 2))      # circle meets the right edge
    left = (c[0] - math.sqrt(cr * cr - (y1 - c[1]) ** 2), y1)      # circle meets the bottom edge
    return (f'M{num(left[0])} {num(left[1])}H{num(x0 + r)}a{r} {r} 0 0 1 -{r} -{r}V{num(y0 + r)}a{r} {r} 0 0 1 {r} -{r}'
            f'H{num(x1 - r)}a{r} {r} 0 0 1 {r} {r}V{num(top[1])}A{num(cr)} {num(cr)} 0 1 1 {num(left[0])} {num(left[1])}Z')


UNION = union_path()


def outline_points(n=24):
    """The piece outline as points, following the same segments as piece_d."""
    a, b, c, d, e, f = PIECE
    pts = [a, (b[0], b[1] - 1), (b[0] + 1, b[1]), (c[0] - 1, c[1]), (c[0], c[1] - 1), d]
    pts += bezier_points(d, (d[0] - 2.5, d[1] - 0.25), (e[0] - 1.25, e[1] + 2.5), e, n)[1:]
    pts += [f] + bezier_points(f, (f[0] - 0.25, f[1] + 2.75), (a[0] + 2.5, a[1]), a, n)[1:-1]
    return pts


def offset(points, d):
    """Polygon moved inwards by d along its vertex normals (clockwise polygon, y down)."""
    out, n = [], len(points)
    for i, (x, y) in enumerate(points):
        (px, py), (nx, ny) = points[i - 1], points[(i + 1) % n]
        tx, ty = nx - px, ny - py
        length = math.hypot(tx, ty) or 1
        out.append((x - ty / length * d, y + tx / length * d))
    return out


def seam_allowance():
    """Cut line solid, seam line dashed inside it at an even distance."""
    inner = offset(outline_points(), -2.25)
    return piece() + path('M' + 'L'.join(f'{num(x)} {num(y)}' for x, y in inner) + 'Z', dashed=True)


def graded():
    """Three sizes of a piece stacked at the center front waist, the smaller ones in front and opaque."""
    out = ''
    for s in (1.0, 0.76, 0.52):
        cx = 3.75 - (3.75 - 10) * s
        cy = 17.25 - (17.25 - 10) * s
        out += path(piece_d(s, cx, cy)).replace('/>', f' fill="{BACKGROUND}"/>')
    return out


DRAFTING = {
    'point_basepoint_icon': icon(arrow(8, 12, 16.25, 3.75), circle(5.5, 14.5, 2.6), result(5.5, 14.5)),
    'segment': icon(line(*A, *B, accent=True), dot(*A), result(*B)),
    'line': icon(line(*A, *B, accent=True), dot(*A), dot(*B)),
    'along_line': icon(line(*A, *B), line(*A, 7.9, 12.1, accent=True), dot(*A), dot(*B), result(7.9, 12.1)),
    'midpoint': icon(line(*A, *B), dot(*A), dot(*B), result(10, 10)),
    'normal': icon(line(6, 16, 16.75, 16), line(6, 16, 6, 4, accent=True), dot(6, 16), dot(16.75, 16), result(6, 4)),
    'bisector': icon(line(3.75, 16.25, 16.25, 16.25), line(3.75, 16.25, 11, 3.75), line(3.75, 16.25, 16, 9.25, accent=True),
                     dot(16.25, 16.25), dot(11, 3.75), dot(3.75, 16.25), result(16, 9.25)),
    'shoulder': icon(line(3.75, 16.25, 16.25, 6.25), line(3.75, 6.25, 13.75, 8.25, dashed=True),
                     line(3.75, 16.25, 13.75, 8.25, accent=True), dot(3.75, 16.25), dot(16.25, 6.25), dot(3.75, 6.25),
                     result(13.75, 8.25)),
    'height': icon(height_icon()),
    'triangle': icon(line(2.75, 12.5, 17.25, 12.5, dashed=True), polyline((3.75, 6.25), (10, 12.5), (16.25, 6.25)),
                     dot(3.75, 6.25), dot(16.25, 6.25), result(10, 12.5)),
    'point_intersectxy_icon': icon(line(4.25, 4.25, 15.75, 4.25, dashed=True), line(4.25, 4.25, 4.25, 15.75, dashed=True),
                                   dot(15.75, 4.25), dot(4.25, 15.75), result(4.25, 4.25)),
    'line_intersect_axis': icon(axis_icon()),
    'intersect': icon(two_lines()),
    'spline': icon(path(curve_d(), accent=True), dot(3.75, 15.75), dot(16.25, 4.25)),
    'cubic_bezier': icon(line(3.75, 16.25, 3.75, 7.25), line(16.25, 3.75, 16.25, 12.75),
                         path('M3.75 16.25C3.75 7.25 16.25 12.75 16.25 3.75', accent=True),
                         dot(*A), dot(*B), handle(3.75, 7.25), handle(16.25, 12.75)),
    'cubic_bezier_path': icon(line(2.75, 16.5, 2.75, 11.5), line(7.25, 6.75, 12.75, 13.25), line(17.25, 3.5, 17.25, 8.5),
                              path('M2.75 16.5C2.75 11.5 7.25 6.75 10 10S17.25 8.5 17.25 3.5', accent=True),
                              dot(2.75, 16.5), dot(10, 10), dot(17.25, 3.5), handle(7.25, 6.75), handle(12.75, 13.25)),
    'splinePath': icon(path(smooth(WAVE), accent=True), *[dot(*p) for p in WAVE]),
    'curve_control_points': icon(path('M3.75 16.25C3.75 7.25 16.25 12.75 16.25 3.75'), line(3.75, 16.25, 3.75, 7.25),
                                 line(16.25, 3.75, 16.25, 12.75), path('M2.75 6.25h2v2h-2zM15.25 11.75h2v2h-2z')),
    'spline_cut_point': icon(masked(path(curve_d()), [cut(10, 10)]), result(10, 10)),
    'splinePath_cut_point': icon(masked(path(smooth(WAVE)), [cut(*WAVE_MID)]), result(*WAVE_MID)),
    'intersection_curves': icon(curves_icon()),
    'curve_intersect_axis': icon(curve_axis_icon()),
    'arc': icon(arc(5.25, 14.75, 11, -90, 0, accent=True), dot(5.25, 14.75)),
    'arc_cut': icon(masked(arc(4.5, 15.5, 11.5, -90, 0), [cut(*polar(4.5, 15.5, 11.5, -45))]),
                    result(*polar(4.5, 15.5, 11.5, -45))),
    'arc_with_length': icon(arc(10, 10, 7.25, 90, 360), arc_arrow(10, 10, 3.75, 100, 330, 2),
                            line(10, 10, 17.25, 10, accent=True, dashed=True), result(10, 10)),
    'arc_intersect_axis': icon(arc(4.5, 15.5, 11.5, -90, 0), line(16.75, 2.75, *polar(4.5, 15.5, 11.5, -30), accent=True, dashed=True),
                               dot(16.75, 2.75), result(*polar(4.5, 15.5, 11.5, -30))),
    'el_arc': icon(path('M9.5 2.75A5.75 7.25 0 0 1 9.5 17.25', accent=True), dot(7, 10)),
    'point_intersect_arc_line': icon(arc_line_icon()),
    'point_of_intersection_arcs': icon(arc(2.25, 10, 8.75, -60, 60), arc(17.75, 10, 8.75, 120, 240), dot(4, 10), dot(16, 10),
                                       *[result(*p) for p in circle_crossings((2.25, 10), 8.75, (17.75, 10), 8.75)]),
    'point_of_intersection_circles': icon(circle(7.5, 11.5, 4.75, dashed=True), circle(12.5, 8.5, 4.75, dashed=True),
                                          *[result(*p) for p in circle_crossings((7.5, 11.5), 4.75, (12.5, 8.5), 4.75)]),
    'point_from_circle_and_tangent': icon(circle_tangent_icon()),
    'point_from_arc_and_tangent': icon(arc_tangent_icon()),
    'true_darts': icon(line(2.75, 5.5, 17.25, 5.5), polyline((6.5, 5.5), (10, 16.5), (13.5, 5.5)),
                       dot(10, 16.5), result(6.5, 5.5), result(13.5, 5.5)),
    'rotation': icon(arc_arrow(10, 10, 6.5, -60, 210, 2.75), result(10, 10)),
    'mirror_by_line': icon(mirror('line')),
    'mirror_by_axis': icon(mirror('axis')),
    'move': icon(arrow(6.5, 13.5, 13.5, 6.5), dot(4.75, 15.25), result(15.25, 4.75)),
    'union': icon(path(UNION, accent=True)),
    'group': icon(BASES['group']()),
    'origin': icon(arrow(4.25, 15.75, 16.75, 15.75), arrow(4.25, 15.75, 4.25, 3.25), result(4.25, 15.75)),
}

# --- everything else ------------------------------------------------------------------------------------------------

plus = path('M10 3.75v12.5M3.75 10h12.5')
minus = path('M3.75 10h12.5')

UI = {
    # files
    'document-new': icon(file_glyph(), badge='add'),
    'document-open': icon(folder_glyph()),
    'document-save': icon(floppy()),
    'document-save-as': icon(floppy(), badge='edit'),
    'document-print': icon(printer()),
    'document-print-preview': icon(printer(), badge='search'),
    'printer_icon': icon(printer()),
    'export': icon(path('M9.25 3.75h-4.5a2 2 0 0 0-2 2v9.5a2 2 0 0 0 2 2h9.5a2 2 0 0 0 2-2v-4.5'), arrow(9.5, 10.5, 16.75, 3.25)),
    'pdf_icon': icon(file_glyph(), text(10, 14.5, 'PDF', 4.5, 700)),
    'seamly2d_file': icon(file_glyph(), badge='piece'),
    'individual_size_file': icon(person(), badge='ruler'),
    'multisize_size_file': icon(graded()),
    'template_size_file': icon(person(dashed=True)),
    'backup_files': icon(file_glyph(), badge='time'),
    'body_scan': icon(path('M2.75 6.25v-2.5a1 1 0 0 1 1-1h2.5M13.75 2.75h2.5a1 1 0 0 1 1 1v2.5'
                           'M17.25 13.75v2.5a1 1 0 0 1-1 1h-2.5M6.25 17.25h-2.5a1 1 0 0 1-1-1v-2.5'), person(10, 5.25, 9.5)),
    'application-exit': icon(path('M8.25 3.75h-3.5a1 1 0 0 0-1 1v10.5a1 1 0 0 0 1 1h3.5'), arrow(8.5, 10, 16.75, 10)),
    # edit
    'add': icon(plus),
    'list-add': icon(plus),
    'orange_plus': icon(plus), 'red_plus': icon(plus),
    'remove': icon(minus),
    'list-remove': icon(minus),
    'edit': icon(path('M3.75 16.25l.75-3.75 9-9a1.77 1.77 0 0 1 2.5 2.5l-9 9z'), path('M12 5l3 3')),
    'trashcan': icon(path('M2.75 5.25h14.5M7.75 5.25v-1.5a1 1 0 0 1 1-1h2.5a1 1 0 0 1 1 1v1.5'),
                     path('M4.75 5.25l.8 11.1a1 1 0 0 0 1 .9h6.9a1 1 0 0 0 1-.9l.8-11.1'), path('M8.25 8.5v5.5M11.75 8.5v5.5')),
    'edit-delete': icon(path('M2.75 5.25h14.5M7.75 5.25v-1.5a1 1 0 0 1 1-1h2.5a1 1 0 0 1 1 1v1.5'),
                        path('M4.75 5.25l.8 11.1a1 1 0 0 0 1 .9h6.9a1 1 0 0 0 1-.9l.8-11.1'), path('M8.25 8.5v5.5M11.75 8.5v5.5')),
    'clear': icon(circle(10, 10, 7.25), path('M7.25 7.25l5.5 5.5M12.75 7.25l-5.5 5.5')),
    'edit-undo': icon(path('M6.75 4.25l-3 3 3 3M3.75 7.25h8.5a4 4 0 0 1 0 8h-3.5')),
    'edit-redo': icon(path('M13.25 4.25l3 3-3 3M16.25 7.25h-8.5a4 4 0 0 0 0 8h3.5')),
    'clipboard_icon': icon(path('M7 4.25h-1.25a1 1 0 0 0-1 1v11a1 1 0 0 0 1 1h8.5a1 1 0 0 0 1-1v-11a1 1 0 0 0-1-1H13'),
                           rect(7, 2.75, 6, 3, r=.75), path('M7.5 9.5h5M7.5 12.5h5')),
    'history': icon(arc(10, 10, 7.25, 210, 150), head(*polar(10, 10, 7.25, 210), 120, 2.5),
                    polyline((10, 6), (10, 10), (12.75, 11.75))),
    'reverse': icon(arrow(3.75, 6.75, 16.25, 6.75), arrow(16.25, 13.25, 3.75, 13.25)),
    'clockwise': icon(arc_arrow(10, 10, 6.5, 150, 60)),
    'color_picker': icon(path('M3.5 16.5l.75-2.75 7-7 2 2-7 7z'), path('M10.5 5.5l4 4'),
                         path('M12.5 7.5l2.25-2.25a1.6 1.6 0 0 1 2.25 2.25l-2.25 2.25')),
    'invert_selection': icon(rect(2.75, 2.75, 14.5, 14.5, dashed=True), path('M5.75 5.75h8.5l-8.5 8.5z', fill=True)),
    'alignment': icon(rect(2.75, 2.75, 14.5, 14.5, dashed=True), path('M10 6.5v7M6.5 10h7'), dot(10, 10)),
    'equal': icon(path('M4.25 7.5h11.5M4.25 12.5h11.5')),
    'properties': icon(path('M3.75 5.75h12.5M3.75 10h12.5M3.75 14.25h12.5'), circle(7, 5.75, 1.75), circle(13, 10, 1.75),
                       circle(8.5, 14.25, 1.75)),
    'filter': icon(path('M3 4h14l-5.25 6.25v5.75l-3.5-1.75v-4z')),
    'table': icon(rect(2.75, 3.75, 14.5, 12.5), path('M2.75 8h14.5M2.75 12.25h14.5M7.5 3.75v12.5M12.25 3.75v12.5')),
    'calculator': icon(rect(4.25, 2.75, 11.5, 14.5), rect(6.5, 5, 7, 3, r=.5),
                       path(''.join(f'M{x} {y}h.01' for y in (11, 14) for x in (7, 10, 13)))),
    'fx': icon(path('M7.75 3.75C6.25 3.1 5.1 3.8 4.9 5.25L3.6 15.5C3.4 16.9 2.4 17.4 1.5 17.1M2.75 8.25H7'),
               path('M10.25 6.75Q8.5 10.5 10.25 14.25M17.25 6.75Q19 10.5 17.25 14.25M11.75 8l4 5M15.75 8l-4 5')),
    'insert_fx': icon(path('M7.75 3.75C6.25 3.1 5.1 3.8 4.9 5.25L3.6 15.5C3.4 16.9 2.4 17.4 1.5 17.1M2.75 8.25H7'),
                      path('M10.25 6.75Q8.5 10.5 10.25 14.25M17.25 6.75Q19 10.5 17.25 14.25M11.75 8l4 5M15.75 8l-4 5'),
                      badge='add'),
    'case': icon(path('M2.5 15l3.75-10 3.75 10M3.9 11.5h4.7'), circle(14, 12.5, 2.5), line(16.5, 10, 16.5, 15)),
    'regex': icon(dot(5, 15, 1.25), path('M13 4v8M9.5 6l7 4M16.5 6l-7 4')),
    'word': icon(path('M3 13.5v2.5h14v-2.5'), circle(7, 8.5, 2.25), line(9.25, 6.25, 9.25, 10.75), line(11, 3.75, 11, 10.75),
                 circle(13.25, 8.5, 2.25)),
    'go-next': icon(chevron(12.5, 10, 0)),
    'go-previous': icon(chevron(7.5, 10, 180)),
    'go-up': icon(chevron(10, 7.5, -90)),
    'go-down': icon(chevron(10, 12.5, 90)),
    'go-top': icon(chevron(10, 9.25, -90, 4.5), line(4.5, 4.75, 15.5, 4.75)),
    'go-bottom': icon(chevron(10, 10.75, 90, 4.5), line(4.5, 15.25, 15.5, 15.25)),
    'list-move': icon(arrow(7, 15.5, 7, 4.5), arrow(13, 4.5, 13, 15.5)),
    'left_to_right_arrow': icon(arrow(3.25, 10, 16.75, 10)),
    'right_to_left_arrow': icon(arrow(16.75, 10, 3.25, 10)),
    'fast_forward_left_to_right_arrow': icon(chevron(9, 10, 0), chevron(15, 10, 0)),
    'fast_forward_right_to_left_arrow': icon(chevron(11, 10, 180), chevron(5, 10, 180)),
    'syncM': icon(refresh()),
    # view
    'find': icon(magnifier()),
    'zoom_in': icon(magnifier(), path('M8.5 6v5M6 8.5h5')),
    'zoom_out': icon(magnifier(), path('M6 8.5h5')),
    'zoom_100_percent': icon(magnifier(), path('M5.5 7l1.25-1v5M10 7l1.25-1v5'), dot(8.4, 7.5, .65), dot(8.4, 10.25, .65)),
    'zoom_to_area': icon(rect(2.75, 2.75, 14.5, 14.5, dashed=True), badge='search'),
    'zoom_to_fit': icon(arrow(7.5, 7.5, 3.25, 3.25, 2.5), arrow(12.5, 7.5, 16.75, 3.25, 2.5),
                        arrow(7.5, 12.5, 3.25, 16.75, 2.5), badge='search'),
    'zoom_to_selection': icon(piece(0.75, 9, 9.75), magnifier()),
    'zoom_to_previous': icon(magnifier(), path('M7 5.75L5.25 7.5 7 9.25M5.25 7.5H9a1.75 1.75 0 0 1 0 3.5H7.75')),
    'zoom_to_object': icon(circle(10, 10, 6), path('M10 2.75v4M10 13.25v4M2.75 10h4M13.25 10h4'), badge='search'),
    'zoom_pan': icon(path('M7.25 10.5v-5.5a1.25 1.25 0 0 1 2.5 0v4.5M9.75 9.5v-6a1.25 1.25 0 0 1 2.5 0v6'
                          'M12.25 9.5v-4.5a1.25 1.25 0 0 1 2.5 0v6.5c0 3.5-2 5.75-5.25 5.75c-2.5 0-3.6-1.2-4.9-3.1'
                          'l-2-3a1.3 1.3 0 0 1 2.1-1.5l1.55 1.85')),
    'arrow_cursor': icon(path('M5.25 2.75v12.5l3.25-3 2.4 5 2.1-1-2.3-4.9 4.5-.35z')),
    'visible_on': icon(eye()),
    'open_eye': icon(eye()),
    'visible_hover': icon(eye()),
    'visible_off': icon(eye(), path('M3.75 3.75l12.5 12.5')),
    'lock_on': icon(lock_glyph()),
    'padlock_locked': icon(lock_glyph()),
    'lock_off': icon(lock_glyph(open_=True)),
    'padlock_opened': icon(lock_glyph(open_=True)),
    'labels': icon(tag(), badge='visible'),
    'increase_size': icon(letter_a(), arrow(15.25, 15.25, 15.25, 4.25, 2.25)),
    'decrease_size': icon(letter_a(), arrow(15.25, 4.25, 15.25, 15.25, 2.25)),
    'pen_color': icon(path('M4.75 13.25l.5-2.5 6.5-6.5a1.5 1.5 0 0 1 2.1 2.1l-6.5 6.5z'), line(2.75, 17, 17.25, 17)),
    'wireframe': icon(piece(), *[accent_dot(*p) for p in PIECE]),
    'show_info_icon': icon(circle(10, 10, 7.25), line(10, 9, 10, 14), dot(10, 6.25, .9)),
    'show_info16_icon': icon(circle(10, 10, 7.25), line(10, 9, 10, 14), dot(10, 6.25, .9)),
    'info': icon(circle(10, 10, 7.25), line(10, 9, 10, 14), dot(10, 6.25, .9)),
    'dialog-warning': icon(path('M10 2.75L17.5 16.25H2.5Z'), line(10, 7.5, 10, 11.25), dot(10, 13.75, .9)),
    'warning': icon(path('M10 2.75L17.5 16.25H2.5Z'), line(10, 7.5, 10, 11.25), dot(10, 13.75, .9)),
    'preferences-other': icon(gear()),
    'config': icon(gear()),
    'graphics_view_config': icon(monitor(), badge='settings'),
    'path_config': icon(folder_glyph(), badge='settings'),
    'pattern_config': icon(piece()),
    '2d_key_icon': icon(keyboard()),
    'me_key_icon': icon(keyboard()),
    'image_menu_icon': icon(image_glyph()),
    'add_image': icon(image_glyph(), badge='add'),
    # modes and tool groups
    'draw': icon(set_square()),
    'new_draw': icon(set_square(), badge='add'),
    'option_draw': icon(set_square(), badge='settings'),
    'kontur': icon(piece()),
    'details_toolbox': icon(piece()),
    'patternpiece_toolbox': icon(piece(), *[dot(*p, 1.1) for p in PIECE]),
    'layout': icon(sheet()),
    'layout_toolbox': icon(sheet()),
    'layout_settings': icon(sheet(), badge='settings'),
    'points_toolbox': icon(BASES['point']()),
    'lines_toolbox': icon(BASES['line']()),
    'spline_toolboxs': icon(path(curve_d()), dot(3.75, 15.75), dot(16.25, 4.25)),
    'arcs_toolbox': icon(BASES['arc']()),
    'operations_toolbox': icon(mirror(None)),
    # pieces
    'new_piece': icon(piece(), badge='add'),
    'new_detail': icon(piece(), badge='add'),
    'insert_nodes_icon': icon(piece(), badge='import'),
    'seam_allowance': icon(seam_allowance()),
    'grainline': icon(BASES['grainline']()),
    'grainlines': icon(BASES['grainline']()),
    'path': icon(piece(), path('M6.5 14.5C8 9 11 12 13.5 7', accent=True)),
    'paths': icon(piece(), path('M6.5 14.5C8 9 11 12 13.5 7')),
    'anchor_point': icon(piece(), path('M10 8v7M6.5 11.5h7'), result(10, 11.5)),
    'piece_label': icon(tag(), badge='piece'),
    'pattern_label': icon(tag(), path('M9 9.5h4.5M11.25 9.5v4.75')),
    'measurement': icon(tape()),
    'mannequin': icon(mannequin()),
    # layout pages and units
    'landscape': icon(page(True)),
    'portrait': icon(page()),
    'template': icon(page(), rect(6.75, 6.75, 6.5, 7.5, r=.5, dashed=True)),
    'roll': icon(roll()),
    'units_cm_on': icon(units('cm')),
    'units_mm_on': icon(units('mm')),
    'units_in_on': icon(units('in')),
    'units_px_on': icon(units('px')),
}


def notch(shape):
    """Seamly's notch icons: the piece edge across the top, the notch shape under it."""
    edge = line(2.75, 5, 17.25, 5)
    if shape == 'diamond':
        return line(2.75, 10, 17.25, 10) + path('M10 3.25l3.25 6.75-3.25 6.75-3.25-6.75z', accent=True)
    shapes = {'slit': 'M8.75 5v9.5M11.25 5v9.5', 't': 'M10 5v9.5M6.75 14.5h6.5', 'u': 'M6.75 5v6.25a3.25 3.25 0 0 0 6.5 0V5',
              'internal_v': 'M6.5 14.5L10 5l3.5 9.5', 'external_v': 'M6.5 5l3.5 9.5L13.5 5', 'castle': 'M6.75 5v9.5h6.5V5'}
    return edge + path(shapes[shape], accent=True)


NOTCHES = {f'{s}_notch': icon(notch(s)) for s in ('slit', 't', 'u', 'internal_v', 'external_v', 'castle', 'diamond')}
NOTCHES['notches'] = NOTCHES['slit_notch']

ICONS = {**DRAFTING, **UI, **NOTCHES}

# variants shown next to each other on the review page, not used by the app
VARIANTS = {'mirror_no_axis': icon(mirror(None))}

# Generic UI icons that Spectrum 2's public workflow set (Apache-2.0, spectrum/LICENSE) already has, identical or close:
# the app uses Spectrum's file from spectrum/, unchanged. The recipe above stays for comparison. Drafting tools keep
# Seamly's own drawing even where Spectrum has something near, because they mark the result in the accent.
SPECTRUM = {
    'document-new': 'FileAdd', 'document-open': 'FolderOpen', 'document-save': 'SaveFloppy', 'document-print': 'Print',
    'printer_icon': 'Print', 'document-print-preview': 'Preview', 'export': 'ExportTo', 'application-exit': 'Leave',
    
    'edit-undo': 'Undo', 'edit-redo': 'Redo', 'trashcan': 'Delete', 'edit-delete': 'Delete', 'edit': 'Edit',
    'history': 'History', 'clear': 'CloseCircle', 'add': 'AddCircle', 'list-add': 'AddCircle',
    'orange_plus': 'AddCircle', 'red_plus': 'AddCircle', 'remove': 'RemoveCircle', 'list-remove': 'RemoveCircle',
    'reverse': 'Switch', 'clockwise': 'RotateCW', 'syncM': 'Refresh', 'color_picker': 'Eyedropper',
    'invert_selection': 'Invert', 'alignment': 'AlignCenter', 'properties': 'Properties', 'filter': 'Filter',
    'table': 'Table', 'go-next': 'ChevronRight', 'go-previous': 'ChevronLeft', 'go-up': 'ChevronUp', 'go-down': 'ChevronDown',
    'find': 'Search', 'zoom_in': 'ZoomIn', 'zoom_out': 'ZoomOut', 'zoom_pan': 'Hand', 'zoom_to_fit': 'FullScreen',
    'arrow_cursor': 'Select',
    'visible_on': 'Visibility', 'open_eye': 'Visibility', 'visible_hover': 'Visibility', 'visible_off': 'VisibilityOff',
    'lock_on': 'Lock', 'padlock_locked': 'Lock', 'lock_off': 'LockOpen', 'padlock_opened': 'LockOpen',
    'increase_size': 'TextIncrease', 'decrease_size': 'TextDecrease', 'pen_color': 'PenBrush',
    'show_info_icon': 'InfoCircle', 'show_info16_icon': 'InfoCircle', 'info': 'InfoCircle',
    'dialog-warning': 'AlertTriangle', 'warning': 'AlertTriangle', 'preferences-other': 'Settings', 'config': 'Settings',
    'graphics_view_config': 'DeviceDesktop', 'path_config': 'Folder', '2d_key_icon': 'Keyboard', 'me_key_icon': 'Keyboard',
    'image_menu_icon': 'Image', 'add_image': 'ImageAdd', 'group': 'Group',
    'landscape': 'OrientationLandscape', 'portrait': 'OrientationPortrait', 'lines_toolbox': 'Line',
    'operations_toolbox': 'TransformGeneric', 'template': 'File',
}


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, 'seamly')
    os.makedirs(out, exist_ok=True)
    for name, svg in ICONS.items():
        if name in SPECTRUM:   # the app uses Spectrum's file
            continue
        with open(os.path.join(out, name + '.tsvg'), 'w') as fh:
            fh.write(svg)
    print(len(ICONS) - len(SPECTRUM), 'icons written,', len(SPECTRUM), 'taken from Spectrum')


if __name__ == '__main__':
    main()
