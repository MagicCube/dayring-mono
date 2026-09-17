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

## Contracts

- `runtime::Shell` owns manager/container and borrows HAL's `PowerManager`. `ShellApplication` is a resident application owning page controllers. Use the facade for production opens; direct manager calls bypass home/lock URL policy.
- Lock temporarily presents outside navigation history; unlock restores the same instance/location, including when interrupting Shell itself. Power timing belongs in [HAL](hal.md).
- Input/update continue during refresh. Input priority: power press, completed single-contact swipe, completed tap release. Runtime converts both swipe endpoints to logical portrait coordinates.
- Container owns bottom-edge gesture capture; Shell requires 200 px upward displacement to unlock. While unlocked, it goes Home unless already at the configured application and full location (query/fragment included). Explicit Home opens retain re-entry behavior.
- Power presses lock only while unlocked. Locked input shows the five-second unlock hint without unlocking or extending normal lighting. Successful unlock gestures are consumed by the container.

## Lock status

`LockPageController` samples external power each second; percentage on entry, connection and displayed-minute changes. SDK charging means external power is present. Unknown percentage displays `Charging`; 100% displays `Fully Charged` as a UI convention, not charger termination. Only known connection changes trigger temporary lighting. No sleep wake source is added.

## Firmware update

Preparation opens `app://shell/firmware-update` even from lock, then suppresses ordinary updates, navigation and input until reboot. Submit one frame without a separate clearing pass. READY requires composition and physical refresh completion, never a fixed delay. USB handshake: [HAL](hal.md).

Checks: `make test-shell-facade test-home-entry test-power-manager test-shell`.
