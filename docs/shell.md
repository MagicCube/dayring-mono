# Shell Code Map

| Find | File | Symbols |
| --- | --- | --- |
| Boot and loop order | `src/main.cpp` | `setup`, `loop` |
| Cooperative background work | `src/platform/tasking/services/TaskDispatchService.*`, `src/platform/runtime/Shell.cpp` | `tasks`, `update`, `stop` |
| Ordered service startup and shutdown | `src/platform/runtime/services/ServiceManager.*`, `src/platform/runtime/Shell.cpp` | `startServices`, `services`, `begin`, `update`, `stop` |
| Production facade and owned services | `src/platform/runtime/Shell.h` | `Shell` |
| Home alias, global lock and update policy | `src/platform/runtime/Shell.cpp` | `open`, `goHome`, `isHome`, `_handleHomeGesture`, `lock`, `unlock`, `update`, `onInput` |
| First-time setup gate | `src/platform/runtime/Shell.cpp`, `src/apps/shell/pages/PairingPageController.*` | `openStartupPage`, `update` |
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

- `runtime::Shell` owns ServiceManager, which starts FrontlightService, PowerService, TaskDispatchService, BLEService, RPCService, DeviceControlService, TimeService, then CalendarService and stops them in reverse order. Services are declared before applications so application handles are destroyed first. Service updates run before input dispatch and foreground updates, including while locked or refreshing; each loop permits eight task steps.
- `runtime::Shell` owns manager/container and accesses PowerService through ServiceManager. `ShellApplication` is a resident application owning page controllers. Use the facade for production opens; direct manager calls bypass home/lock URL policy.
- Lock temporarily presents outside navigation history; unlock restores the same instance/location, including when interrupting Shell itself. Power timing belongs in [HAL](hal.md).
- Input/update continue during refresh. Input priority: power press, completed single-contact swipe, completed tap release. Runtime converts both swipe endpoints to logical portrait coordinates.
- Container owns bottom-edge gesture capture; Shell requires 200 px upward displacement to unlock. While unlocked, it goes Home unless already at the configured application and full location (query/fragment included). Explicit Home opens retain re-entry behavior.
- Power presses lock only while unlocked. The lock-screen Power button toggles the frontlight immediately while remaining locked. The first touch interaction lights the screen for eight seconds without a hint. A second interaction within that window renews eight seconds of lighting and shows the hint for five seconds. Successful unlock gestures are consumed by the container.

## Lock status

`LockPageController` samples external power each second; percentage on entry, connection and displayed-minute changes. SDK charging means external power is present. Unknown percentage displays `Charging`; 100% displays `Fully Charged` as a UI convention, not charger termination. Known connection changes also trigger temporary lighting and do not shorten an active lock-interaction light window. No sleep wake source is added.

## Lock calendar preview

LockPageController reads `CalendarService::upcoming()` on entry, stably prioritizes all-day occurrences, and keeps at most three rows and subscribes while active. Snapshot/time changes mark its data dirty; minute/date updates also refresh relative day labels. `onLeave` releases the scoped subscription, and re-entry reads the latest service state. The page receives presentation-only title/location/time/day Props with no service or hardware dependency.

The list sits 24 px above the bottom edge and remains visible while the unlock hint is shown. The hint shares the fixed line below the clock with charging status and takes priority over it. It lasts five seconds, then returns to the current charging status or an empty line. The first interaction after entering Lock or after eight seconds without an interaction only lights the screen; a second interaction within that window shows the hint. Further interactions renew the light window and, when shown, the independent five-second hint timer. Press/release pairs count once. Successful unlock swipes are consumed by the container and return to ordinary unlocked power policy.

Timed events have two left-aligned rows: RobotoL title, then RobotoM light-gray time range (`14:00-15:00`). All-day events use a single RobotoL title row. The event marker is a light-gray rounded rectangle. Location remains in the service data but is not displayed. Today's group is labeled `Upcoming`; it and `Tomorrow` use RobotoS. Empty groups are omitted. All-day entries are stably prioritized in `Upcoming` before applying the three-row limit, including tomorrow's all-day entries. Timed tomorrow entries remain in `Tomorrow`. There are no card backgrounds. FreeInk `maxLines = 1` handles overflow without manual character truncation.

Layout spacing remains on a four-pixel grid: 32 px side insets, 8 px between the rule and text, 4 px between text rows, 4 px between events within a group, 16 px between groups, 4 px below each group label, and 24 px bottom inset. Font line boxes round up to a multiple of four. Each vertical marker is a slim rounded rectangle, 6 px wide with a 3 px corner radius. Single-line and two-line text blocks share a 4 px optical upward adjustment against the marker. Font slots include the Chinese coverage documented in [Fonts](fonts.md).


Use native `--calendar` fixtures in the [preview CLI](../tools/preview/README.md#local-calendar-fixtures). No mock data is compiled into firmware.

## Firmware update

Preparation opens `app://shell/firmware-update` even from lock, then stops services in reverse startup order, cancels unfinished tasks, and rejects new submissions, and suppresses ordinary updates, navigation and input until reboot. Submit one frame without a separate clearing pass. READY requires composition and physical refresh completion, never a fixed delay. USB handshake: [HAL](hal.md).

Checks: `make test-shell-facade test-home-entry test-power-service test-shell`.

## First-time pairing

Firmware calls `openStartupPage()` after service startup and application registration. A radio with no stored bonds opens `app://shell/pairing`; an existing bond goes to the configured Home even if the phone is offline. Native previews report radio Unavailable and retain ordinary Home behavior; the pairing route can be captured directly.

While the startup gate is active, Shell rejects ordinary opens, Home gestures and manual locking; ShellApplication disables idle lock on the pairing page. Manual lock requests, lock URLs and power presses are also blocked whenever this page is active, including direct route entry outside the startup gate. Firmware-update preparation remains available. Service updates continue; once a stored bond appears, Shell goes Home outside controller callbacks and resets activity timing so a long setup session cannot immediately idle-lock Home. Connection or encryption without a bond does not finish setup.

PairingPageController observes BLE state and owns the cached QR matrix. PairingPage renders only explicit Props, with a download link to `https://dayring.ai`. This is a website destination, not an app-store availability guarantee. Future application authorization should replace the BLE-bond-only setup criterion.

Checks: `make test-pairing-startup test-qr-code`; `tools/preview/preview capture app://shell/pairing`.
