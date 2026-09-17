# UI Preview CLI

Run the real firmware application/page rendering on macOS and capture a PNG for an edit, capture, inspect, and revise loop. The CLI uses the same application registry, page routes, lifecycle, fonts, status bar, and `DisplayTarget` as firmware. It does not redraw the UI in HTML or convert SVG.

## Quick start

```sh
make preview "app://shell/"
./tools/preview/preview capture 'app://shell/' --battery 85
./tools/preview/preview capture 'app://shell/lock' --time 09:15
make preview "app://typography/?article=display"
```

The launcher uses `.pio-core/penv/bin/python`; no additional Python or image packages are needed once the project environment exists. That interpreter may be a symlink to a local Python installation, which must remain available. Override it with `PYTHON=/path/to/python3`. Native compilation requires Xcode Command Line Tools (`clang++`); override with `HOST_CXX=/path/to/host-c++`. The ESP32 cross compiler cannot produce a macOS runner.

Default screenshots are under the project-root `.preview/`, regardless of the caller's working directory:

```text
.preview/shell.png
.preview/shell--lock.png
.preview/typography--article-display.png
```

Filenames identify the application, non-root page, and readable query. Root pages have no page suffix; nested path segments use `--`. There are no UUIDs or hashes in screenshot filenames. Capturing the same URL replaces its PNG, including when device state changes. Query variants such as `article=reading` and `article=display` remain separate. Sanitized query/path names can collide; use `--output` when separate captures are needed.

`.preview/` contains only PNGs. Build executables, dependency manifests, reusable object files, Python bytecode, and temporary work live in `.cache/preview/`. Both directories are Git-ignored. Removing `.preview/` clears screenshots without losing build caches; removing `.cache/preview/` forces a fresh build.

`make preview` without a URL captures `app://shell/`. Pass exactly one quoted URL when selecting another page. Use the direct CLI for options or machine-readable output.

Use `--output path.png` to override the filename. Explicit relative output paths are relative to the caller's current working directory. Successful writes replace the destination atomically; failures preserve existing images.

## Progressive help

```sh
./tools/preview/preview --help
./tools/preview/preview capture --help
./tools/preview/preview routes --json
./tools/preview/preview help typography
```

Top-level help introduces commands. Command help describes options. Application help discovers routes and parameter documentation from C++ registrations. `routes` initializes registered applications without entering their pages; application constructors and `onCreate` must support host initialization. Add route documentation alongside `router().registerPage(...)`, not in the CLI.

Capture defaults are a complete 480 × 800 frame, the current local time, battery `75%`, and charging off. The CLI samples the host clock once immediately before rendering, after any build. Use `--battery 85` for 85%, and `--time HH:MM` for repeatable screenshots; `--battery-charging` enables the charging indicator. JSON results record the exact sampled time. Named isolated View examples keep their explicit Props. Fullscreen pages naturally hide the status bar. Quote URLs so shell metacharacters such as `&` and `#` remain part of the URL.

`app://home` uses the production Home alias. `app://shell/lock` uses the production lock presentation. Unknown applications and paths fail rather than falling back to an unrelated screenshot. The host opens Home before the target to establish the lifecycle required by lock presentation.

Typography accepts `article=reading` (default) or `article=display`; invalid values fail on both host and firmware. Query keys and values are percent-decoded bytes, `+` remains a literal plus, and the first repeated key wins. Unknown keys are ignored by current pages. Malformed percent escapes and decoded NULs fail. Page paths remain exact and case-sensitive, without percent-decoding; query and fragment text is preserved for `PageController::onEnter`.

## Machine-readable results

Append `--json` to `capture`, `routes`, or application `help`. Capture returns one JSON object with `ok`, absolute `output`, `requested_url`, `resolved_url`, `width`, `height`, `state`, and `cache_hit`. Failures return `ok: false` and an `error` containing stable `code` and explanatory `message`; diagnostics also go to stderr. Launcher errors before Python starts are stderr-only.

