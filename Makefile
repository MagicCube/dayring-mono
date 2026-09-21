ENV ?= papermono
PYTHON ?= $(CURDIR)/.pio-core/penv/bin/python
PIO = "$(PYTHON)" -m platformio
# GUI terminals may omit Homebrew from PATH even when clang-format is installed.
CLANG_FORMAT ?= $(firstword $(shell command -v clang-format 2>/dev/null) $(wildcard /opt/homebrew/bin/clang-format /usr/local/bin/clang-format /opt/homebrew/opt/llvm/bin/clang-format /usr/local/opt/llvm/bin/clang-format) clang-format)
HOST_CXX ?= c++
CXX_SOURCES := $(shell find src include tests tools/preview -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \))

QR_SOURCES := src/platform/ui/QRCode.cpp src/platform/ui/QRCodeEncoder.cpp
APPLICATION_SOURCES := src/platform/ui/PageController.cpp src/platform/runtime/Application.cpp src/platform/runtime/ApplicationRouter.cpp \
    src/platform/runtime/ApplicationNavigation.cpp src/platform/runtime/ApplicationManager.cpp src/platform/runtime/AppURL.cpp
FRONTLIGHT_SOURCES := src/platform/hal/services/FrontlightService.cpp src/platform/hal/Frontlight.cpp
POWER_SOURCES := src/platform/hal/services/PowerService.cpp $(FRONTLIGHT_SOURCES)
BLE_SOURCES := src/platform/ble/services/BLEService.cpp
RPC_SOURCES := src/platform/rpc/services/RPCService.cpp
TIME_SOURCES := src/platform/time/services/TimeService.cpp $(RPC_SOURCES)
HOST_UI_FLAGS := -isystem freeink-sdk/libs/hardware/Rtc/include -Isrc -Itests/stubs -isystem freeink-sdk/libs/ui/FreeInkUI/include

.DEFAULT_GOAL := build
.PHONY: format build upload monitor dev-server test test-application-manager test-shell test-navigation test-home-entry test-shell-facade test-power-service test-board-startup

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
		src/apps/common/*.cpp src/apps/typography/*.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp \
		$(APPLICATION_SOURCES) \
		src/platform/runtime/Input.cpp src/platform/runtime/Display.cpp src/platform/fonts/Fonts.cpp \
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
			-D"DAYRING_HOME_URL=\"$$home_url\"" tests/HomeEntryTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) -o "$$home_test" || exit $$?; \
		case "$$home_url" in app://calendar/*) expected=valid ;; *) expected=invalid ;; esac; \
		"$$home_test" "$$expected" || exit $$?; \
	done

test-shell-facade:
	@facade_test=$$(mktemp /tmp/dayring-shell-facade-test.XXXXXX); \
	trap 'rm -f "$$facade_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/ShellFacadeTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(BLE_SOURCES) $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) \
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
		tests/FontsTest.cpp src/platform/fonts/Fonts.cpp -o "$$font_test" && "$$font_test"

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
		tests/ServiceManagerTest.cpp tests/stubs/RuntimeDispatch.cpp $(POWER_SOURCES) $(TIME_SOURCES) src/platform/runtime/services/ServiceManager.cpp $(BLE_SOURCES) src/platform/tasking/services/TaskDispatchService.cpp \
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
		tests/PairingStartupTest.cpp src/apps/shell/ShellApplication.cpp src/apps/shell/pages/*.cpp $(QR_SOURCES) tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp src/apps/shell/components/StatusBarController.cpp src/apps/shell/views/*.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp src/platform/tasking/services/TaskDispatchService.cpp src/platform/runtime/services/ServiceManager.cpp $(POWER_SOURCES) $(TIME_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) \
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
