# Seamly icon style

Seamly's icons are single-colour line drawings that the icon engine (`src/libs/vmisc/themed_icon_engine.cpp`)
recolours for every theme and button state. They follow the measurements of Adobe Spectrum 2's workflow icons
(rounded strokes, one base glyph plus a corner badge). Generic UI icons are Spectrum's own files; the drafting and
pattern-making icons are drawn for Seamly.

## Spectrum first for generic UI

Where Spectrum 2's public workflow set (`@adobe/spectrum-css-workflow-icons` 5.0.0, Apache-2.0) has an icon for a
generic UI concept (files, edit, view, navigation, page orientation, info, warning, table, filter, keyboard…), Seamly
uses that file unchanged from `spectrum/`, next to Spectrum's LICENSE and COPYRIGHT. `SPECTRUM` in `make_icons.py`
lists which Seamly icon uses which Spectrum file. `TextDecrease.tsvg` is modified from Spectrum's `TextIncrease`
(arrow flipped), as its header notes. Seamly draws its own only where Spectrum has nothing: geometric
construction, pattern making, notches, units, f(x), and the drafting tools that mark their result in the accent.
Spectrum's strokes are about 1.5 px, Seamly's 1.25 px.

The icons are generated. Edit `make_icons.py` and run it; never edit the `.tsvg` files by hand.

    python3 make_icons.py

## Grid

- Canvas 20 × 20, `viewBox="0 0 20 20"`. Icons scale to 16, 20, 24 and 32 px in the app.
- Live area 2 to 18. The outer edge of a stroke may touch 2 or 18, so a stroke along the edge has its centre at
  2.75 or 17.25.
- Optical centre is 10, 10. Lines that run corner to corner use the standard diagonal from 3.75, 16.25 to 16.25, 3.75.

## Strokes and corners

- One stroke width: 1.25.
- Round caps and round joins everywhere.
- Corners: radius 1.5 (at the stroke centre) on shapes 12 px or larger, 1 on smaller ones. Construction geometry
  (triangles, polylines, notches) keeps sharp corners, softened only by the round join.
- No fills, except points, badge disks and arrow tips. A filled shape means "a point" or "a state", never decoration.

## Colour

Icons are drawn in three placeholder colours. The engine replaces them at paint time:

| In the file | Becomes | Use |
|---|---|---|
| `#000` | line colour: icon grey, text colour on hover and selection, half way to the background when disabled | everything by default |
| `#00ff7f` | theme accent (Seamly purple) | the result of a drafting tool, nothing else |
| `#fff` | panel background | rarely, to punch an object out of what lies behind it |

Inside a `<mask>` use only `white` and `rgb(0,0,0)`: the engine leaves those alone. `var(--iconPrimary…)` also
becomes the line colour, for any Spectrum file still in use.

### Accent rule

- Only drafting tools use the accent, and only on what the tool creates: the new point, line, curve, arc, notch or
  copy. One accent element per icon; inputs stay in the line colour.
- Generic UI (files, view, edit, lock, visibility, preferences) never uses the accent. These icons must work in a
  menu, where nothing is "created".
- Accent values per brightness level, all at the hue of the logo's `#503584` (261°), each at least 3:1 against the
  panel:

| Level | Panel | Accent | Contrast panel / tool tile |
|---|---|---|---|
| Darkest | `#323232` | `#a477f8` | 4.0 / 2.9 |
| Dark | `#535353` | `#bd99ff` | 3.4 / 2.8 |
| Light | `#b8b8b8` | `#4a0ac2` | 5.0 / 4.1 |
| Lightest | `#f0f0f0` | `#5516ca` | 7.8 / 4.8 |

## Drafting grammar

Seamly's old tool icons already told a construction story: dark existing points, a red new point, dashed helper
lines. The new set keeps that story in two colours.

