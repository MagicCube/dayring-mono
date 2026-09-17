# Fonts

## Principles

- Use Roboto for readable UI and continuous text; reserve Ndot for short display accents, large numbers, and clocks. Avoid Ndot for paragraphs or dense controls.
- Default body text to `RobotoM`; use `RobotoS` for secondary information. Build hierarchy with the existing slots and whitespace before adding sizes.
- Measure text with the selected font and available width; use measured wrapping and line height, not nominal pixel size, to allocate bounds.
- Keep slots global and explicit. Preserve glyph coverage and baseline metrics; verify changes in the real-page preview and on the panel.

## Code map

| Find | File | Symbols |
| --- | --- | --- |
| Typed slot IDs and SDK conversion | `src/platform/fonts/Fonts.h` | `Font`, `fontId` |
| Binding all eight slots | `src/platform/fonts/Fonts.cpp` | `registerFonts` |
| Registration before every rendered frame | `src/platform/runtime/Display.cpp` | `renderFrame` |
| Bitmap data and metrics | `src/platform/fonts/generated/*.h` | Generated constants; do not hand-edit |
| Reproducible generation and metric guards | `tools/generate-fonts.py` | `SPECS`, `generate`, `rasterize` |
| On-device font specimen | `src/apps/typography/TypographyPage.cpp` | `readingArticle`, `displayArticle`, `_renderArticle` |
| Clock consumer | `src/apps/shell/pages/LockPage.cpp` | `LockPage::render` |
| Small text consumer | `src/apps/shell/components/StatusBar.cpp` | `StatusBar::render` |
| Registration, glyph rendering and clock stability checks | `tests/FontsTest.cpp` | `main` |

## Slot contract

| ID | `Font` enumerator | Source face | Pixel size | Line height | Recommended use |
| --- | --- | --- | --- | --- | --- |
| 0 | `RobotoS` | Roboto Regular | 23 | 28 | Status bar, captions, metadata |
| 1 | `RobotoM` | Roboto Regular | 28 | 33 | Body text, standard labels |
| 2 | `RobotoL` | Roboto Medium | 32 | 38 | Section headings, prominent labels |
| 3 | `RobotoXL` | Roboto Medium | 40 | 48 | Page headings, subheadings |
| 4 | `Roboto2XL` | Roboto Medium | 46 | 55 | Primary titles |
| 5 | `NDot2XL` | Ndot 57 Regular | 46 | 45 | Short display headings |
| 6 | `NDot4XL` | Ndot 57 Regular | 80 | 76 | Large display words or numbers |
| 7 | `NDot120` | Ndot 57 Regular | 120 | 114 | Lock-screen clock, very short uppercase display text |

Use `platform::fonts::Font` in application code; convert with `fontId` at SDK boundaries, e.g. `.font = fontId(Font::RobotoM)`. Do not introduce raw slot numbers or replace slots per page. SDK default `TextStyle` and unconfigured theme tokens still select slot 0; body/title consumers must explicitly choose their intended slot. Registration alone does not set a theme.

Slots 0–6 include all 95 printable ASCII characters U+0020–U+007E, including punctuation. Slot 7 includes exactly `0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ` (37 rasterized glyphs), with digits and uppercase letters centered within uniform 72px advance cells. The colon uses its natural 24px advance, centered in that narrower cell. Every `HH:MM` measures 312px; changing the time never changes its width. The SDK requires a contiguous metric table: U+003B–U+0040 are reserved blank cells, not supported characters. Do not use that slot for general text.

No CJK or extended Unicode font fallback is installed. SDK ellipsis normalization maps U+2026 to three ASCII dots in slots 0–6; other non-ASCII punctuation is not covered. Extend coverage deliberately rather than silently baking missing-glyph boxes. Source font coverage and SDK integer field bounds are checked by the generator.

## Regeneration and validation

Run `python tools/generate-fonts.py` from a Python environment with Pillow 12.3.0 and fonttools 4.65.0; `clang-format` must be on PATH. Inputs live in `fonts/`; source provenance is in `fonts/README.md`. Generated headers are committed assets; firmware builds do not require Python font libraries. Large generated data tables are exempt from handwritten file-size guidelines.

Assets use 1bpp rasterization for the monochrome framebuffer. Each bitmap is included only by `Fonts.cpp`. Preserve baseline metrics; nominal pixel size is not line height. The generator renders using translated glyph bounds to avoid clipping negative bearings.

Run `make test-fonts test-shell test-shell-facade`. Build firmware with `.pio-core/penv/bin/python -m platformio run -e papermono`; `make build` also formats unrelated source files. Font size/density and Ndot dot separation still require physical-panel inspection.

Typography preview: launch `app://typography/` from Home. Two article spreads cover all eight slots, using `layoutLinear` and `measureWrappedText`, 24px side padding and 16px block gaps. Left swipe/right-third tap advances; right swipe/left-third tap returns. Center taps and vertical swipes do not page; endpoints do not wrap. Rendered bounds gate input, and page changes invalidate through `TypographyApplication::onInput`. Global bottom-edge home gestures remain container-owned.

For host pixel verification, run `./tools/preview/preview capture 'app://typography/?article=reading'` and repeat with `article=display`. The CLI registers the same committed font bitmaps and renders the same page methods as firmware; no desktop font substitution is used.