| Exit | Meaning |
| --- | --- |
| 0 | Success |
| 2 | Invalid arguments, URL, query encoding, or page parameters |
| 3 | Unknown application or page |
| 4 | Missing Python/compiler, build failure, or build-cache I/O failure |
| 5 | Navigation, renderer, timeout, or native-protocol failure |
| 6 | PNG output failure |
| 130 | Interrupted |

Native builds cache each C++ translation unit separately and compile independent files in parallel (up to eight jobs). Compiler-generated dependency manifests include SDK/system headers and generated fonts. Warm captures hash these known dependencies directly, avoiding a compiler preprocessing pass. Changing one page only recompiles affected translation units and relinks; unchanged SDK and font objects are reused. Header inventory changes invalidate configuration to detect newly added headers shadowing existing includes. Content hashing detects edits even when modification times are preserved.

This trades disk space for iteration speed: object files and linked executables from prior builds remain reusable under `.cache/preview/build/`. `--rebuild` forces recompilation. Compiler identity, flags, toolchain environment, and build implementation are included in cache identity. The host reads `DAYRING_HOME_URL` from the PaperMono environment in `platformio.ini`. Concurrent invocations use isolated temporary files and atomic publication. Each render has a 30-second limit; each compiler invocation has a 180-second limit.

The native runner emits a versioned JSON line followed by raw gray8 portrait pixels. Python validates the frame and encodes lossless PNG using only `struct` and `zlib`. This protocol is internal; use the launcher for normal verification.

## Verification and limits

```sh
make test-preview
make test
.pio-core/penv/bin/python -m platformio run -e papermono
```

Tests cover query handling, shared route discovery, exact navigation, framebuffer orientation, all current routes, both Typography articles, Home/lock behavior, state overrides, PNG pixel parity, deterministic output, JSON/errors, dependency-content cache invalidation, failed builds, cold concurrent builds, and concurrent captures.

After changing UI code, run `make preview "app://application/path"` and visually inspect the resulting PNG. Show the screenshot to the user with Markdown image syntax and its absolute path; do not stop at a tool-only inspection or a file link. Pixel tests do not establish that a layout is readable or attractive. Fonts and layout match firmware. Preview output preserves four tones (0, 85, 170, 255) in a gray8 mirror of the native drawing target. Firmware still uses its existing 1-bit upload path; the preview does not enable hardware grayscale refresh. It does not simulate ghosting, refresh waveforms, touch interaction, or asynchronous application loading. Future pages requiring services must receive appropriate host adapters; do not replace their render methods with preview-only UI.

## Isolated View previews

```sh
./tools/preview/preview views
./tools/preview/preview view-help TypographyPage
./tools/preview/preview capture-view TypographyPage --example display
./tools/preview/preview capture-view StatusBar --example charging --json
```

These commands build a separate runner containing only Views, FreeInk, fonts, typed C++ examples, and shared framebuffer/export utilities. No Application, controller, Router, HAL, Arduino stub, or device initialization is linked. Route commands still exercise real controllers and host adapters. Both paths call the same production View rendering code.

Examples are defined in `native/ViewMain.cpp` with explicit Props and bounds. `views` lists every example; `view-help` filters by exact View name. `capture-view` requires `--example`; unknown Views or examples return exit 3 and preserve existing output. No JSON-to-Props reflection is used. Add typed examples when adding a View or a visual state.

All exports use the same 480 x 800 portrait frame. Page examples use the complete frame with no implicit status bar; StatusBar examples draw in the top 36 pixels and leave the rest white. Default filenames are `.preview/view-<name>--<example>.png`; `--output`, `--json`, and `--rebuild` behave like route capture. JSON discovery exposes `examples` with `view`, `name`, and `bounds`; capture results expose `view`, `example`, output geometry, and cache status.

Both targets use independent build identities and retain per-translation-unit caching. Measure warm end-to-end capture separately from cold compilation; the development target is below one second for an unchanged capture. See [UI architecture](../../docs/ui.md) for ownership, rendering, and portability rules.

Use `capture app://shell/lock --power-press` to preview the transient unlock hint through the real Shell input path. The optional flag sends one power press after the initial page render; it is available for any route and follows normal firmware input behavior.
