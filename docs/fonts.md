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
| Startup loading and binding all eight slots | `src/platform/fonts/Fonts.cpp` | `loadFonts`, `registerFonts` |
| Validated binary loading and owned PSRAM | `src/platform/fonts/FontAsset.h/.cpp` | `FontAsset::load` |
| Shared FAT mount without formatting | `src/platform/storage/FatFilesystem.h` | `mountFatFilesystem` |
| In-memory binding before every rendered frame | `src/platform/runtime/Display.cpp` | `renderFrame` |
| Bitmap data and metrics | `data/fonts/*.bin` | Generated FATFS assets; do not hand-edit |
| Reproducible generation and metric guards | `tools/generate-fonts/generate-fonts.py` | `SPECS`, `generate`, `rasterize` |
| On-device font specimen | `src/apps/typography/TypographyPage.cpp` | `readingArticle`, `displayArticle`, `_renderArticle` |
| Clock consumer | `src/apps/shell/pages/LockPage.cpp` | `LockPage::render` |
| Small text consumer | `src/apps/shell/components/StatusBar.cpp` | `StatusBar::render` |
| Registration, glyph rendering and clock stability checks | `tests/FontsTest.cpp` | `main` |

## Slot contract

| ID | `Font` enumerator | Source face | Pixel size | Line height | Recommended use |
| --- | --- | --- | --- | --- | --- |
| 0 | `RobotoS` | Roboto Regular + Source Han Sans SC Medium | 23 Latin / 22 CJK | 28 | Status bar, captions, metadata |
| 1 | `RobotoM` | Roboto Regular + Source Han Sans SC Medium | 28 Latin / 26 CJK | 33 | Body text, standard labels |
| 2 | `RobotoL` | Roboto Medium + Source Han Sans SC Medium | 32 Latin / 30 CJK | 48 | Section headings, prominent labels |
| 3 | `RobotoXL` | Roboto Medium + Source Han Sans SC Medium | 40 Latin / 38 CJK | 48 | Page headings, subheadings |
| 4 | `Roboto2XL` | Roboto Medium + Source Han Sans SC Medium | 46 Latin / 43 CJK | 55 | Primary titles |
| 5 | `NDot2XL` | Ndot 57 Regular | 46 | 45 | Short display headings |
| 6 | `NDot4XL` | Ndot 57 Regular | 80 | 76 | Large display words or numbers |
| 7 | `NDot120` | Ndot 57 Regular | 120 | 114 | Lock-screen clock, very short uppercase display text |

Use `platform::fonts::Font` in application code; convert with `fontId` at SDK boundaries, e.g. `.font = fontId(Font::RobotoM)`. Do not introduce raw slot numbers or replace slots per page. SDK default `TextStyle` and unconfigured theme tokens still select slot 0; body/title consumers must explicitly choose their intended slot. Registration alone does not set a theme.

## Coverage

- Slots 0–4 (`SansS` through `Sans2XL`): ASCII and halfwidth punctuation use Roboto at their existing pixel sizes and weights. All added characters use Source Han Sans SC Medium at the CJK pixel sizes above. Each slot includes all GB2312 level-1 (3,755) and level-2 (3,008) ideographs, GB2312 rows 1 and 3, and additional Chinese brackets, em dash, middle dot and ideographic zero (7,053 supported characters total).
- Slots 5–6: printable ASCII U+0020–U+007E only.
- Existing line heights and baselines are preserved: S 28/22, M 33/26, L 48/38, XL 48/38 and 2XL 55/43 px (line height/baseline). Chinese and Latin glyphs share the baseline.
- The renderer uses the actual ellipsis glyph when present; ASCII-only slots retain the three-dot fallback. Empty unsupported table entries retain missing-glyph fallback.
- Slot 7: `0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ` only. Digits/letters have centered 72 px advances; colon 24 px; every `HH:MM` is 312 px wide. U+003B–U+0040 are blank metric-table padding, not supported glyphs.

## Generation and checks

Run `python3 tools/generate-fonts/generate-fonts.py` with Pillow 12.3.0 and fonttools 4.65.0. Inputs/provenance: `fonts/README.md`. Commit FATFS assets under `data/fonts/`; never hand-edit generated files. The generator preserves unchanged contents and timestamps and removes its obsolete C++ font headers. No application bitmap font arrays are compiled into firmware; firmware builds need no Python font libraries.

Firmware `setup()` calls `loadFonts()` after HAL initialization and before starting services or opening a page. It mounts the shared `storage` FAT partition without formatting and reads all eight `/fonts/*.bin` files through the `/ffat` VFS mount. Each asset's glyph records, sparse keys and inflated bitmap stream live in explicitly allocated PSRAM for the device session. Compressed input is temporary and released after each load. Loading validates sizes, versions, sorted indices, glyph bounds, Rice streams and zlib checksums before publishing all eight slots atomically. Missing, corrupt or allocation-failed assets leave no partial font set; startup retains the boot image and services HAL/USB updates without starting the UI, so assets can be repaired. Diagnostics identify mount failure or the failed asset path.

