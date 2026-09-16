# UI Code Map

| Find | File | Symbols |
| --- | --- | --- |
| Drawing adapter and coordinate types | `src/platform/ui/Component.h` | `Component`, `Canvas`, `Rect` |
| Page ownership, lifecycle, fullscreen invalidation | `src/platform/ui/Page.*` | `Page`, `setFullscreen` |
| Status layout, full-frame composition, touch filtering and home gesture | `src/platform/ui/ApplicationContainer.cpp` | `render`, `needsRender`, `onInput` |
| Status clock/battery sampling | `src/apps/shell/components/StatusBar.cpp` | `update`, `render` |
| Minute change timing | `src/platform/ui/MinuteClock.h` | `MinuteClock::update` |
| Framebuffer adapter and refresh gate | `src/platform/runtime/Display.cpp` | `displayDevice`, `renderFrame` |
| Article typography preview and tap/swipe paging | `src/apps/typography/TypographyPage.*` | `render`, `_renderArticle`, `onInput` |
| Font IDs, assets and registration | `docs/platform/fonts.md` | Read the font map before font changes |
| SDK primitives and layout | `freeink-sdk/docs/freeink-ui.md` | Load only for FreeInk drawing changes |

Apps own pages and explicitly delegate update/render/input. Consuming input does not invalidate; request rendering for visual changes. This is immediate-mode drawing, not an automatic widget tree.

Container owns the global Shell StatusBar; visibility derives from page fullscreen state. It reserves 36 px and force-renders the app during full composition. Bounds and input remain absolute logical coordinates (portrait 480 x 800); do not subtract status height.

Touch press/release checks last-rendered page identity, status visibility and content bounds; this does not detect every same-page layout change. Display draws only when HAL is ready, then submits full asynchronous refresh; input/update remain active while busy.

Checks: `make test-shell`; `make test-navigation` for page lifecycle; `make test-shell-facade` for lock integration.

Container captures completed bottom-edge upward swipes before application input using FreeInk `edgeSwipe`. The invisible bottom band is 72 logical pixels and does not reduce content bounds; it also applies to fullscreen pages. Swipe endpoints are absolute logical coordinates. The same rendered-page/status guard rejects stale-layout swipes. The injected home handler consumes matching gestures even when Shell ignores or cannot execute navigation; other swipes reach the app.
