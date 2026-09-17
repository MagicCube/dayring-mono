# UI Code Map

Design rules and rationale: [UI architecture](ui.md).

| Find | File | Symbols |
| --- | --- | --- |
| Drawing adapter and coordinate types | `src/platform/ui/View.h` | `View`, `Canvas`, `Rect` |
| Page ownership, lifecycle, fullscreen invalidation | `src/platform/ui/PageController.*` | `PageController`, `setFullscreen` |
| Presentation-only pages with default Props | `src/platform/ui/StaticPageController.h` | `StaticPageController<PageType>` |
| Status layout, full-frame composition, touch filtering and home gesture | `src/platform/ui/ApplicationContainer.cpp` | `render`, `needsRender`, `onInput` |
| Status clock sampling and battery composition | `src/apps/shell/components/StatusBarController.cpp` | `update`, `render` |
| Minute change timing | `src/platform/runtime/MinuteClock.h` | `MinuteClock::update` |
| Framebuffer adapter and refresh gate | `src/platform/runtime/Display.cpp` | `displayDevice`, `renderFrame` |
| Article typography preview and tap/swipe paging | `src/apps/typography/TypographyPage.*` | `render`, `_renderArticle`; `TypographyPageController` handles input |
| Font IDs, assets and registration | `docs/fonts.md` | Read the font map before font changes |
| SDK primitives and layout | `freeink-sdk/docs/freeink-ui.md` | Load only for FreeInk drawing changes |

Apps own page controllers and explicitly delegate update/render/input. Consuming input does not invalidate; request rendering for visual changes. This is immediate-mode drawing, not an automatic widget tree.

Container owns the global StatusBarController and its StatusBar; visibility derives from page-controller fullscreen state. It reserves 36 px and force-renders the app during full composition. Bounds and input remain absolute logical coordinates (portrait 480 x 800); do not subtract status height.

Touch press/release checks last-rendered page-controller identity, status visibility and content bounds; this does not detect every same-page layout change. Display draws only when HAL is ready, then submits full asynchronous refresh; input/update remain active while busy.

Checks: `make test-shell`; `make test-navigation` for page lifecycle; `make test-shell-facade` for lock integration.

Container captures completed bottom-edge upward swipes before application input using FreeInk `edgeSwipe`. The invisible bottom band is 72 logical pixels and does not reduce content bounds; it also applies to fullscreen pages. Swipe endpoints are absolute logical coordinates. The same rendered-page/status guard rejects stale-layout swipes. The injected home handler consumes matching gestures even when Shell ignores or cannot execute navigation; other swipes reach the app.

## PNG Verification

`make preview "app://application/path"` runs production composition and rendering on macOS. Host hardware adapters in `tools/preview/native/` supply fixed clock/battery state and the same 800 × 480 native framebuffer. The runner exports the rotated 480 × 800 portrait frame; Python writes PNG under `.preview/`, while build and dependency/object caches live in `.cache/preview/`. Screenshot names omit the root page and hashes; repeated captures overwrite the same PNG. No SVG or browser rendering is involved. See [Preview CLI](../tools/preview/README.md) for progressive help and tests.

Typography supports `app://typography/?article=reading` and `app://typography/?article=display` through shared page validation and entry logic. Swipe/tap paging remains available on-device.

Pure page Views live beside their dedicated `PageController` types when behavior is needed; presentation-only pages use `StaticPageController<PageType>` directly in the Application. `StatusBar` owns drawing and `StatusBarController` samples hardware. `Page<Props>` is a stateless `View<Props>`; lifecycle and fullscreen state belong to `PageController`. Router registers controllers; navigation and the manager expose `currentPageController()`.

Isolated screenshots use `preview views`, `view-help <name>`, and `capture-view <name> --example <name>`. The separate View build excludes runtime and hardware sources.

Battery rendering lives in `src/apps/shell/views/BatteryIndicatorView.*`; its explicit Props select percent, charging, and `platform::ui::Theme`. `BatteryIndicatorController` owns HAL sampling (charging every second, percentage every three minutes). StatusBarController owns the battery controller and passes its snapshot to StatusBar, which composes the pure battery View. Both controllers expose `setTheme`; theme changes invalidate through `update`. The white charging bolt has a black separation edge and never increases the reported fill. `Color::LightGray` uses the target's grayscale mapping (the current BW preview dithers it).

`preview capture-view BatteryIndicatorView --example themes` shows 0, 1, 10, 50, 85, and 100 percent, each idle and charging, in light and dark themes.
