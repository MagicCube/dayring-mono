# Shell Code Map

| Find | File | Symbols |
| --- | --- | --- |
| Boot and loop order | `src/main.cpp` | `setup`, `loop` |
| Cooperative background work | `src/platform/tasking/services/TaskDispatchService.*`, `src/platform/runtime/Shell.cpp` | `tasks`, `update`, `stop` |
| Ordered service startup and shutdown | `src/platform/runtime/services/ServiceManager.*`, `src/platform/runtime/Shell.cpp` | `startServices`, `services`, `begin`, `update`, `stop` |
| Production facade and owned services | `src/platform/runtime/Shell.h` | `Shell` |
| Home alias, global lock and update policy | `src/platform/runtime/Shell.cpp` | `open`, `goHome`, `isHome`, `_handleHomeGesture`, `lock`, `unlock`, `update`, `onInput` |
| Configured home destination | `platformio.ini` | `DAYRING_HOME_URL` |
| Power lifecycle and lock/activity policy | `src/platform/hal/services/PowerService.cpp` | `start`, `update`, `notifyActivity`, `setLocked` |
| Runtime frontlight control | `src/platform/hal/services/FrontlightService.cpp` | `start`, `brightness`, `setBrightness`, `turnOn`, `turnOff` |
| Frontlight hardware and boot feedback | `src/platform/hal/Frontlight.cpp` | `beginFrontlight`, `setFrontlightBrightness` |
| Shared clock and display minute revisions | `src/platform/time/services/TimeService.cpp` | `start`, `update`, `time`, `displayTime`, `minuteRevision` |
| Hardware-to-runtime input translation | `src/platform/runtime/Input.cpp` | `dispatchInput` |
| Application factories/residency | `src/apps/RegisterApplications.cpp` | `registerApplications` |
| Built-in home/lock app | `src/apps/shell/ShellApplication.cpp` | `onCreate`, `onEnter` |
| Home actions and lock clock | `src/apps/shell/pages/` | `HomePageController`, `LockPageController` |
| Firmware upload screen | `src/apps/shell/pages/FirmwareUpdatePage.*`, `src/apps/shell/ShellApplication.h` | `FirmwareUpdatePage`, `StaticPageController<FirmwareUpdatePage>` |
| Upload preparation and frame completion | `src/platform/runtime/Shell.cpp` | `prepareFirmwareUpdate`, `isFirmwareUpdateReady` |

## Contracts

- `runtime::Shell` owns ServiceManager, which starts FrontlightService, PowerService, TaskDispatchService, then TimeService and stops them in reverse order. Services are declared before applications so application handles are destroyed first. Service updates run before input dispatch and foreground updates, including while locked or refreshing; each loop permits eight task steps.
- `runtime::Shell` owns manager/container and accesses PowerService through ServiceManager. `ShellApplication` is a resident application owning page controllers. Use the facade for production opens; direct manager calls bypass home/lock URL policy.
- Lock temporarily presents outside navigation history; unlock restores the same instance/location, including when interrupting Shell itself. Power timing belongs in [HAL](hal.md).
- Input/update continue during refresh. Input priority: power press, completed single-contact swipe, completed tap release. Runtime converts both swipe endpoints to logical portrait coordinates.
- Container owns bottom-edge gesture capture; Shell requires 200 px upward displacement to unlock. While unlocked, it goes Home unless already at the configured application and full location (query/fragment included). Explicit Home opens retain re-entry behavior.
- Power presses lock only while unlocked. Locked input shows the five-second unlock hint without unlocking or extending normal lighting. Successful unlock gestures are consumed by the container.

## Lock status

`LockPageController` samples external power each second; percentage on entry, connection and displayed-minute changes. SDK charging means external power is present. Unknown percentage displays `Charging`; 100% displays `Fully Charged` as a UI convention, not charger termination. Only known connection changes trigger temporary lighting. No sleep wake source is added.

## Firmware update

Preparation opens `app://shell/firmware-update` even from lock, then stops services in reverse startup order, cancels unfinished tasks, and rejects new submissions, and suppresses ordinary updates, navigation and input until reboot. Submit one frame without a separate clearing pass. READY requires composition and physical refresh completion, never a fixed delay. USB handshake: [HAL](hal.md).

Checks: `make test-shell-facade test-home-entry test-power-service test-shell`.
