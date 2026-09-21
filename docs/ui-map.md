# UI Code Map

Design rules and rationale: [UI architecture](ui.md).

| Find | File | Symbols |
| --- | --- | --- |
| Drawing adapter and coordinate types | `src/platform/ui/View.h` | `View`, `Canvas`, `Rect` |
| Page ownership, lifecycle, fullscreen invalidation | `src/platform/ui/PageController.*` | `PageController`, `setFullscreen` |
| Presentation-only pages with default Props | `src/platform/ui/StaticPageController.h` | `StaticPageController<PageType>` |
| Status layout, full-frame composition, touch filtering and home gesture | `src/platform/ui/ApplicationContainer.cpp` | `render`, `needsRender`, `onInput` |
| Status clock sampling and battery composition | `src/apps/shell/components/StatusBarController.cpp` | `update`, `render` |
| Minute change timing | `src/platform/time/services/TimeService.cpp` | `update`, `displayTime`, `minuteRevision` |
| Framebuffer adapter and refresh gate | `src/platform/runtime/Display.cpp` | `displayDevice`, `renderFrame` |
| Article typography preview and tap/swipe paging | `src/apps/typography/TypographyPage.*` | `render`, `_renderArticle`; `TypographyPageController` handles input |
| Font IDs, assets and registration | `docs/fonts.md` | Read the font map before font changes |
| SDK primitives and layout | `freeink-sdk/docs/freeink-ui.md` | Load only for FreeInk drawing changes |

## Composition and input

- Immediate-mode full-frame drawing; apps delegate update/render/input. Input consumption does not invalidate visual changes.
- Container owns StatusBarController/StatusBar. Page fullscreen state controls visibility; otherwise reserve 36 px. Force-render the app during composition.
- Bounds/input use absolute 480 × 800 portrait coordinates; do not subtract status height. Touch guards compare last-rendered controller, status visibility and content bounds, not every same-page layout change.
- Container captures completed bottom-edge upward swipes before app input via FreeInk `edgeSwipe`. The invisible 72 px band also applies fullscreen and does not reduce content bounds. Stale-layout guards apply; matching gestures are consumed even if Shell cannot navigate. Policy: [Shell](shell.md).
- Display submits only when HAL is ready; input/update continue during asynchronous refresh.

## Battery and previews

`src/apps/shell/views/BatteryIndicatorView.*` renders explicit percent/charging/theme Props. `BatteryIndicatorController` samples HAL; StatusBarController passes its snapshot into StatusBar. Theme changes invalidate through controller update. Charging decoration must not increase reported fill.

Use [Preview CLI](../tools/preview/README.md) for route captures and isolated View examples. Route previews execute production composition; isolated builds exclude controllers/runtime/hardware. Both use committed fonts and export portrait PNGs.

- Typography routes: `app://typography/?article=reading` or `article=display`.
- Battery matrix: `preview capture-view BatteryIndicatorView --example themes`.
- UI changes require final real-page PNG capture, visual inspection and an absolute-path image in the response (see root `AGENTS.md`).

Checks: `make test-shell test-navigation test-shell-facade test-preview`.

## QR codes and pairing

- `src/platform/ui/QRCode.h/.cpp`: value-owned encoded matrix, versions 1–10, medium error correction with automatic strengthening. `encode()` rejects invalid/oversized input and clears previous output on failure. Encode when content changes, not on every frame.
- `src/platform/ui/QRCodeView.h`: reusable stateless black-on-white renderer; centers at an integer module scale and retains a four-module white quiet zone. Bounds too small to fit are left untouched; callers should also show the destination as text.
- `third_party/qrcodegen/`: unmodified MIT-licensed Nayuki C library v1.8.0; `QRCodeEncoder.cpp` builds the same implementation for native previews and firmware without allocations or exceptions. FreeInk has no QR encoder; this application-level component uses its drawing primitives.
- `src/apps/shell/pages/PairingPage.*`: fullscreen black setup page with Ndot heading, QR and plain-text URL.
- `PairingPageController.*`: owns encoded content and observes pairing state; startup gating/navigation is owned by Shell.

Capture: `tools/preview/preview capture app://shell/pairing`. QR decode validation of the actual PNG returns `https://dayring.ai`.
