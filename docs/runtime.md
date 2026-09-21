# Runtime Code Map

| Find | File under `src/platform/runtime/` | Symbols |
| --- | --- | --- |
| URL syntax and intent reasons | `AppURL.cpp`, `Intent.h` | `AppURL::parse`, `Intent::Reason` |
| Service lifecycle and ordered startup | `services/Service.h`, `services/ServiceManager.h/.cpp` | `add`, `begin`, `update`, `stop`, `tasks` |
| Registration, lazy creation, switching, eviction | `ApplicationManager.cpp` | `registerApplication`, `_prepare`, `_enter`, `_retainTransient` |
| App lifecycle and render dirtiness | `Application.cpp`, `Application.h` | `_activate`, `_deactivate`, `requestRender` |
| Exact path registration and borrowed controllers | `ApplicationRouter.cpp` | `registerPage`, `resolve` |
| Page stack and lifecycle | `ApplicationNavigation.cpp` | `_navigate`, `pop`, `_resume`, `_suspend` |
| Lock presentation mechanics | `ApplicationManager.cpp` | `_interrupt`, `_restore` |
| Shared time and display revisions | `../time/services/TimeService.h/.cpp` | `time`, `displayTime`, `minuteRevision` |
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

## Background tasks

`Shell::services()` owns the production services independently of foreground application lifetime; `Shell::tasks()` forwards to TaskDispatchService. The pure C++20 API, timing rules, and host tests are documented in [Task scheduling](tasking.md). TimeService samples RTC wall time independently of the monotonic task clock.

## Services

`ServiceManager` is the service owner and startup runner. Its constructor installs FrontlightService, PowerService, TaskDispatchService, then TimeService and BLEService; add later services in dependency order before the first `begin()`. Pass dependencies through constructors and keep typed borrowed references; no dependency graph or automatic sorting is involved. Future Calendar/Weather services follow these built-ins. Every service must live in its owning domain's `services/` directory: hardware services in `hal/services/`, task dispatch in `tasking/services/`, time in `time/services/`, and the lifecycle framework in `runtime/services/`. BLEService lives in `ble/services/`. Directory organization does not add a `services` namespace; existing domain namespaces remain unchanged.

`begin(now)` samples the task clock and starts services in registration order. `update(now)` calls their optional update hooks in the same order; TaskDispatchService executes at most eight steps per call. Failure rolls back the successful prefix in reverse order; the failing service must clean up its own partial startup. Stop and destruction run in reverse order, leaving dependencies available while dependents clean up. Registration seals on the first startup attempt; begin/stop are idempotent and a stopped manager can restart the same instances. Lifecycle calls belong on the owning loop outside task execution and callbacks. Only the manager should start/stop managed services. Recursive begin is rejected and recursive update is ignored; stopping from lifecycle or update callbacks is prohibited.

Firmware setup invokes `Shell::startServices()` after HAL initialization. Construction and route discovery do not start services; native capture explicitly starts them. Being started means initialized, not connected. Services own background handles, unsubscribe and cancel work on stop, and must not retain application/controller pointers. Applications borrow service references; stopping a service does not destroy it. Tests: `make test-service-manager test-task-dispatch-service`.

Services are resident for the device session: ordinary applications do not start or stop them. `stop()` exists for firmware-update quiescence and teardown. PowerService stops policy execution without changing hardware power or brightness. HAL initializes the frontlight before display initialization.

TimeService reads RTC once at startup and at most once per second thereafter. `time()` is the latest sampled wall time; `displayTime()` preserves the displayed minute through second 00 and advances at second 01 or the next available sample. `minuteRevision()` changes on date/hour/minute changes and initial sampling; controllers retain only their last revision. Reads of these properties do not touch hardware. MinuteTicker has been removed. Tests: `make test-time-service test-power-service`.

FrontlightService owns the runtime brightness API (`brightness`, `setBrightness`, `turnOn`, `turnOff`). Startup adopts the HAL boot brightness without another initialization/write; stop leaves physical lighting unchanged. Turning on restores the last nonzero brightness (20% initially). PowerService receives FrontlightService by constructor reference and owns only activity/lock/idle policy, not a second brightness cache. Services remain resident during ordinary application use. Tests: `make test-frontlight-service test-power-service`.

BLEService owns the NimBLE peripheral independently of applications. Firmware startup advertises the Dayring service; disconnect resumes advertising. Native startup reports Unavailable without accessing a host radio. Pairing, NVS persistence, the encrypted status characteristic, and verification are documented in [BLE](ble.md). `ServiceManager::ble()` returns a borrowed reference.
