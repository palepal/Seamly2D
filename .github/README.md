# Cutalyst Pattern

Cutalyst Pattern is a fork of [Seamly2D](https://github.com/FashionFreedom/Seamly2D), the open source
patternmaking software, with additional features built on top: faster layouts, production exports and drafting
improvements. It can hand a pattern to Cutalyst, a separate app for sewing it in 3D.

Like Seamly2D, Cutalyst Pattern is free software under the GPLv3+ license.

___________________________________________________
### Relationship to Seamly2D

* Cutalyst Pattern follows Seamly2D's `develop` branch and merges its changes regularly.
* We try to keep Seamly2D's core features, its file formats (`.sm2d`, `.smis`, `.smms`) and the way patterns render
  and export the same wherever we can. Full parity isn't guaranteed: features only Cutalyst Pattern has can store
  data that Seamly2D doesn't read, and some behaviour differs.
* Cutalyst Pattern is an independent project, not affiliated with or endorsed by Seamly, LLC or Seamly Systems, Inc.
  Please report Cutalyst Pattern problems here, not to the Seamly2D project.
* Official Seamly2D downloads are on [seamly.io](https://seamly.io) and the
  [Seamly2D releases page](https://github.com/FashionFreedom/Seamly2D/releases/latest).

___________________________________________________
### What Cutalyst Pattern adds

**Layout and production**
* Layouts can be nested with the [Sparrow](https://github.com/JeroenGar/sparrow) engine, with a live preview of the
  run on the layout canvas.
* AAMA DXF export with turn points, curve points, a sew line layer and an option to export curves.
* Command line export that uses only the pieces marked *In layout*, with an `--inLayoutOnly` option.

**Drafting**
* A search bar for tools, points, curves, variables and help.
* Pick a tool by clicking the lines it draws.
* The notch angle can be edited for every notch type.

**Interface**
* A flat theme with themed SVG icons and a new icon set.
* Sideways scrolling with a trackpad, close buttons on floating toolbars and a visible toolbar overflow button.

Plus a number of bug fixes to drafting, seam allowances, exports and backups.

**Hand-off**
* `--drape x.pieces.json` writes a pattern's pieces to a documented JSON file for other programs, and File > Open in
  Cutalyst opens the pattern in Cutalyst, a separate app.

___________________________________________________
### Branches

* `local`: Seamly2D `develop` with every finished Cutalyst Pattern feature merged in. This is the branch to build.
* Each feature or fix also lives on its own branch, cut from Seamly2D `develop`, so it stays easy to merge.

___________________________________________________
### Building

Cutalyst Pattern builds like Seamly2D, see the [Developer README](README-DEVELOPER.md). On top of that:

* Clone with submodules: `git clone --recurse-submodules https://github.com/palepal/Seamly2D.git`
* Install Rust ([rustup.rs](https://rustup.rs)) so the build can compile Sparrow with `cargo`. Without it, Cutalyst
  Pattern still builds, just without the Sparrow layout engine.

___________________________________________________
### Additional components

Besides Seamly2D's own components (listed under Licensing), Cutalyst Pattern uses:

| Component | Used for | License | How it's included |
| :--- | :--- | :--- | :--- |
| [Sparrow](https://github.com/JeroenGar/sparrow) ([our fork](https://github.com/palepal/sparrow)) | Nesting pieces in layouts | [MIT](https://opensource.org/licenses/MIT) | Git submodule in `src/3rdparty/sparrow`, built with `cargo`. Optional. Bundled in the macOS app for now. |
| [Spectrum workflow icons](https://github.com/adobe/spectrum-css-workflow-icons) | Some interface icons | [Apache 2.0](https://apache.org/licenses/LICENSE-2.0), © Adobe | SVG copies in `src/libs/vmisc/share/resources/tsvg/spectrum` |

___________________________________________________
### Licensing

![GPLv3](./img/GPLv3_Logo-121x60px.png)

See [https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html) for more information.

Cutalyst Pattern is based on Seamly2D (Seamly, LLC and the Seamly2D contributors), which is based on Valentina (the
Valentina project). Their copyright notices are kept in the source.

Seamly2D components and licensing:
* QMuParser - [Simplified BSD license](https://opensource.org/licenses/BSD-2-Clause)
* VPropertyExplorer - [LGPLv2.1 license](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html)
* xerces-c - [Apache License, Version 2.0](https://apache.org/licenses/LICENSE-2.0)
