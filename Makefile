ENV ?= papermono
PYTHON ?= $(CURDIR)/.pio-core/penv/bin/python
PIO = "$(PYTHON)" -m platformio
CLANG_FORMAT ?= clang-format
CXX_SOURCES := $(wildcard src/*.cpp src/*.h src/*.hpp include/*.h include/*.hpp)

.DEFAULT_GOAL := build
.PHONY: format build upload monitor

format:
	$(CLANG_FORMAT) -i $(CXX_SOURCES)

build: format
	$(PIO) run -e $(ENV)

upload: format
	$(PIO) run -e $(ENV) -t upload

monitor:
	$(PIO) device monitor -e $(ENV)
