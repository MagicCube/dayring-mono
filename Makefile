ENV ?= papermono
PYTHON ?= $(CURDIR)/.pio-core/penv/bin/python
FONT_PYTHON ?= python3
UPLOAD_PORT ?=
PIO = "$(PYTHON)" -m platformio
# GUI terminals may omit Homebrew from PATH even when clang-format is installed.
CLANG_FORMAT ?= $(firstword $(shell command -v clang-format 2>/dev/null) $(wildcard /opt/homebrew/bin/clang-format /usr/local/bin/clang-format /opt/homebrew/opt/llvm/bin/clang-format /usr/local/opt/llvm/bin/clang-format) clang-format)
HOST_CXX ?= c++
# Generated bitmap tables are owned by the font generator, not source formatting.
CXX_SOURCES := $(shell find src include tests tools/preview -path 'src/platform/fonts/generated' -prune -o -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print)

FONT_SOURCES := $(wildcard src/platform/fonts/*.cpp)
FONT_FLAGS := -isystem freeink-sdk/libs/book/FreeInkBook/third_party/miniz

QR_SOURCES := src/platform/ui/QRCode.cpp src/platform/ui/QRCodeEncoder.cpp
APPLICATION_SOURCES := src/platform/ui/PageController.cpp src/platform/runtime/Application.cpp src/platform/runtime/ApplicationRouter.cpp \
    src/platform/runtime/ApplicationNavigation.cpp src/platform/runtime/ApplicationManager.cpp src/platform/runtime/AppURL.cpp
FRONTLIGHT_SOURCES := src/platform/hal/services/FrontlightService.cpp src/platform/hal/Frontlight.cpp
POWER_SOURCES := src/platform/hal/services/PowerService.cpp $(FRONTLIGHT_SOURCES)
BLE_SOURCES := src/platform/ble/services/BLEService.cpp
RPC_SOURCES := src/platform/rpc/services/RPCService.cpp src/platform/rpc/MessageChannel.cpp
CONTROL_SOURCES := src/platform/hal/services/DeviceControlService.cpp src/platform/hal/Restart.cpp
CALENDAR_SOURCES := $(wildcard src/platform/calendar/*.cpp src/platform/calendar/services/*.cpp)
TIME_SOURCES := src/platform/time/services/TimeService.cpp $(RPC_SOURCES)
HOST_UI_FLAGS := -isystem freeink-sdk/libs/hardware/Rtc/include -Isrc -Itests/stubs -isystem freeink-sdk/libs/ui/FreeInkUI/include

.DEFAULT_GOAL := build
.PHONY: format build upload fs\:upload monitor dev-server test test-application-manager test-shell test-navigation test-home-entry test-shell-facade test-power-service test-board-startup

format:
	@command -v "$(CLANG_FORMAT)" >/dev/null 2>&1 || { \
		echo "clang-format not found. Install it or run make CLANG_FORMAT=/absolute/path/to/clang-format build." >&2; \
		exit 1; \
	}
	$(CLANG_FORMAT) -i $(CXX_SOURCES)

build: format
	$(PIO) run -e $(ENV)

upload: format
	$(PIO) run -e $(ENV) -t upload

# Build and upload the existing filesystem assets without regenerating fonts.
fs\:upload:
	@fs_log=$$(mktemp /tmp/dayring-buildfs.XXXXXX); \
	trap 'rm -f "$$fs_log"' EXIT; \
	$(PIO) run -e $(ENV) -t buildfs >"$$fs_log" 2>&1 || { cat "$$fs_log"; exit 1; }
	@$(PYTHON) tools/sync-fs.py --image .pio/build/$(ENV)/fatfs.bin --offset 0x410000 --esptool .pio-core/penv/bin/esptool $(if $(UPLOAD_PORT),--port "$(UPLOAD_PORT)")

monitor:
	$(PIO) device monitor -e $(ENV)

dev-server:
	./tools/dayring-cli/dayring-cli dev-server

test: test-application-manager test-shell test-navigation test-home-entry test-shell-facade test-power-service test-board-startup

test-board-startup:
	@board_test=$$(mktemp /tmp/dayring-board-startup-test.XXXXXX); \
	trap 'rm -f "$$board_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Itests/board-stubs \
		-Ifreeink-sdk/libs/hardware/BoardConfig/include tests/BoardStartupTest.cpp -o "$$board_test" && \
	for scenario in ready delayed expander-delayed probe-retry config-retry output-retry timeout-retry; do \
		"$$board_test" "$$scenario" || exit $$?; \
	done

test-application-manager:
	@application_manager_test=$$(mktemp /tmp/dayring-application-manager-test.XXXXXX); \
	trap 'rm -f "$$application_manager_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) tests/ApplicationManagerTest.cpp $(APPLICATION_SOURCES) -o "$$application_manager_test" && \
	for scenario in urls registration switching dispatch eviction limits; do \
		"$$application_manager_test" "$$scenario" || exit $$?; \
	done

test-shell:
	@shell_test=$$(mktemp /tmp/dayring-shell-test.XXXXXX); \
	trap 'rm -f "$$shell_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Isrc -Itests/stubs \
		-isystem freeink-sdk/libs/ui/FreeInkUI/include -isystem freeink-sdk/libs/hardware/Rtc/include \
		tests/ShellApplicationTest.cpp src/apps/RegisterApplications.cpp src/apps/shell/ShellApplication.cpp src/apps/shell/pages/*.cpp $(QR_SOURCES) src/apps/shell/components/*.cpp src/apps/shell/views/*.cpp \
		src/apps/common/*.cpp src/apps/typography/*.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(CALENDAR_SOURCES) $(CONTROL_SOURCES) $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp \
		$(APPLICATION_SOURCES) \
		src/platform/runtime/Input.cpp src/platform/runtime/Display.cpp $(FONT_SOURCES) $(FONT_FLAGS) \
		freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$$shell_test" && "$$shell_test"

test-navigation:
	@navigation_test=$$(mktemp /tmp/dayring-navigation-test.XXXXXX); \
	trap 'rm -f "$$navigation_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/ApplicationNavigationTest.cpp $(APPLICATION_SOURCES) -o "$$navigation_test" && "$$navigation_test"

test-home-entry:
	@home_test=$$(mktemp /tmp/dayring-home-test.XXXXXX); \
	trap 'rm -f "$$home_test"' EXIT; \
	for home_url in 'app://calendar/path/to/page?id=42&mode=week#top' 'app://home' 'invalid'; do \
		$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
			-D"DAYRING_HOME_URL=\"$$home_url\"" tests/HomeEntryTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(CALENDAR_SOURCES) $(CONTROL_SOURCES) $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) -o "$$home_test" || exit $$?; \
		case "$$home_url" in app://calendar/*) expected=valid ;; *) expected=invalid ;; esac; \
		"$$home_test" "$$expected" || exit $$?; \
	done

test-shell-facade:
	@facade_test=$$(mktemp /tmp/dayring-shell-facade-test.XXXXXX); \
	trap 'rm -f "$$facade_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/ShellFacadeTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(CALENDAR_SOURCES) $(CONTROL_SOURCES) $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) \
		-o "$$facade_test" && "$$facade_test"

test-power-service:
	@power_test=$$(mktemp /tmp/dayring-power-manager-test.XXXXXX); \
	trap 'rm -f "$$power_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Isrc -Itests/stubs \
		tests/PowerServiceTest.cpp $(POWER_SOURCES) \
		-o "$$power_test" && "$$power_test"

.PHONY: test-fonts
test: test-fonts

test-fonts:
	@font_test=$$(mktemp /tmp/dayring-font-test.XXXXXX); \
	trap 'rm -f "$$font_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/FontsTest.cpp $(FONT_SOURCES) $(FONT_FLAGS) -o "$$font_test" && "$$font_test"

test-fonts: test-font-loader

.PHONY: test-font-loader
test-font-loader:
	@font_loader_test=$$(mktemp /tmp/dayring-font-loader-test.XXXXXX); \
	trap 'rm -f "$$font_loader_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) $(FONT_FLAGS) \
		tests/FontAssetTest.cpp $(FONT_SOURCES) -o "$$font_loader_test" && "$$font_loader_test"

.PHONY: test-preview test-preview-runtime
test: test-preview

test-preview: test-preview-runtime
	PYTHONPYCACHEPREFIX="$(CURDIR)/.cache/preview/python" PYTHON="$(PYTHON)" "$(PYTHON)" tools/preview/tests/test_cli.py

test-preview-runtime:
	@mkdir -p "$(CURDIR)/.cache/preview/tmp"
	@preview_test=$$(mktemp "$(CURDIR)/.cache/preview/tmp/runtime-test.XXXXXX"); \
	trap 'rm -f "$$preview_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Isrc -Itools/preview/native/include \
		-isystem freeink-sdk/libs/ui/FreeInkUI/include -isystem freeink-sdk/libs/hardware/Rtc/include \
		tools/preview/tests/RuntimeTest.cpp tools/preview/native/HostHardware.cpp tools/preview/native/FrameBuffer.cpp src/platform/hal/Frontlight.cpp \
		$(APPLICATION_SOURCES) freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp -o "$$preview_test" && "$$preview_test"

# Accept URL goals or command-line assignments without interpolating them into shell code.
# Export the literal URL so query metacharacters remain data.
ifneq ($(filter preview,$(MAKECMDGOALS)),)
PREVIEW_URL_GOALS := $(filter app://%,$(MAKECMDGOALS))
PREVIEW_URL_VARIABLES := $(foreach name,$(filter app://%,$(.VARIABLES)),$(if $(filter command line,$(origin $(name))),$(name)))
ifneq ($(words $(PREVIEW_URL_GOALS) $(PREVIEW_URL_VARIABLES)),0)
ifneq ($(words $(PREVIEW_URL_GOALS) $(PREVIEW_URL_VARIABLES)),1)
$(error Pass exactly one app:// URL to make preview)
endif
endif
export DAYRING_PREVIEW_URL := $(if $(PREVIEW_URL_VARIABLES),$(PREVIEW_URL_VARIABLES)=$(value $(PREVIEW_URL_VARIABLES)),$(if $(PREVIEW_URL_GOALS),$(PREVIEW_URL_GOALS),app://shell/))
ifneq ($(PREVIEW_URL_GOALS),)
$(subst :,\:,$(PREVIEW_URL_GOALS)):
	@:
endif
endif

.PHONY: preview
preview:
	@PYTHON="$(PYTHON)" ./tools/preview/preview capture "$$DAYRING_PREVIEW_URL"

.PHONY: test-task-dispatch-service
test: test-task-dispatch-service

test-task-dispatch-service:
	@task_test=$$(mktemp /tmp/dayring-task-manager-test.XXXXXX); \
	trap 'rm -f "$$task_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc \
		tests/TaskDispatchServiceTest.cpp src/platform/tasking/services/TaskDispatchService.cpp -o "$$task_test" && "$$task_test"

.PHONY: test-service-manager
test: test-service-manager

test-service-manager:
	@service_test=$$(mktemp /tmp/dayring-service-manager-test.XXXXXX); \
	trap 'rm -f "$$service_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/ServiceManagerTest.cpp tests/stubs/RuntimeDispatch.cpp $(POWER_SOURCES) $(TIME_SOURCES) src/platform/runtime/services/ServiceManager.cpp $(CALENDAR_SOURCES) $(CONTROL_SOURCES) $(BLE_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp \
		-o "$$service_test" && "$$service_test"

.PHONY: test-time-service
test: test-time-service

test-time-service:
	@time_test=$$(mktemp /tmp/dayring-time-service-test.XXXXXX); \
	trap 'rm -f "$$time_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/TimeServiceTest.cpp $(TIME_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp -o "$$time_test" && "$$time_test"

.PHONY: test-frontlight-service
test: test-frontlight-service

test-frontlight-service:
	@frontlight_test=$$(mktemp /tmp/dayring-frontlight-service-test.XXXXXX); \
	trap 'rm -f "$$frontlight_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc -Itests/stubs \
		tests/FrontlightServiceTest.cpp $(FRONTLIGHT_SOURCES) -o "$$frontlight_test" && "$$frontlight_test"

.PHONY: test-ble-service
test: test-ble-service

test-ble-service:
	@ble_test=$$(mktemp /tmp/dayring-ble-test.XXXXXX); \
	trap 'rm -f "$$ble_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -DARDUINO_ARCH_ESP32 \
		-Itests/ble-stubs -Isrc tests/BLEServiceTest.cpp $(BLE_SOURCES) -o "$$ble_test" && "$$ble_test"

.PHONY: test-pairing-startup
test: test-pairing-startup

test-pairing-startup:
	@facade_test=$$(mktemp /tmp/dayring-shell-facade-test.XXXXXX); \
	trap 'rm -f "$$facade_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/PairingStartupTest.cpp src/apps/shell/ShellApplication.cpp src/apps/shell/pages/*.cpp $(QR_SOURCES) tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(CALENDAR_SOURCES) $(CONTROL_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) \
		-o "$$facade_test" && "$$facade_test"

.PHONY: test-qr-code
test: test-qr-code

test-qr-code:
	@qr_test=$$(mktemp /tmp/dayring-qr-test.XXXXXX); \
	trap 'rm -f "$$qr_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc \
		tests/QRCodeTest.cpp $(QR_SOURCES) -o "$$qr_test" && "$$qr_test"

.PHONY: test-rpc-service test-time-sync
test: test-rpc-service test-time-sync

test-rpc-service:
	@rpc_test=$$(mktemp /tmp/dayring-rpc-test.XXXXXX); \
	trap 'rm -f "$$rpc_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc \
		tests/RPCServiceTest.cpp $(RPC_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp -o "$$rpc_test" && "$$rpc_test"

test-time-sync:
	@sync_test=$$(mktemp /tmp/dayring-time-sync-test.XXXXXX); \
	trap 'rm -f "$$sync_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/TimeSyncTest.cpp $(TIME_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp -o "$$sync_test" && "$$sync_test"

.PHONY: test-rtc-clock
test: test-rtc-clock

test-rtc-clock:
	@rtc_test=$$(mktemp /tmp/dayring-rtc-test.XXXXXX); \
	trap 'rm -f "$$rtc_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/RtcClockTest.cpp src/platform/hal/RtcClock.cpp -o "$$rtc_test" && "$$rtc_test"

.PHONY: test-rpc-message-channel
test: test-rpc-message-channel

test-rpc-message-channel:
	@rpc_test=$$(mktemp /tmp/dayring-rpc-channel-test.XXXXXX); \
	trap 'rm -f "$$rpc_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc \
		tests/RPCMessageChannelTest.cpp src/platform/rpc/MessageChannel.cpp -o "$$rpc_test" && "$$rpc_test"

.PHONY: test-rpc-interop
test-rpc-interop:
	@mkdir -p .cache/rpc-interop
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc \
		tests/RPCInteropPeer.cpp $(RPC_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp \
		-o .cache/rpc-interop/cpp-peer
	swiftc tools/dayring-cli/Sources/DayringBLE/RPCPeer.swift \
		tools/dayring-cli/Sources/DayringBLE/RPCMessageChannel.swift tools/dayring-cli/Tests/Interop/*.swift \
		-o .cache/rpc-interop/swift-peer
	.cache/rpc-interop/swift-peer "$(CURDIR)/.cache/rpc-interop/cpp-peer"

.PHONY: test-device-control
test: test-device-control

test-device-control:
	@control_test=$$(mktemp /tmp/dayring-device-control-test.XXXXXX); \
	trap 'rm -f "$$control_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions -Isrc -Itests/stubs \
		tests/DeviceControlServiceTest.cpp src/platform/hal/services/DeviceControlService.cpp $(RPC_SOURCES) \
		src/platform/tasking/services/TaskDispatchService.cpp -o "$$control_test" && "$$control_test"

.PHONY: test-cli-administration
test-cli-administration:
	swift build --package-path tools/dayring-cli --scratch-path .cache/dayring-cli --build-system native
	@mkdir -p .cache/rpc-admin
	@swift_bin=$$(swift build --package-path tools/dayring-cli --scratch-path .cache/dayring-cli --build-system native --show-bin-path); \
	swiftc -I "$$swift_bin/Modules" "$$swift_bin"/DayringBLE.build/*.swift.o \
		tools/dayring-cli/Sources/DayringCLI/Options.swift tools/dayring-cli/Sources/DayringCLI/AdministrationRunner.swift \
		tools/dayring-cli/Tests/Administration/AdminChecks.swift -o .cache/rpc-admin/cli-checks && .cache/rpc-admin/cli-checks

.PHONY: test-cli-calendar
test-cli-calendar:
	swift build --package-path tools/dayring-cli --scratch-path .cache/dayring-cli --build-system native
	@mkdir -p .cache/calendar
	@swift_bin=$$(swift build --package-path tools/dayring-cli --scratch-path .cache/dayring-cli --build-system native --show-bin-path); \
	swiftc -I "$$swift_bin/Modules" "$$swift_bin"/DayringBLE.build/*.swift.o \
		tools/dayring-cli/Sources/DayringCLI/Calendar/CalendarSnapshot.swift \
		tools/dayring-cli/Sources/DayringCLI/Calendar/CalendarRPCService.swift \
		tools/dayring-cli/Tests/Calendar/CalendarChecks.swift -o .cache/calendar/checks && .cache/calendar/checks

.PHONY: test-calendar
test: test-calendar
test-calendar:
	@mkdir -p .cache/calendar
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/CalendarServiceTest.cpp $(filter-out src/platform/calendar/CalendarClock.cpp,$(CALENDAR_SOURCES)) \
		$(RPC_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp -o .cache/calendar/service-checks
	.cache/calendar/service-checks
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/CalendarModelTest.cpp src/platform/calendar/Calendar.cpp src/platform/calendar/CalendarStorage.cpp \
		src/platform/calendar/Json.cpp -o .cache/calendar/model-checks
	.cache/calendar/model-checks

.PHONY: test-calendar-interop
test-calendar-interop:
	@mkdir -p .cache/calendar
	swiftc tools/dayring-cli/Sources/DayringCLI/Calendar/CalendarSnapshot.swift \
		tools/dayring-cli/Tests/Calendar/CalendarWireFixture.swift -o .cache/calendar/swift-fixture
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -fno-exceptions $(HOST_UI_FLAGS) \
		tests/CalendarInteropTest.cpp $(filter-out src/platform/calendar/CalendarClock.cpp,$(CALENDAR_SOURCES)) \
		$(RPC_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp -o .cache/calendar/interop-checks
	.cache/calendar/swift-fixture | .cache/calendar/interop-checks

.PHONY: test-font-assets
test-font-assets:
	$(FONT_PYTHON) tests/FontAssetsTest.py

.PHONY: test-fs-sync
test: test-fs-sync
test-fs-sync:
	$(PYTHON) tests/SyncFsTest.py