| Element | Drawing | Primitive |
|---|---|---|
| existing point | filled dot, radius 1.5, line colour | `dot(x, y)` |
| created point | filled dot, radius 2, accent | `result(x, y)` |
| existing line, curve, arc | 1.25 stroke, line colour | `line`, `path`, `arc` |
| created line, curve, arc | 1.25 stroke, accent | `line(..., accent=True)` |
| construction line, axis, tangent, offset | dashed 2, gap 1.5, butt caps | `line(..., dashed=True)` |
| control point of a Bézier | small dot, radius 1.125, joined by a dashed handle | `handle(x, y)` |
| right angle | open square mark, 2.5 px | `square_mark` |

Points sit on the geometry they belong to. Dots are drawn last so they cover the line ends.

## Base glyph plus badge

Like Spectrum's Data, DataAdd, DataRefresh family, an icon that acts on a Seamly object is the object's glyph with
one badge in the bottom right corner.

- The badge disk has radius 4, centred on 15, 15.
- A gap of 1 is cut out of the base around the badge (mask radius 5), so the badge never touches the base.
- At most one badge. The badge is always bottom right; nothing else goes in that corner.
- **Solid badges** (filled disk, glyph knocked out) are states and quantities: `add`, `remove`, `clear`, `check`.
- **Line badges** (1.25 stroke, no disk) are actions: `edit`, `settings`, `search`, `export`, `import`, `back`,
  `refresh`, `lock`, `unlock`, `visible`, `time`, `rotate`, `move`, and the nouns `person`, `stack`, `piece`.

### Base glyphs (nouns)

| Glyph | Concept |
|---|---|
| `piece` | pattern piece (a bodice front block) |
| `file` | document |
| `folder` | folder |
| `sheet` | layout sheet with nested pieces |
| `draft` | draft block, draft mode (set square) |
| `tag` | label |
| `magnifier` | view, search |
| `notch` | a piece edge open for a notch |
| `gear` | settings |
| `person` | individual measurements |
| `curve`, `line`, `arc`, `point` | geometry |
| `grainline` | grainline (double-headed arrow) |
| `group` | group (bounds around objects) |
| `measurements` | measurements (tape measure) |

### Combination rules

| Meaning | Recipe |
|---|---|
| new X | X + `add` |
| delete / remove X | X + `remove` |
| clear X | X + `clear` |
| edit / rename X | X + `edit` |
| X settings, X preferences | X + `settings` |
| find in X, zoom to X | X + `search` |
| export X | X + `export` |
| import into X | X + `import` |
| X history, previous X | X + `back` or `time` |
| lock / unlock X | X + `lock` / `unlock` |
| show X | X + `visible` |
| X of a person (individual) / of many sizes (multisize) | X + `person` / `stack` |
| X belonging to a piece | X + `piece` |

When a tool is a geometric construction (bisector, normal, intersections), draw the construction with the
drafting grammar instead of using a badge.

## View tools

| Tool | Drawing |
|---|---|
| zoom in / out | Spectrum's ZoomIn / ZoomOut |
| zoom 1:1 | magnifier with 1:1 |
| zoom to fit | Spectrum's FullScreen |
| zoom to area | dashed rectangle + `search` |
| zoom to selection | a piece behind the magnifier, showing outside the glass |
| zoom previous | magnifier with a return arrow inside the lens |
| pan | Spectrum's Hand |

The drawn ones follow common CAD conventions (FreeCAD, QCAD, Inkscape).

## Text in icons

Only where the letters are the symbol: units (cm, mm, in, px), fx, search options (Aa, .*). Use `text()`, which
draws in the line colour with the system sans-serif.

## Requirements

Masks need Qt SVG 6.7 or newer.

## Adding an icon

1. Pick the base glyph and badge from the tables above, or draw the construction with the primitives.
2. Add a recipe to `ICONS` in `make_icons.py`, named after what it shows (or the PNG it replaces).
3. Run the script, add the new file to `tsvg.qrc` (the script rewrites it), and reference it as
   `:/tsvg/seamly/<name>.tsvg`.
4. Check it at 16 px on the darkest and lightest levels.
