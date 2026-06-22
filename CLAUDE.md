# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

GhostScribe: single-sketch PlatformIO/Arduino firmware for a **Waveshare ESP32-S3-GEEK**
(USB-stick form factor, ESP32-S3R2, onboard 1.14" ST7789 LCD). The device enumerates as a
USB-HID keyboard, types a boot message once, then types a random word from a pool at a random
5–300 s interval, mirroring each word on the LCD. BOOT button = arm/disarm kill switch.

All logic lives in [src/main.cpp](src/main.cpp). There is no test suite — verification is
done on hardware (see below).

## Commands

```bash
make setup     # install PlatformIO into ~/.platformio/penv if missing (pip --user is blocked by PEP 668)
make build     # pio run -e esp32-s3-geek
make flash     # build + upload over /dev/ttyACM0   (alias: make install)
make monitor   # serial monitor @115200 (works because the device is a composite HID+CDC)
make dev       # flash then monitor
make clean / make boards / make list / make recover
```

`pio` lives at `~/.platformio/penv/bin/pio` (symlinked to `~/.local/bin/pio`). The env name is
`esp32-s3-geek`; override the Makefile's `ENV`/`PORT` vars if needed.

### Flashing caveat (permissions)

`/dev/ttyACM0` is `root:dialout`. The user is in the `dialout` group but it only takes effect
after re-login. **Within a session that predates that, flash via group activation:**

```bash
sg dialout -c 'export PATH="$HOME/.local/bin:$PATH"; cd /home/user/projects/mine/badusb && make flash'
```

`board_upload.use_1200bps_touch=yes` lets PlatformIO reflash even while the device is already
running in HID mode. If an upload still won't start: hold **BOOT**, tap **RST**, release BOOT,
then `make flash`.

## Architecture / non-obvious details

- **USB HID requires `-D ARDUINO_USB_MODE=0`** (USB-OTG/TinyUSB), *not* `=1` (Hardware CDC/JTAG,
  which cannot present an HID device). Paired with `-D ARDUINO_USB_CDC_ON_BOOT=1` the device is a
  **composite HID keyboard + CDC serial**, so the typing works *and* `make monitor`/auto-reflash
  keep working. Both flags are in [platformio.ini](platformio.ini); `main.cpp` has a `#if
  ARDUINO_USB_MODE == 1 #error` guard so a wrong build fails to compile.
- **Display = ST7789 over SPI, driven by LovyanGFX** via the hand-written `LGFX` class at the top
  of `main.cpp`. The GEEK's pins are non-standard and were verified against the CircuitPython
  board definition: SCLK=12, MOSI=11, CS=10, DC=8, RST=9, BL=7; panel native 135×240 with
  offsets 52/40, `invert=true`, `setRotation(1)` → 240×135 landscape. This board is **not** a
  LilyGO T-Display-S3 (which is an 8-bit parallel bus — that config gives a black screen here).
- **Board def:** `board = esp32-s3-devkitc-1` (no dedicated GEEK board) with
  `board_build.arduino.memory_type = qio_qspi` (S3R2 has QSPI PSRAM, not OPI).
- **Behavior knobs are grouped in a TUNABLES block** at the top of `main.cpp`: `BOOT_MESSAGE`,
  `PHRASES[]` (ASCII / US layout), `MIN/MAX_INTERVAL_MS`, `KEY_MIN/MAX_MS` jitter,
  `PRESS_ENTER_AFTER`. Scheduling is non-blocking (`millis()` + `esp_random()`, wrap-safe
  due-check); only per-character typing jitter and the one-time enumeration settle use `delay()`.
- The LCD draw path uses change-detection (`drawDynamic`) to avoid flicker; the typed word is
  auto-sized to fit 240 px in `drawPhrase`.
