# GhostScribe

Waveshare **ESP32-S3-GEEK** (USB-stick form factor) that pretends to be a USB
keyboard and, at a **random interval**, types one of your phrases into whatever
window has focus — and shows it on the onboard 1.14" LCD.

> Authorized, owned-hardware, personal gadget. Slow, randomly-timed, with an
> on-device arm/disarm kill switch.

## Behavior

- On boot it types **`UKLJUCIO SAM SE`** once (and shows it on the LCD).
- Then every **5–300 s** (random) it types a random word from `PHRASES[]`.
- The LCD shows: title, current/last word (big), countdown, progress bar, counters.

## Quick start

```bash
make setup     # install PlatformIO (into ~/.platformio/penv) if missing
make flash     # compile + upload over /dev/ttyACM0
make monitor   # watch the serial log (typed: ...)
```

## Configure

Top of [`src/main.cpp`](src/main.cpp):

- `PHRASES[]` — the words it types (ASCII, US keyboard layout).
- `BOOT_MESSAGE` — the one-time boot announcement.
- `MIN_INTERVAL_MS` / `MAX_INTERVAL_MS` — random gap window (default 5 s–5 min).
- `KEY_MIN_MS` / `KEY_MAX_MS` — per-character typing jitter.
- `PRESS_ENTER_AFTER` — send Enter after each word.

## On-device controls (BOOT button)

- **Short press** — toggle ARM / OFF (kill switch; never types while OFF).
- **Long press (~1.2 s)** — panic disarm.

LCD state chip: green = ARM, amber = OFF, cyan = typing.

## Hardware (Waveshare ESP32-S3-GEEK)

- ESP32-S3R2 (2 MB QSPI PSRAM, 16 MB flash), native USB → enumerates as composite
  **HID keyboard + CDC serial** (because `ARDUINO_USB_MODE=0` + `ARDUINO_USB_CDC_ON_BOOT=1`).
- Onboard 1.14" **ST7789** LCD, 240×135, 4-wire SPI. Pin map (in the `LGFX` class):

  | SCLK | MOSI | CS | DC | RST | BL |
  |------|------|----|----|-----|----|
  | 12   | 11   | 10 | 8  | 9   | 7  |

  Display driver: **LovyanGFX** (`Bus_SPI` + `Panel_ST7789`, offsets 52/40, invert on).
- BOOT button on GPIO0.

## Two USB gotchas

1. **HID needs `ARDUINO_USB_MODE=0`** (USB-OTG/TinyUSB), not `=1` (Hardware CDC).
2. Re-flashing after HID takes the port usually still works (`use_1200bps_touch`).
   If an upload won't start: **hold BOOT, tap RST, release BOOT, then `make flash`**.

## Make targets

`make help` lists them: `build`, `flash`/`install`, `monitor`, `dev`, `clean`,
`list`, `boards`, `setup`, `recover`.
