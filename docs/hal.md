# HAL Code Map

| Find | File under `src/platform/hal/` | Symbols |
| --- | --- | --- |
| Board bring-up and fatal errors | `Hardware.cpp` | `begin`, `fatal` |
| Centered boot bitmap and monochrome submission | `Hardware.cpp`, `BootImage.h`, `generated/BootImage.h` | `displayBootImage`, `boot::composeRow` |
| Input sampling and activity | `Hardware.cpp` | `update`, `powerButtonPressed`, `touchTapped`, `touchSwiped`, `hasInputActivity` |
| Buffer ownership and refresh completion | `Hardware.h`, `Hardware.cpp` | `Framebuffer`, `framebuffer`, `displayReady`, `refreshDisplay` |
| Checked battery/charging reads | `Hardware.cpp` | `readBatteryPercent`, `readCharging` |
| Power lifecycle and lock/activity policy | `PowerManager.h`, `PowerManager.cpp` | `begin`, `update`, `notifyActivity`, `notifyPowerConnectionChanged`, `setLocked` |
| Private frontlight initialization, boot feedback and idle stages | `PowerManager.cpp` | `_beginFrontlight`, `_updateFrontlight`, `_activateFrontlight`, `_setFrontlightBrightness` |
| RTC initialization/read policy | `RtcClock.cpp` | `initializeRtc`, `clockTime` |
| USB firmware upload handshake | `FirmwareUpload.cpp` | `beginFirmwareUpload`, `pollFirmwareUpload` |

## Contracts

- HAL owns devices and `PowerManager`; SDK owns framebuffer storage. Render only when ready; `update()` confirms asynchronous refresh completion before releasing the buffer.
- Target: PaperMono-Lite, 16 MB flash, at least 8 MB PSRAM, native 800 × 480 buffer. Product orientation and electrical constraints: [hardware](hardware.md).
- SDK `PaperMonoBoard::ensureBooted()` retries PMIC/IOE1 initialization for up to 1000 ms. Failed initialization remains retryable; successful calls must preserve active rails. No extra blanket startup delay.
- Logging is disabled (`CORE_DEBUG_LEVEL=0`). PMIC single-click reset is disabled; long-hold download escape remains enabled.
- Update power management after input sampling on every loop, including during refresh. HAL exposes native touch coordinates; runtime converts orientation.

## Power policy

`PowerManager.cpp` is the source for timing and brightness constants:

- Boot/activity: 20%; idle: 10% at 52 seconds, off at 60 seconds. Shell applies idle lock when the foreground app permits it.
- First manual lock: 20% for 10 seconds; later or automatic locks: off immediately. Automatic lock consumes the first-lock grace. Locked input/repeated locks do not extend it.
- Unlock restores normal activity lighting/timing. A known external-power connection change lights a locked device for five seconds without unlocking; initial sampling does not.

## Firmware upload

`FirmwareUpload.cpp` serves a dedicated USB CDC control channel after Shell initialization, without waiting for a host. `DAYRING PREPARE_UPDATE` → `DAYRING PREPARING` → `DAYRING READY`; READY requires physical frame completion. Input polling is bounded.

`tools/platformio-firmware-update/platformio-firmware-update.py` retries preparation because opening native USB can reset the board. Before recognition, missing support/serial errors allow normal upload; after PREPARING, timeout/disconnect aborts upload to protect refresh. Older firmware cannot prepare the screen; failed uploads may leave it visible until reboot.

Config: `platformio.ini`; RTC upload sync: `tools/platformio-upload-time/platformio-upload-time.py`. `make build`/`make upload` also format unrelated sources.

Checks: `make test-board-startup test-power-manager`; `.pio-core/penv/bin/python tests/FirmwareUploadHookTest.py`. Driver behavior, USB reset and image retention require device verification.
