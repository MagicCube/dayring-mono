# Shell Code Map

| Find | File | Symbols |
| --- | --- | --- |
| Boot and loop order | `src/main.cpp` | `setup`, `loop` |
| Production facade and owned services | `src/platform/runtime/Shell.h` | `Shell` |
| Home alias, global lock and update policy | `src/platform/runtime/Shell.cpp` | `open`, `goHome`, `isHome`, `_handleHomeGesture`, `lock`, `unlock`, `update`, `onInput` |
| Configured home destination | `platformio.ini` | `DAYRING_HOME_URL` |
| Power lifecycle and lock/activity policy | `src/platform/hal/PowerManager.cpp` | `begin`, `update`, `notifyActivity`, `setLocked` |
| Private frontlight boot feedback and idle stages | `src/platform/hal/PowerManager.cpp` | `_beginFrontlight`, `_updateFrontlight`, `_activateFrontlight`, `_setFrontlightBrightness` |
| Hardware-to-runtime input translation | `src/platform/runtime/Input.cpp` | `dispatchInput` |
| Application factories/residency | `src/apps/RegisterApplications.cpp` | `registerApplications` |
| Built-in home/lock app | `src/apps/shell/ShellApplication.cpp` | `onCreate`, `onEnter` |
| Home actions and lock clock | `src/apps/shell/pages/` | `HomeScreen`, `LockScreen` |

`runtime::Shell` is the singleton facade owning manager/container and referencing the HAL-owned `PowerManager`; `apps::shell::ShellApplication` is a resident app owning pages. Use the facade for production opens; direct manager calls bypass home/lock URL policy.

Lock uses Runtime's temporary presentation outside history; unlock restores the same instance/location, even when locking Shell itself. The first successful manual lock after boot keeps the frontlight at 20% for 10 seconds from lock entry; subsequent locks turn it off immediately. Unlock restores 20%. Idle dims the frontlight at 52 seconds, then turns it off and locks at 60 seconds when the foreground app allows idle locking. Shell checks `PowerManager::isIdleLockDue()` after input dispatch, even during display refresh. Automatic locks skip the first-lock lighting grace and consume it; unlocking restores the previous page and restarts the idle timer. Input/update continue during refresh; the adapter emits power presses, completed single-contact swipes and completed-tap releases, in that priority order. Both swipe endpoints are converted to logical portrait coordinates.

Checks: `make test-shell-facade`, `make test-home-entry`, `make test-power-manager`, `make test-shell`.


The container home-gesture callback does nothing while locked or when `isHome()` matches the configured application and complete navigation location (including query/fragment). Otherwise it calls `goHome()`. Explicit `goHome()` and home URL opens retain their existing re-entry behavior. Swipe qualification comes from the SDK: at least 60 native pixels on either axis within 700 ms, emitted on release.

LockScreen renders a solid black background with white Ndot 57 time text (`NDot120`). Entry uses a single rendered frame without a separate white clearing pass.
