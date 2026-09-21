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
| Reproducible generation and metric guards | `tools/generate-fonts/generate-fonts.py` | `SPECS`, `generate`, `rasterize` |
| On-device font specimen | `src/apps/typography/TypographyPage.cpp` | `readingArticle`, `displayArticle`, `_renderArticle` |
| Clock consumer | `src/apps/shell/pages/LockPage.cpp` | `LockPage::render` |
| Small text consumer | `src/apps/shell/components/StatusBar.cpp` | `StatusBar::render` |
| Registration, glyph rendering and clock stability checks | `tests/FontsTest.cpp` | `main` |

## Slot contract

| ID | `Font` enumerator | Source face | Pixel size | Line height | Recommended use |
| --- | --- | --- | --- | --- | --- |
| 0 | `RobotoS` | Roboto Regular | 22 | 27 | Status bar, captions, metadata |
| 1 | `RobotoM` | Roboto Regular | 28 | 33 | Body text, standard labels |
| 2 | `RobotoL` | Roboto Medium | 32 | 38 | Section headings, prominent labels |
| 3 | `RobotoXL` | Roboto Medium | 40 | 48 | Page headings, subheadings |
| 4 | `Roboto2XL` | Roboto Medium | 46 | 55 | Primary titles |
| 5 | `NDot2XL` | Ndot 57 Regular | 46 | 45 | Short display headings |
| 6 | `NDot4XL` | Ndot 57 Regular | 80 | 76 | Large display words or numbers |
| 7 | `NDot120` | Ndot 57 Regular | 120 | 114 | Lock-screen clock, very short uppercase display text |

Use `platform::fonts::Font` in application code; convert with `fontId` at SDK boundaries, e.g. `.font = fontId(Font::RobotoM)`. Do not introduce raw slot numbers or replace slots per page. SDK default `TextStyle` and unconfigured theme tokens still select slot 0; body/title consumers must explicitly choose their intended slot. Registration alone does not set a theme.

## Coverage

- Slots 0–6: printable ASCII U+0020–U+007E. No CJK/extended Unicode fallback. SDK normalizes ellipsis to three dots; extend other missing coverage deliberately.
- Slot 7: `0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ` only. Digits/letters have centered 72 px advances; colon 24 px; every `HH:MM` is 312 px wide. U+003B–U+0040 are blank metric-table padding, not supported glyphs.

## Generation and checks

Run `python tools/generate-fonts/generate-fonts.py` with Pillow 12.3.0, fonttools 4.65.0 and `clang-format` on PATH. Inputs/provenance: `fonts/README.md`. Commit generated headers; never hand-edit. Only `Fonts.cpp` includes bitmap data; firmware builds need no Python font libraries.

The generator checks source coverage/integer bounds and handles negative bearings. Preserve baseline metrics and glyph coverage. Assets are 1bpp; this does not limit panel grayscale capability.

Checks: `make test-fonts test-shell test-shell-facade`. Firmware: `.pio-core/penv/bin/python -m platformio run -e papermono` (`make build` also formats unrelated files). Capture both `app://typography/?article=reading` and `article=display`; font density and Ndot dot separation also need physical-panel inspection.
