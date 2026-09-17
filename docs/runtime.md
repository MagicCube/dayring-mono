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

## Contracts

- Manager owns applications; applications own controllers/router/navigation. Router/history borrow controller pointers; repeated entries do not clone state. Only the foreground app receives dispatch. Residents persist; transients use a recency cache (default two).
- `onEnter(Open)` selects routes/fallback: ordinary open success does not guarantee the requested page exists. `Present`/`Restore` preserve history. PageController callbacks cannot recursively navigate or switch apps (`TransitionGuard`).
- Applications explicitly delegate page dispatch and invalidate visual changes. Cleanup and suspension rules: [UI ownership](ui.md#ownership-and-lifecycle).
- Register new apps in `src/apps/RegisterApplications.cpp`; example: `src/apps/common/PlaceholderApplication.cpp`. Production transitions use [Shell](shell.md).

## Route validation and tooling

- `OpenMode::Exact` rejects unregistered paths. Both modes reject invalid parameters before foreground navigation using side-effect-free `PageController::acceptsLocation`.
- `checkRoute` prepares and validates; `currentURL` reports the entered location; `Shell::resolveURL` handles Home aliases.
- `ApplicationRouter::descriptions` exposes help/fullscreen metadata. `describeRoutes` may initialize every registered app without entering pages; intended for short-lived tooling.
- `LocationQuery` decodes query values while preserving raw locations for callbacks. [Preview CLI](../tools/preview/README.md) shares firmware registration and validation.

Checks: `make test-application-manager test-navigation test-preview`.
