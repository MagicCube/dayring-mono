# HAL Code Map

| Find | File under `src/platform/hal/` | Symbols |
| --- | --- | --- |
| Board bring-up and fatal errors | `Hardware.cpp` | `begin`, `fatal` |
| Centered boot bitmap and monochrome submission | `Hardware.cpp`, `BootImage.h`, `generated/BootImage.h` | `displayBootImage`, `boot::composeRow` |
| Input sampling and activity | `Hardware.cpp` | `update`, `powerButtonPressed`, `touchTapped`, `touchSwiped`, `hasInputActivity` |
| Buffer ownership and refresh completion | `Hardware.h`, `Hardware.cpp` | `Framebuffer`, `framebuffer`, `displayReady`, `refreshDisplay` |
| Checked battery/charging reads | `Hardware.cpp` | `readBatteryPercent`, `readCharging` |
| Power lifecycle and lock/activity policy | `PowerManager.h`, `PowerManager.cpp` | `begin`, `update`, `notifyActivity`, `setLocked` |
| Private frontlight initialization, boot feedback and idle stages | `PowerManager.cpp` | `_beginFrontlight`, `_updateFrontlight`, `_activateFrontlight`, `_setFrontlightBrightness` |
| RTC initialization/read policy | `RtcClock.cpp` | `initializeRtc`, `clockTime` |

HAL owns static devices and a `hal::PowerManager`, which owns the SDK frontlight driver and encapsulates frontlight policy in private methods; the SDK owns framebuffer storage. Render only when ready. Refresh is asynchronous; `update()` verifies commit before releasing the buffer. Current boot validates PaperMono-Lite, 16 MB flash, at least 8 MB PSRAM and physical 800 x 480 geometry.

SDK `PaperMonoBoard.h::ensureBooted()` retries PMIC/IOE1 initialization with 10 ms pauses and a 1000 ms retry budget before HAL declares a fatal error. Normal successful startup has no retry delay. Only success is cached; `M5Ioe1.h::begin()` also leaves failed probes/configuration retryable. Initialization retries happen before consumers enable rails; once boot succeeds, subsequent calls do not clear active outputs. `make test-board-startup` covers delayed readiness, failed probes/writes, bounded failure, recovery after failure and successful-init idempotence using the real SDK headers with a simulated I2C bus.

Build/board config: `platformio.ini`. Upload RTC sync: `tools/platformio-upload-time.py`. Host tests stub hardware; on-device verification is needed for driver behavior. `make build`/`make upload` also run broad formatting.

Arduino runtime logging is disabled (`CORE_DEBUG_LEVEL=0`), and HAL does not initialize a diagnostic serial transport or reporter task. This also avoids the Debug-level chip report before `setup()`. Board bring-up relies on the SDK's per-device power/reset settling delays, without an additional blanket 2500 ms delay. The app disables PMIC single-click reset intentionally; the long-hold download escape remains enabled.

HAL calls `PowerManager::begin()` during bring-up and `PowerManager::update()` after input sampling on every loop, including during display refresh. These call the private `_beginFrontlight()` and `_updateFrontlight()` methods. Boot lighting turns on immediately at 20% during board bring-up, before display initialization or the first update, to provide visible startup feedback. Idle time starts when it lights: 10% after 52 seconds, then off after another 8 seconds (60 seconds total). Activity restores 20%. The first manual lock after each `begin()` lights the frontlight at 20% for 10 seconds from lock entry, then turns it off; repeated lock requests and locked input do not extend this grace period. Automatic idle locks and later locks turn it off immediately. Shell enters the lock screen at the 60-second idle timeout when the foreground app allows idle locking. An automatic first lock consumes the lighting grace. Unlock restores 20% and restarts the normal idle timer, including when it interrupts the first-lock grace period. Check with `make test-power-manager`.

`touchSwiped` exposes `InputManager::wasSwipe` start/end points in normalized panel-native coordinates from the latest synchronous update. Runtime owns orientation conversion; ApplicationContainer owns bottom-edge capture.
