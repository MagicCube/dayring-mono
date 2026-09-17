# Service Lifecycle and Task Dispatch

## Context

Background capabilities must outlive application eviction and own their data, subscriptions, and task handles. Introduce local services with explicit startup order, and rename the existing `TaskManager` directly to `TaskDispatchService`, preserving its scheduler implementation. This plan builds on the current uncommitted tasking work; Bluetooth, Calendar, and Weather implementations remain future work.

## Approach

### 1. Define the lifecycle — `src/platform/runtime/services/Service.h` (add)

Define `platform::runtime::Service` with a virtual destructor, `bool start()`, and `void stop()`. Construction performs no background work. Start and stop are idempotent; failed start cleans up its own partial resources. Stop unregisters callbacks and cancels owned tasks before returning, without destroying the service instance. All lifecycle operations run on the owning loop, outside task execution and callbacks; An optional `update(uint32_t now)` hook defaults to no work. No threads, remote calls, or asynchronous shutdown protocol are introduced.

### 2. Own and order services — `src/plMtform/runtime/ServiceManager.h/.cpp` (add)

Use an ordered `vector<unique_ptr<Service>>`, a started-prefix count, and `bool begin(uint32_t now = 0)` / `void stop()`. The constructor installs TaskDispatchService first; `tasks()` returns its stable typed reference. `bool add(unique_ptr<Service>)` appends services before the first startup attempt; registration is then sealed. Dependencies are constructor references to previously installed services, not runtime lookups. Additional typed accessors can be added when actual business services exist; no string registry, RTTI lookup, dependency graph, or automatic sorting.

Begin in registration order, drive optional update hooks with `update(now)` in that order, and stop in reverse order. On failure, stop the successful prefix in reverse and leave the manager stopped; a later start retries the same instances. Destruction stops services and destroys instances explicitly in reverse order. Future composition is TaskDispatch → Bluetooth → Calendar / Weather; Calendar and Weather need no relative ordering unless a real dependency appears. Being later in startup order does not require an artificial direct dependency on every earlier service. Service readiness means initialized, not connected to a phone or network.

### 3. Migrate the scheduler — `src/platform/tasking/services/TaskDispatchService.h/.cpp` (add)

Rename the class and files directly, retaining `platform::tasking`, existing task APIs, capacity, fairness, retries, deadlines, and RAII handle semantics. Implement Service without an extra scheduler wrapper. Update the friend declaration in `Task.h`. Instances begin stopped; start enables submissions, and stop first rejects submissions, marks unfinished handles Cancelled, then releases queued tasks. Replace permanent `pause()` / `isPaused()` with lifecycle state (`isRunning()`); replace `SubmitError::Paused` with `Stopped`. Restart begins with an empty queue, preserving monotonically increasing task IDs so old IDs and notifications cannot affect new tasks. Existing terminal handle states remain inspectable.

Keep `update(now, budget)` and its clock semantics. Before each managed startup, Shell passes `millis()` to `ServiceManager::begin(now)`, which samples it through a zero-budget update while dispatch is stopped, so newly submitted delays use current time, including after a long stop. Standalone tests supply virtual time explicitly. Start must not reset that sample. Background services retain their own TaskHandles; tasks never borrow application/controller pointers. Stop is not permitted from an executing task, avoiding destruction/restart reentrancy.

### 4. Integrate boot and shutdown — `src/platform/runtime/Shell.h/.cpp`, `src/main.cpp` (modify)

Replace Shell's scheduler member with ServiceManager, declared before ApplicationManager so application references and handles are destroyed first. Add `bool startServices()` and `services()`; retain `tasks()` as a typed forwarding convenience. Firmware setup explicitly starts services after HAL initialization and before entering applications; startup failure follows the existing fatal path. Constructors and route discovery do not start services.

Continue dispatching eight task steps before foreground updates, including during lock and display refresh. Firmware preparation opens the update page first, then stops all services in reverse order; dispatch stops last and rejects subsequent submissions. Refuse service restart while firmware updating. Future services must synchronously quiesce their callbacks/resources before stop returns; safe asynchronous write draining requires a separate design when such I/O exists. No persistence across reboot or deep sleep is promised.

### 5. Migrate integration and tests — `Makefile`, `tests/`, `tools/preview/`, `docs/` (modify)

Rename scheduler tests/target to TaskDispatchService and explicitly start standalone instances. Add lifecycle tests with fake dependent services. Update Shell fixtures to start services, and firmware assertions to expect stopped dispatch and cancelled work. Preview capture starts services explicitly; route listing remains side-effect free. Add service sources to host builds and route-preview discovery without adding them to isolated View previews. Update code maps and scheduling documentation to describe ownership, startup, shutdown, and interface borrowing. Future business interfaces may use pure virtual classes; do not create speculative Calendar/Weather APIs now.

## Key Files

| File | Change | Notes |
|------|--------|-------|
| `src/platform/runtime/services/Service.h`, `src/platform/runtime/services/ServiceManager.h`, `src/platform/runtime/services/ServiceManager.cpp` | add | Lifecycle, ordered ownership, typed task access |
| `src/platform/tasking/TaskManager.h`, `src/platform/tasking/TaskManager.cpp` | delete | Replaced by renamed service files |
| `src/platform/tasking/services/TaskDispatchService.h`, `src/platform/tasking/services/TaskDispatchService.cpp` | add | Existing scheduling logic with lifecycle |
| `src/platform/tasking/Task.h` | modify | Friend name and stopped submission error |
| `src/platform/runtime/Shell.h`, `src/platform/runtime/Shell.cpp`, `src/main.cpp` | modify | Ownership, explicit startup, firmware shutdown |
| `tests/TaskManagerTest.cpp` | delete | Rename scheduler regression suite |
| `tests/TaskDispatchServiceTest.cpp`, `tests/ServiceManagerTest.cpp` | add | Scheduler preservation and lifecycle tests |
| `tests/ShellFacadeTest.cpp`, `tests/ShellApplicationTest.cpp`, `tests/HomeEntryTest.cpp` | modify | Explicit startup and service independence |
| `Makefile`, `tools/preview/build.py`, `tools/preview/native/Main.cpp`, `tools/preview/tests/test_cli.py` | modify | Build sources, test targets, preview startup |
| `docs/tasking.md`, `docs/runtime.md`, `docs/shell.md` | modify | Updated code maps and service contracts |

## Verification

- Run `make test-service-manager test-task-dispatch-service`: assert startup order, reverse stop/destruction, failed-start rollback, idempotency, sealed registration, cancellation, safe retained handles, restart timing, stale-ID isolation, and existing deterministic scheduler behavior. Keep core tests C++20, SDK-free, and exception-free.
- Run `make test-shell-facade test-shell test-home-entry`: verify application eviction/lock leaves services running, firmware preparation stops them and prevents restart, and service-owned tasks survive application destruction until service shutdown.
- Run `make test-preview`: validate new source discovery, explicit capture startup, and route listing without service startup. No UI rendering changes are intended.
- Format only modified C++ files, then run `.pio-core/penv/bin/python -m platformio run -e papermono`; do not use the broad-formatting `make build` target for this migration.
