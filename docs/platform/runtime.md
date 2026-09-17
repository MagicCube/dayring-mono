# Runtime Code Map

| Find | File under `src/platform/runtime/` | Symbols |
| --- | --- | --- |
| URL syntax and intent reasons | `AppURL.cpp`, `Intent.h` | `AppURL::parse`, `Intent::Reason` |
| Registration, lazy creation, switching, eviction | `ApplicationManager.cpp` | `registerApplication`, `_prepare`, `_enter`, `_retainTransient` |
| App lifecycle and render dirtiness | `Application.cpp`, `Application.h` | `_activate`, `_deactivate`, `requestRender` |
| Exact path registration and borrowed controllers | `ApplicationRouter.cpp` | `registerPage`, `resolve` |
| Page stack and lifecycle | `ApplicationNavigation.cpp` | `_navigate`, `pop`, `_resume`, `_suspend` |
| Lock presentation mechanics | `ApplicationManager.cpp` | `_interrupt`, `_restore` |
| Callback reentrancy | `TransitionGuard.h` | `TransitionGuard` |

Ownership: manager → applications → page controllers/router/navigation. Router and history borrow page-controller pointers; repeated entries do not clone page state. Only the foreground app receives dispatch. Resident apps persist; transient apps use a recency cache (default two).

Ordinary `onEnter(Open)` handles route selection/fallback; manager open success does not guarantee the requested page exists. PageController callbacks cannot recursively navigate/switch apps. `Present`/`Restore` preserve normal history. Applications explicitly delegate page dispatch and invalidate visual changes.

New app entry: `src/apps/RegisterApplications.cpp`; example: `src/apps/common/PlaceholderApplication.cpp`. `app://typography/` opens `src/apps/typography/TypographyApplication.*`, which owns `TypographyPageController`; HomePageController launches it alongside Calendar and Test. Production inter-app transitions use [Shell](shell.md).

Checks: `make test-application-manager`, `make test-navigation`.

## Host Preview and Exact Routes

`ApplicationManager::open` and `Shell::open` accept `OpenMode::Exact` to reject unregistered page paths rather than accepting application fallback. `checkRoute` prepares the application and reports URL, registration, and parameter errors; `currentURL` reports the entered location. Invalid parameters are rejected before foreground navigation in both open modes. `Shell::resolveURL` centralizes Home alias resolution.

`ApplicationRouter::descriptions` exposes optional registration help and fullscreen state. `ApplicationManager::describeRoutes` discovers all applications before any is active, without entering pages; discovery can initialize all registered applications and is intended for short-lived tooling. Page validation uses the side-effect-free `PageController::acceptsLocation` hook before navigation leaves the current page. `LocationQuery` provides percent-decoded query lookup while preserving raw locations for lifecycle callbacks.

The host CLI at `tools/preview/preview` compiles the same application registrations and runtime. See [Preview CLI](../../tools/preview/README.md). Checks: `make test-preview`.