`registerFonts()` only binds the resident descriptors to a new target and performs no file I/O or decompression. Repeated successful `loadFonts()` calls are also no-ops. Native route and isolated View previews load the identical binaries from `data/fonts/` at process startup; the CLI launches from the repository root. Calendar persistence uses the same mount helper and cannot format a failed mount.

The filesystem font files are `sans-s.bin`, `sans-m.bin`, `sans-l.bin`, `sans-xl.bin`, `sans-2xl.bin`, `ndot-2xl.bin`, `ndot-4xl.bin`, and `ndot-120.bin`. The Sans fonts contain printable ASCII U+0020–U+007E, including common English punctuation; all five also contain the Chinese coverage above; Ndot120 contains `0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ`. Files use a versioned little-endian header, glyph metrics, and zlib-compressed 1bpp bitmap data. `make fs:upload` packages the existing `data/` files without regenerating fonts and uploads the FATFS image in one esptool connection using uncompressed 256 KiB chunks at a default 460800 baud. The command hides routine build/esptool output and prints `Uploading N/total... done` after each verified chunk; failures show diagnostic output. Esptool verifies each chunk before proceeding; there are no resets or port rediscovery between chunks. This avoids large compressed packets expanding into long flash writes in sparse image regions. A failed transfer leaves an incomplete image and must be rerun. `make test-fs-sync` checks transfer construction without contacting hardware. Regenerate fonts explicitly with `python3 tools/generate-fonts/generate-fonts.py` when font sources or settings change. Generation requires Pillow and fonttools; filesystem synchronization does not.

The generator checks source coverage/integer bounds and handles negative bearings. Preserve baseline metrics and glyph coverage. Assets are 1bpp; this does not limit panel grayscale capability.

Checks: `make test-font-assets test-fonts test-shell test-shell-facade`. Firmware: `.pio-core/penv/bin/python -m platformio run -e papermono` (`make build` also formats unrelated files). Capture both `app://typography/?article=reading` and `article=display`; font density and Ndot dot separation also need physical-panel inspection.

### Font representation and binary format

Sans fonts use a sorted sparse Unicode index and lossless Rice-coded alternating zero/one pixel runs. Each glyph starts with a zero run (possibly empty). A run stores a unary quotient (ones terminated by zero), followed by a fixed-width remainder; bits are MSB-first and each glyph starts at a byte boundary. S/M/L/XL use two remainder bits; 2XL uses three. The renderer streams decoded pixels without allocating a decompressed font. Ndot fonts keep raw bitmaps and contiguous tables. `FontGlyph` uses a packed 9-byte layout with 32-bit offsets.

The `DRFONT1\0` magic is followed by a version byte. Version 1 retains 7-byte `<HBBBbb` records for Ndot. Legacy version 2 used contiguous 9-byte `<IBBBbb` records. Version 3 (all five Sans assets) stores the same 24-byte header, a 16-bit glyph count, a one-byte Rice parameter, sorted 16-bit Unicode keys, 9-byte `<IBBBbb` glyph records, then a zlib-compressed Rice bitmap stream. The header's uncompressed bitmap length and glyph offsets refer to the Rice stream, not raw pixels. Consumers must branch on the version. `FontAsset` reads versions 1–3, widens legacy offsets, and keeps descriptor pointers valid through owned storage.

SDK implementation: `FreeInkUIFontLookup.h` provides sparse lookup and `FontBitReader`; both `DisplayTarget` and `BitmapBookFont` use it. Font assets occupy the FAT partition, while their runtime representation lives in PSRAM; the existing partition layout is unchanged.

Inspect the generator, binary metadata, tests and previews for font coverage and rendering.

GB2312 level counts are also documented in [Tsinghua University Press teaching material](https://www.tup.com.cn/upload/books/yz/105768-01.pdf). Punctuation follows the [Unicode CJK Symbols and Punctuation names list](https://www.unicode.org/charts/nameslist/n_3000.html).

### Runtime capacity (1bpp, full coverage)

Resident figures include packed glyph records, sparse Unicode keys and bitmap bytes, excluding the small font descriptors. Filesystem figures include metadata and the zlib-compressed bitmap. All eight resident assets occupy about 3.17 MiB of PSRAM; startup also needs one compressed input buffer at a time.

| Slot | Bitmap bytes | Resident PSRAM bytes | Filesystem asset bytes |
| --- | ---: | ---: | ---: |
| S | 346,233 | 423,816 | 374,091 |
| M | 427,285 | 504,868 | 452,362 |
| L | 510,370 | 587,953 | 526,118 |
| XL | 726,255 | 803,838 | 701,673 |
| 2XL | 854,206 | 931,789 | 810,823 |
| NDot2XL | 9,297 | 10,152 | 1,978 |
| NDot4XL | 27,938 | 28,793 | 1,759 |
| NDot120 | 27,144 | 27,531 | 1,253 |

For a device’s first upload, successfully complete `make fs:upload` before `make upload`; firmware depends on these assets at startup. Keep all eight `data/fonts/*.bin` files in Git.
