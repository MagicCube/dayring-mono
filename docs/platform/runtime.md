# Runtime Code Map

| Find | File under `src/platform/runtime/` | Symbols |
| --- | --- | --- |
| URL syntax and intent reasons | `AppURL.cpp`, `Intent.h` | `AppURL::parse`, `Intent::Reason` |
| Registration, lazy creation, switching, eviction | `ApplicationManager.cpp` | `registerApplication`, `_prepare`, `_enter`, `_retainTransient` |
| App lifecycle and render dirtiness | `Application.cpp`, `Application.h` | `_activate`, `_deactivate`, `requestRender` |
| Exact path registration and page ownership | `ApplicationRouter.cpp` | `registerPage`, `resolve` |
| Page stack and lifecycle | `ApplicationNavigation.cpp` | `_navigate`, `pop`, `_resume`, `_suspend` |
| Lock presentation mechanics | `ApplicationManager.cpp` | `_interrupt`, `_restore` |
| Callback reentrancy | `TransitionGuard.h` | `TransitionGuard` |

Ownership: manager → applications → pages/router/navigation. Router and history borrow page pointers; repeated entries do not clone page state. Only the foreground app receives dispatch. Resident apps persist; transient apps use a recency cache (default two).

Ordinary `onEnter(Open)` handles route selection/fallback; manager open success does not guarantee the requested page exists. Page callbacks cannot recursively navigate/switch apps. `Present`/`Restore` preserve normal history. Applications explicitly delegate page dispatch and invalidate visual changes.

New app entry: `src/apps/RegisterApplications.cpp`; example: `src/apps/common/PlaceholderApplication.cpp`. `app://typography/` opens `src/apps/typography/TypographyApplication.*`, which owns `TypographyPage`; HomeScreen launches it alongside Calendar and Test. Production inter-app transitions use [Shell](shell.md).

Checks: `make test-application-manager`, `make test-navigation`.
