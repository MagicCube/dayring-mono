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
| Home actions and lock clock | `src/apps/shell/pages/` | `HomePageController`, `LockPageController` |
| Firmware upload screen | `src/apps/shell/pages/FirmwareUpdatePage.*`, `src/apps/shell/ShellApplication.h` | `FirmwareUpdatePage`, `StaticPageController<FirmwareUpdatePage>` |
| Upload preparation and frame completion | `src/platform/runtime/Shell.cpp` | `prepareFirmwareUpdate`, `isFirmwareUpdateReady` |

`runtime::Shell` is the singleton facade owning manager/container and referencing the HAL-owned `PowerManager`; `apps::shell::ShellApplication` is a resident app owning page controllers. Use the facade for production opens; direct manager calls bypass home/lock URL policy.

Lock uses Runtime's temporary presentation outside history; unlock restores the same instance/location, even when locking Shell itself. The first successful manual lock after boot keeps the frontlight at 20% for 10 seconds from lock entry; subsequent locks turn it off immediately. Unlock restores 20%. Idle dims the frontlight at 52 seconds, then turns it off and locks at 60 seconds when the foreground app allows idle locking. Shell checks `PowerManager::isIdleLockDue()` after input dispatch, even during display refresh. Automatic locks skip the first-lock lighting grace and consume it; unlocking restores the previous page and restarts the idle timer. Input/update continue during refresh; the adapter emits power presses, completed single-contact swipes and completed-tap releases, in that priority order. Both swipe endpoints are converted to logical portrait coordinates.

Checks: `make test-shell-facade`, `make test-home-entry`, `make test-power-manager`, `make test-shell`.

Firmware upload preparation opens `app://shell/firmware-update` even from lock. The page has a fixed black background, white `NDot2XL` title, and a light-gray Roboto M `Keep USB Connected` hint centered at the same 40 px bottom inset as the unlock hint. Preparation stops ordinary app updates, navigation, and input until reboot. It submits one frame without an extra clearing pass or animation. Readiness requires both composition completion and the HAL's physical refresh completion; it is not a fixed delay. The existing panel refresh policy remains in use.


The container bottom-edge upward gesture requires at least 200 px of upward net displacement to unlock while locked, restoring the interrupted page and frontlight. Power presses lock only while unlocked; while locked they show the unlock hint without unlocking or lighting the frontlight. The gesture callback does nothing when `isHome()` matches the configured application and complete navigation location (including query/fragment). Otherwise it calls `goHome()`. Explicit `goHome()` and home URL opens retain their existing re-entry behavior. Swipe qualification comes from the SDK: at least 60 native pixels on either axis within 700 ms, emitted on release.

LockPage renders a solid black background with centered text: a white Roboto L date (`Wed 26 Aug`) starting 40 px below the top, white Ndot 57 time (`NDot120`), and light-gray Roboto M `Charged 85%` text only while charging. Date and time stay in place when the charging line is hidden. LockPageController checks external-power status every second and invalidates immediately on change. Percentage is sampled on entry, connection, and each displayed minute change; an unavailable percentage falls back to `Charging`. On PaperMono, the SDK charging flag means external power is present. At a reported 100%, the line reads `Fully Charged`; this is a display convention, not a charger-termination signal. Disconnecting hides the line. Connection changes while locked light the frontlight for five seconds; the initial sample and percentage changes do not. No sleep wake source is added. Entry uses a single rendered frame without a separate white clearing pass.

Unsuccessful lock-screen swipes, touch presses/releases, and power presses show `Swipe Up to Unlock` in centered light-gray Roboto M near the bottom (40 px bottom inset). Repeated input restarts its five-second timer without redundant redraws. Expiry invalidates the page, and lock entry clears the hint. Successful bottom-edge swipes remain consumed by the container and restore the interrupted page.
