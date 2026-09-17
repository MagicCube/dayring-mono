# UI Architecture

FreeInk immediate-mode rendering redraws the full composition on demand. There is no retained widget tree or automatic property observation. Implementation entry points: [UI map](ui-map.md); display constraints: [hardware](hardware.md); font choices: [fonts](fonts.md).

## Views

- `View<Props>` is stateless; `Page<Props>` marks a full page. Implement `render(Canvas&, const Rect&, const Props&) const` with typed, named Props. Prefer value members; extract child Views only for reuse.
- Views own all drawing, layout, text measurement and hit-region generation. They must not depend on controllers, routing, services, clocks, HAL, Arduino or other device code.
- Render from explicit Props/bounds into Canvas and optional layout output. Never mutate business state, navigate or retain borrowed Props. Fixed inputs, fonts and target configuration must produce deterministic output.

## Controllers

- Use `StaticPageController<PageType>` for default-Props presentation-only pages; e.g. `StaticPageController<pages::FirmwareUpdatePage> _firmwareUpdate{true};`. Do not add a dedicated class/alias just to forward rendering or choose fullscreen.
- Add a dedicated PageController for state, input, service access, custom Props, route validation or lifecycle behavior. `ViewController` supplies render/update/input dispatch; `PageController` adds page lifecycle, routing and fullscreen policy.
- Controllers own Views/state, prepare Props per render, call Views and record layout results. No drawing/layout code or synchronized Props copy. Borrow large inputs only while their owner remains alive through rendering.
- Interactive Views accept a concrete `RenderResult&`; their three-argument render uses a temporary result for screenshots. Controllers own result buffers and dispatch resulting actions. Views populate output from fresh local interaction buffers so old focus/press/flash state cannot become a hidden rendering input.
- Clear interaction state on entry/leave and whenever it no longer represents the visible layout. Preserve last-rendered-layout guards, absolute coordinates, status reservation and container home gestures.
- Request rendering for visual changes independently of input consumption. Hardware refresh readiness must not stop state/input updates.

## Ownership and lifecycle

```text
Shell
  owns ApplicationManager → Applications
    each Application owns PageControllers → state, Pages, child controllers/Views
    each Application owns Router/Navigation → borrowed PageController pointers
  owns ApplicationContainer → StatusBarController → StatusBar
  owns ServiceManager → FrontlightService, PowerService, TaskDispatchService, TimeService
```

- Prefer values or `unique_ptr`; avoid ownership cycles. Controllers must outlive router/history use. Router/navigation destructors must not dereference controllers: derived Application members die before base members.
- Foreground departure suspends, not necessarily destroys. Residents/cache entries retain state; lock preserves the interrupted instance. Stop app-owned background work on suspension.
- Eviction/shutdown must release every app-owned resource exactly once, including controllers, Views, state, native handles, subscriptions and tasks. Borrowed platform services and global status bar are outside this boundary.
- Give each resource an explicit RAII owner and cleanup path, including construction failure. Cancel/drain callbacks before destroying their dependencies; `onLeave()` alone, a later launch or process exit is not cleanup.
- Extend `tests/ApplicationManagerTest.cpp` for new resource types/async integrations: retention during suspension, exactly-once eviction cleanup and zero owned instances after shutdown.

## Preview and performance

Route previews run real applications/controllers/composition through host adapters. Isolated View previews compile only rendering sources, FreeInk, fonts, typed examples and framebuffer export; examples supply Props/bounds without recreating UI or implicitly adding a status bar. Keep both builds independently cached and device dependencies out of Views.

Avoid per-frame allocation, large copies and unnecessary rendering. Target warm screenshots under one second; measure CLI latency separately from cold compilation. Commands and options: [Preview CLI](../tools/preview/README.md).
