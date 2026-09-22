# HAL Code Map

| Find | File under `src/platform/hal/` | Symbols |
| --- | --- | --- |
| Board bring-up and fatal errors | `Hardware.cpp` | `begin`, `fatal` |
| Centered boot bitmap and monochrome submission | `Hardware.cpp`, `BootImage.h`, `generated/BootImage.h` | `displayBootImage`, `boot::composeRow` |
| Per-loop hardware servicing, input sampling and activity | `Hardware.cpp` | `update`, `powerButtonPressed`, `touchTapped`, `touchSwiped`, `hasInputActivity` |
| Buffer ownership and refresh completion | `Hardware.h`, `Hardware.cpp` | `Framebuffer`, `framebuffer`, `displayReady`, `refreshDisplay` |
| Checked battery/charging reads | `Hardware.cpp` | `readBatteryPercent`, `readCharging` |
| Runtime frontlight control | `services/FrontlightService.h/.cpp` | `start`, `brightness`, `setBrightness`, `turnOn`, `turnOff` |
| Runtime power policy | `services/PowerService.h/.cpp` | `start`, `update`, `notifyActivity`, `setLocked` |
| Frontlight driver and boot feedback | `Frontlight.h`, `Frontlight.cpp` | `beginFrontlight`, `setFrontlightBrightness`, `frontlightBrightness` |
| Deferred software restart and administrative RPC | `services/DeviceControlService.*`, `Restart.*` | `update`, `restartDevice` |
| RTC initialization/read policy | `RtcClock.cpp` | `initializeRtc`, `clockTime` |
| USB firmware upload handshake | `FirmwareUpload.cpp` | `beginFirmwareUpload`, `pollFirmwareUpload` |

The shared FAT mount is owned by `src/platform/storage/FatFilesystem.h::mountFatFilesystem`; font startup and calendar persistence reuse it without formatting on failure. Font data loads after HAL bring-up; see [Fonts](fonts.md).

## Contracts

- HAL owns devices and the frontlight driver; ServiceManager owns runtime power policy; SDK owns framebuffer storage. Render only when ready; `update()` verifies refresh completion before releasing the buffer.
- Target: PaperMono-Lite, 16 MB flash, at least 8 MB PSRAM, native 800 × 480 buffer. Product orientation and electrical constraints: [hardware](hardware.md).
- SDK `PaperMonoBoard::ensureBooted()` retries PMIC/IOE1 initialization for up to 1000 ms. Failed initialization remains retryable; successful calls must preserve active rails. No extra blanket startup delay.
- Logging is disabled (`CORE_DEBUG_LEVEL=0`). PMIC single-click reset is disabled; long-hold download escape remains enabled.
- Shell updates PowerService after HAL input sampling on every normal loop, including during refresh. HAL exposes native touch coordinates; runtime converts orientation.

## Display tones

The standard SDK `DisplayTarget` draws into the 1-bit framebuffer. Gray colors
use ordered black/white dithering, including on black backgrounds, and HAL submits
through `displayBufferAsync(FULL_REFRESH)` using PaperMono's B/W OTP path.
This is simulated gray, not physical grayscale. Previews export that same packed
framebuffer so dots and endpoints match firmware. No custom grayscale planes,
waveforms, or grayscale refresh cadence are used.

## Power policy

`src/platform/hal/services/PowerService.cpp` owns runtime timing and target-brightness policy and calls `frontlight::FrontlightService`; HAL `Frontlight.cpp` initializes the driver and 20% boot feedback before display initialization:

- Boot/activity: 20%; idle: 10% at 52 seconds, off at 60 seconds. Shell applies idle lock when the foreground app permits it.
- First manual lock: 20% for 10 seconds; later or automatic locks: off immediately. Each accepted locked interaction lights at 20% for a fresh eight seconds via `notifyLockInteraction`; timing is wrap-safe. Automatic lock consumes the first-lock grace. Repeated lock requests do not extend the initial grace; explicit locked interactions use the separate eight-second window.
- Unlock restores normal activity lighting/timing. A known external-power connection change lights a locked device for five seconds without unlocking; it does not shorten an active interaction light window. Initial sampling does not light the screen.

## Firmware upload

HAL `begin()` initializes the dedicated USB CDC control channel without waiting for a host. HAL `update()` polls `FirmwareUpload.cpp` after servicing display completion on every loop, including while a refresh is pending. The main loop starts after Shell initialization. `DAYRING PREPARE_UPDATE` → `DAYRING PREPARING` → `DAYRING READY`; READY requires physical frame completion. Input polling is bounded.

`tools/platformio-firmware-update/platformio-firmware-update.py` retries preparation because opening native USB can reset the board. Before recognition, missing support/serial errors allow normal upload; after PREPARING, timeout/disconnect aborts upload to protect refresh. Older firmware cannot prepare the screen; failed uploads may leave it visible until reboot.

Config: `platformio.ini`. Firmware uploads do not inject host time or modify a valid RTC. `make build`/`make upload` also format unrelated sources.

Checks: `make test-board-startup test-power-service`; `.pio-core/penv/bin/python tests/FirmwareUploadHookTest.py`. Driver behavior, USB reset and image retention require device verification.

## Bluetooth

The BLE domain owns the NimBLE radio and GATT server through `src/platform/ble/services/BLEService.h/.cpp`; HAL does not initialize a second Bluetooth stack. ServiceManager owns its lifecycle. See [BLE](ble.md) for pairing and NVS persistence.

RTC initialization preserves readable hardware time. An invalid/reset RTC gets a fixed baseline of 2000-01-01 (Saturday) so startup can proceed until BLE RPC supplies actual time and timezone. Neither upload timestamps nor compiler date/time are used; the retired `paper-mono/rtc-stamp` NVS key is ignored, without erasing existing NVS or BLE bonds. Checks: `make test-rtc-clock test-time-sync`.
