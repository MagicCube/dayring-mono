ENV ?= papermono
PYTHON ?= $(CURDIR)/.pio-core/penv/bin/python
PIO = "$(PYTHON)" -m platformio
CLANG_FORMAT ?= clang-format
HOST_CXX ?= c++
CXX_SOURCES := $(shell find src include tests tools/preview -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \))

APPLICATION_SOURCES := src/platform/ui/Page.cpp src/platform/runtime/Application.cpp src/platform/runtime/ApplicationRouter.cpp \
    src/platform/runtime/ApplicationNavigation.cpp src/platform/runtime/ApplicationManager.cpp src/platform/runtime/AppURL.cpp
POWER_SOURCES := src/platform/hal/PowerManager.cpp tests/stubs/PowerManager.cpp
HOST_UI_FLAGS := -isystem freeink-sdk/libs/hardware/Rtc/include -Isrc -Itests/stubs -isystem freeink-sdk/libs/ui/FreeInkUI/include

.DEFAULT_GOAL := build
.PHONY: format build upload monitor test test-application-manager test-shell test-navigation test-home-entry test-shell-facade test-power-manager test-board-startup

format:
	$(CLANG_FORMAT) -i $(CXX_SOURCES)

build: format
	$(PIO) run -e $(ENV)

upload: format
	$(PIO) run -e $(ENV) -t upload

monitor:
	$(PIO) device monitor -e $(ENV)

test: test-application-manager test-shell test-navigation test-home-entry test-shell-facade test-power-manager test-board-startup

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
		tests/ShellApplicationTest.cpp src/apps/RegisterApplications.cpp src/apps/shell/ShellApplication.cpp src/apps/shell/pages/*.cpp src/apps/shell/components/*.cpp \
		src/apps/common/PlaceholderApplication.cpp src/apps/typography/*.cpp src/platform/runtime/Shell.cpp $(POWER_SOURCES) src/platform/ui/ApplicationContainer.cpp \
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
			-D"DAYRING_HOME_URL=\"$$home_url\"" tests/HomeEntryTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp $(POWER_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) -o "$$home_test" || exit $$?; \
		case "$$home_url" in app://calendar/*) expected=valid ;; *) expected=invalid ;; esac; \
		"$$home_test" "$$expected" || exit $$?; \
	done

test-shell-facade:
	@facade_test=$$(mktemp /tmp/dayring-shell-facade-test.XXXXXX); \
	trap 'rm -f "$$facade_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror $(HOST_UI_FLAGS) \
		tests/ShellFacadeTest.cpp tests/stubs/RuntimeDispatch.cpp src/apps/shell/components/StatusBar.cpp freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp src/platform/runtime/Shell.cpp $(POWER_SOURCES) src/platform/ui/ApplicationContainer.cpp $(APPLICATION_SOURCES) \
		-o "$$facade_test" && "$$facade_test"

test-power-manager:
	@power_test=$$(mktemp /tmp/dayring-power-manager-test.XXXXXX); \
	trap 'rm -f "$$power_test"' EXIT; \
	$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Isrc -Itests/stubs \
		tests/PowerManagerTest.cpp src/platform/hal/PowerManager.cpp \
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
		tools/preview/tests/RuntimeTest.cpp tools/preview/native/HostHardware.cpp src/platform/hal/PowerManager.cpp \
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
