# Dot icons

Six 11x11 weather assets are derived from the vendored Lucide SVGs.
`WeatherDotIcons.h` includes upstream license notices. Generation runs locally.

## Generation

```sh
python3 -m venv .cache/dot-icons/venv
.cache/dot-icons/venv/bin/pip install -r tools/generate-dot-icons/requirements.txt
.cache/dot-icons/venv/bin/python tools/generate-dot-icons/generate-dot-icons.py
.cache/dot-icons/venv/bin/python tools/generate-dot-icons/generate-dot-icons.py --check
```

CairoSVG needs the Cairo system library. On Homebrew macOS, set
`DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib` if Cairo is not discovered.

The manifest selects SVG layers, optional child elements and SVG transforms.
Optional `fill` and `stroke_width` override SVG root styling before sampling.
Clear fills the circular sun disk while retaining its rays; Snow uses the
original Lucide snowflake stroke.
Rasterization uses 64 samples per cell, area averaging of alpha coverage and
a configurable threshold. Sources must use transparent backgrounds. Composition
requires matching viewBoxes; the first source supplies root SVG styling.
Output is deterministic and unchanged files are not rewritten. Empty and
duplicate matrices are rejected. Custom manifests, source directories and
output headers can be selected with `--manifest`, `--source` and `--output`.
The manifest `size` field sets the grid side (11 for weather); `--size 7`,
`--size 9` or `--size 11` overrides it. Sizes 1 through 16 are supported, including
even sizes. Each generated asset carries its size; no View code changes are needed.
At low resolutions, distinct source icons can collapse to the same matrix; the
generator reports this instead of silently emitting duplicate icons.

## Weather categories

| Category | Lucide source | WWO codes |
| --- | --- | --- |
| Clear | sun | 113 |
| Partly Cloudy | cloud-sun | 116 |
| Cloudy / Fog | cloud | 119, 122, 143, 248, 260 |
| Rain | cloud-rain | 176, 263, 266, 293, 296, 299, 302, 305, 308, 353, 356, 359 |
| Thunderstorm | zap | 200, 386, 389, 392, 395 |
| Snow / Sleet / Icy | snowflake | 179, 182, 185, 227, 230, 281, 284, 311, 314, 317, 320, 323, 326, 329, 332, 335, 338, 350, 362, 365, 368, 371, 374, 377 |

Merging applies only to icons. Rain intensity, fog, overcast, sleet and freezing
conditions retain their original detailed text labels. Thunderstorm and Snow use
standalone lightning and snowflake shapes. Unknown codes reserve an empty icon
slot and show `--`. All 48 WWO codes map to these six icon categories.

## Representation and rendering

`platform::ui::DotMatrix` stores an explicit `size` and up to sixteen 16-bit rows,
with bit `size - 1` at the left. Only `size` rows and their low `size` bits participate.
The weather assets use 11x11 grids (121 cells); 7x7 and 9x9 use the same type.
`DotMatrixView` is a reusable, stateless View with matrix, color and dot diameter
Props. Active cells are circles; inactive cells are transparent. Center spacing
equals the diameter, so adjacent dots touch. Bounds are centered without automatic
scaling. Invalid diameters below 2 or insufficient bounds are left untouched.

The default diameter is 12px (132x132). Lock uses 5px (55x55), exactly 2.4 times
smaller. Integer pixel-center circle scanlines avoid platform-dependent rounded
rectangle corner conventions. Rendering has no weather or service dependency.

## Preview and verification

```sh
tools/preview/preview capture-view DotMatrixView --example weather
tools/preview/preview capture app://shell/lock --weather sample --calendar sample --time 13:40
make test-dot-matrix
```

The gallery renders all six generated assets through the production View, with
12px dots on a 480x800 portrait frame. Lock uses the same matrices at 5px.
Tests check actual framebuffer pixels for 7/9/11 grids, even and odd diameters, both colors,
centering, bounds, empty states, and coverage of all 48 WWO codes.

The earlier unchanged-source 7/9/11/13 comparison remains available with
`--compare .preview/dot-icon-comparison.png`; it does not modify the C++ assets.
