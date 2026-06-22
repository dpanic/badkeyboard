PIO  ?= pio
ENV  ?= esp32-s3-geek
PORT ?= /dev/ttyACM0

.PHONY: build flash install monitor dev clean list boards setup recover help

build:          ## compile
	$(PIO) run -e $(ENV)

flash install:  ## compile + upload to the board
	$(PIO) run -e $(ENV) -t upload

monitor:        ## serial monitor (works because CDC_ON_BOOT=1)
	$(PIO) device monitor -b 115200

dev:            ## flash then immediately monitor
	$(PIO) run -e $(ENV) -t upload -t monitor

clean:          ## remove build artifacts
	$(PIO) run -e $(ENV) -t clean

list:           ## list serial devices
	$(PIO) device list

boards:         ## show matching board ids (verify ENV)
	$(PIO) boards t-display

setup:          ## install PlatformIO core if missing (isolated penv)
	@command -v pio >/dev/null 2>&1 || { \
	  curl -fsSL -o /tmp/get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py && \
	  python3 /tmp/get-platformio.py && \
	  mkdir -p $(HOME)/.local/bin && \
	  ln -sf $(HOME)/.platformio/penv/bin/pio $(HOME)/.local/bin/pio ; }
	@pio --version 2>/dev/null && echo "PlatformIO ready (ensure ~/.local/bin is on PATH)" || echo "PlatformIO missing — see above"

recover:        ## how to enter download mode when HID owns the port
	@echo "Hold BOOT, tap RST, release BOOT, then: make flash"

help:           ## list targets
	@grep -E '^[a-z].*##' $(MAKEFILE_LIST) | sed -E 's/:.*## /\t/'
