# Swipe-up-to-home

## Context

Capture a bottom-edge upward swipe in `ApplicationContainer` and return through the configured Shell home destination. The current adapter delivers only power presses and completed taps; FreeInk already recognizes completed swipes but Dayring does not forward them.

## Approach

### 1. Forward completed swipes — `src/platform/hal/Hardware.h`, `src/platform/hal/Hardware.cpp`, `src/platform/runtime/InputEvent.h`, `src/platform/runtime/Input.cpp` (modify)

Add `hal::touchSwiped(float& startX, float& startY, float& endX, float& endY)` wrapping `InputManager::wasSwipe()`. Add `InputEvent::Type::Swipe`, preserving existing aggregate initializers by appending defaulted `startX`/`startY`; `x`/`y` represent the endpoint. Convert both endpoints with `touchToLogical(displayDevice(), ...)`. Dispatch power first, then a completed swipe, then the existing tap path, returning after each dispatched event. Keep the current synchronous polling model; no raw press/move stream or async queue is required.

The SDK accepts a single-contact release with at least 60 native pixels of displacement on either axis and duration at most 700 ms. It excludes suppressed and multi-contact sequences. Reuse this qualification rather than classifying a tap release as a swipe.

### 2. Capture the bottom gesture — `src/platform/ui/ApplicationContainer.h`, `src/platform/ui/ApplicationContainer.cpp` (modify)

Store the full rendered container bounds, independently of status-bar/content bounds. Before forwarding `Swipe` to the application, require a valid rendered layout, a start point inside these bounds, and `freeink::ui::edgeSwipe(ScreenEdge::Bottom, ...)` with coordinates relative to the container. Use an initial 72 logical pixel bottom band, passed as an explicit height fraction; FreeInk's default 14% would occupy 112 pixels on this display. Clamp the band for smaller bounds. Upward movement must dominate horizontal movement; exact diagonals do not match. The endpoint may leave the start band.

Use an invisible input region over existing content: do not reserve bottom layout space, draw a bar, or suppress ordinary bottom taps. Enable it on fullscreen pages too. Apply the existing rendered-page/status visibility guard to swipe events; before the first render or while those identities are stale, discard the event. Nonmatching swipes on a valid layout continue to the application. This guard does not prove that a gesture started on the current page; strict cross-navigation contact cancellation would require a separate input-lifecycle extension.

### 3. Route the action through Shell — `src/platform/runtime/Shell.h`, `src/platform/runtime/Shell.cpp`, `src/platform/ui/ApplicationContainer.h`, `src/platform/ui/ApplicationContainer.cpp` (modify)

Give the container an optional `std::function<void()>` home handler at construction, so existing standalone container callers remain valid. Shell supplies `[this] { _handleHomeGesture(); }`. The handler ignores locked state and `isHome()`, which compares the active application and full navigation location with the configured home URL, and otherwise calls `goHome()`. A matching gesture invokes the handler once and consumes the event even if navigation fails; never pass a captured system gesture to the application afterward. Shell's existing `goHome()` resolves `DAYRING_HOME_URL` and refuses navigation while locked. Swiping must neither unlock nor replace the interrupted application. Do not hard-code `app://shell/` or bypass the facade through the manager.

Do not extend FreeInk's `gestureBar`: it exposes only left/right actions, and its swipe routing uses `findFirst`, not a start-coordinate hit test. The existing geometric helper is the reusable part needed here.

### 4. Cover input and policy boundaries — `tests/ShellApplicationTest.cpp`, `tests/HomeEntryTest.cpp`, `docs/platform/ui.md`, `docs/platform/shell.md`, `docs/platform/hal.md` (modify)

Add a HAL swipe stub and exercise the real adapter through `Shell::update()`. Test endpoint rotation, capture precedence, bottom-band boundaries, upward/sideways/downward/diagonal cases, ordinary taps, fullscreen layout, pre-render/stale-layout rejection, and one callback per event. Verify return from another app, lock preservation and subsequent restoration, navigation failure consumption, and input while refresh is busy. Extend configured-home coverage using a rendered standalone container wired to the facade, including invalid home configuration. Update the maps to describe the new event and capture ownership.

## Key Files

| File | Change | Notes |
|------|--------|-------|
| `src/platform/hal/Hardware.h`, `src/platform/hal/Hardware.cpp` | modify | Expose completed SDK swipe endpoints |
| `src/platform/runtime/InputEvent.h` | modify | Add endpoint-bearing Swipe event |
| `src/platform/runtime/Input.cpp` | modify | Convert and dispatch both endpoints |
| `src/platform/ui/ApplicationContainer.h`, `src/platform/ui/ApplicationContainer.cpp` | modify | Own bottom capture bounds and home handler |
| `src/platform/runtime/Shell.h`, `src/platform/runtime/Shell.cpp` | modify | Bind the home handler and ignore lock/home |
| `tests/ShellApplicationTest.cpp` | modify | Cover adapter, capture and lock/refresh integration |
| `tests/HomeEntryTest.cpp` | modify | Verify configured and failed home destinations |
| `docs/platform/ui.md`, `docs/platform/shell.md`, `docs/platform/hal.md` | modify | Update input and ownership maps |

## Verification

- Format only modified C++ files with the repository `.clang-format`; run `make test` for existing runtime, navigation, Shell and power regressions.
- Build firmware with `.pio-core/penv/bin/python -m platformio run -e papermono` directly; `make build` also formats unrelated files.
- On hardware, verify portrait mapping, single firing on finger release, no tap after a swipe, and capture during e-paper refresh. Tune the proposed 72 px band using bottom controls and fullscreen content.
- Confirm fast swipes at/above 60 native pixels qualify while short movements and contacts exceeding 700 ms do not. Slow upward drags are intentionally outside this first implementation's SDK-defined swipe behavior.
