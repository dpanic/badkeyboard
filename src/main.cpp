// GhostScribe — Waveshare ESP32-S3-GEEK USB-HID typer with onboard LCD.
//
// On boot it types BOOT_MESSAGE, then at a random interval types a random word
// from PHRASES[] into whatever window has focus. Every typed string is also shown
// on the onboard 1.14" ST7789 LCD (240x135). BOOT button arms/disarms (kill
// switch); long-press = panic disarm.
//
// USB HID requires USB-OTG / TinyUSB  ->  platformio.ini sets -D ARDUINO_USB_MODE=0.
// Display: ST7789 over SPI via LovyanGFX.

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "esp_random.h"

#if ARDUINO_USB_MODE == 1
#error "Set -D ARDUINO_USB_MODE=0 (USB-OTG/TinyUSB) in platformio.ini — HID is impossible in Hardware-CDC mode."
#endif

// ---- LovyanGFX config for the Waveshare ESP32-S3-GEEK 1.14" ST7789 (SPI) ----
// Pins verified against the CircuitPython board definition (named LCD aliases).
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;
public:
  LGFX(void) {
    { auto c = _bus.config();
      c.spi_host    = SPI2_HOST;
      c.spi_mode    = 0;
      c.freq_write  = 40000000;
      c.freq_read   = 16000000;
      c.spi_3wire   = false;
      c.use_lock    = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 12;
      c.pin_mosi = 11;
      c.pin_miso = -1;
      c.pin_dc   = 8;
      _bus.config(c); _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs   = 10;
      c.pin_rst  = 9;
      c.pin_busy = -1;
      c.panel_width  = 135;
      c.panel_height = 240;
      c.offset_x = 52;
      c.offset_y = 40;
      c.offset_rotation = 0;
      c.readable = false;
      c.invert   = true;
      c.rgb_order = false;
      c.dlen_16bit = false;
      c.bus_shared = false;
      _panel.config(c);
    }
    { auto c = _light.config();
      c.pin_bl = 7; c.invert = false; c.freq = 1000; c.pwm_channel = 0;
      _light.config(c); _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

// ============================ TUNABLES ============================
// Typed once, right after the device connects:
static const char *BOOT_MESSAGE = "UKLJUCIO SAM SE";

// The pool it picks from at each random interval (ASCII, US keyboard layout):
static const char *PHRASES[] = {
    "Duco",
    "Dubravko",
    "Dubravko Konj",
    "Duco najbolji",
    // TODO: 5th phrase "... ti ja" — pending clean spelling from the user.
};
static const size_t PHRASE_COUNT = sizeof(PHRASES) / sizeof(PHRASES[0]);

static const uint32_t MIN_INTERVAL_MS = 5UL * 1000UL;    // 5 s   — shortest gap
static const uint32_t MAX_INTERVAL_MS = 300UL * 1000UL;  // 300 s — longest gap
static const uint32_t SETTLE_MS       = 2500;            // wait for USB to enumerate before first keystroke
static const uint16_t KEY_MIN_MS      = 40;              // per-character jitter (min)
static const uint16_t KEY_MAX_MS      = 120;             // per-character jitter (max)
static const bool     PRESS_ENTER_AFTER = false;         // send Enter after each string?
static const uint32_t LONGPRESS_MS      = 1200;          // BOOT long-press = panic disarm
// =================================================================

static const int PIN_BTN_BOOT = 0;   // BOOT button, active LOW (INPUT_PULLUP)

// Screen geometry (landscape)
static const int SCR_W = 240;
static const int SCR_H = 135;

LGFX           tft;
USBHIDKeyboard Keyboard;

enum State { ST_ARMED, ST_DISARMED, ST_TYPING };

static bool     armed         = true;
static uint32_t intervalStart = 0;
static uint32_t nextFireAt    = 0;
static uint32_t typedCount    = 0;
static String   lastPhrase    = "-";

static uint32_t randInRange(uint32_t lo, uint32_t hi) {
  return lo + (esp_random() % (hi - lo + 1));
}

static void scheduleNext() {
  intervalStart = millis();
  nextFireAt = intervalStart + randInRange(MIN_INTERVAL_MS, MAX_INTERVAL_MS);
}

static uint16_t stateColor(State s) {
  switch (s) {
    case ST_ARMED:    return TFT_GREEN;
    case ST_DISARMED: return TFT_ORANGE;
    case ST_TYPING:   return TFT_CYAN;
  }
  return TFT_WHITE;
}

static const char *stateName(State s) {
  switch (s) {
    case ST_ARMED:    return "ARM";
    case ST_DISARMED: return "OFF";
    case ST_TYPING:   return "TYP";
  }
  return "?";
}

// ---- typing ----
static void typeString(const char *s) {
  for (const char *p = s; *p; ++p) {
    Keyboard.write((uint8_t)*p);
    delay(randInRange(KEY_MIN_MS, KEY_MAX_MS));  // human-ish cadence
  }
  if (PRESS_ENTER_AFTER) Keyboard.write('\n');
}

// ---- BOOT button: short press = arm/disarm, long press = panic disarm ----
static void pollButton() {
  static bool     prev = HIGH;
  static uint32_t downAt = 0;
  static bool     longHandled = false;

  bool now = digitalRead(PIN_BTN_BOOT);
  if (prev == HIGH && now == LOW) {                 // press begins
    downAt = millis();
    longHandled = false;
  } else if (prev == LOW && now == LOW) {           // held
    if (!longHandled && (millis() - downAt) >= LONGPRESS_MS) {
      armed = false;                                // panic
      longHandled = true;
    }
  } else if (prev == LOW && now == HIGH) {          // released
    if (!longHandled && (millis() - downAt) >= 40) {
      armed = !armed;                               // toggle
      if (armed) scheduleNext();                    // re-arm -> fresh interval
    }
  }
  prev = now;
}

// ---- LCD (240x135) ----
static void drawTitle() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(4, 4);
  tft.print("GhostScribe");
}

// Auto-sized, centered current/last phrase in the middle band (y 30..72).
static void drawPhrase(const String &phrase) {
  const int bandY = 30, bandH = 42;
  tft.fillRect(0, bandY, SCR_W, bandH, TFT_BLACK);
  String s = phrase;
  if (s.length() > 22) s = s.substring(0, 21) + "~";
  int size = 3;
  tft.setTextSize(size);
  while (size > 1 && (int)tft.textWidth(s) > SCR_W - 8) { tft.setTextSize(--size); }
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  int w = (int)tft.textWidth(s);
  int h = 8 * size;
  int x = (SCR_W - w) / 2; if (x < 0) x = 0;
  tft.setCursor(x, bandY + (bandH - h) / 2);
  tft.print(s);
}

static void drawDynamic(State st, bool force) {
  static State    lastSt   = (State)-1;
  static int      lastSecs = -1;
  static uint32_t lastCnt  = 0xFFFFFFFF;

  int secs = (int)((int32_t)(nextFireAt - millis()) / 1000);
  if (secs < 0) secs = 0;

  if (force || st != lastSt) {                       // state chip (top-right)
    uint16_t c = stateColor(st);
    tft.fillRoundRect(SCR_W - 50, 2, 46, 18, 3, c);
    tft.setTextColor(TFT_BLACK, c);
    tft.setTextSize(2);
    tft.setCursor(SCR_W - 44, 4);
    tft.print(stateName(st));
    lastSt = st;
  }

  if (force || secs != lastSecs) {                   // countdown + progress bar
    tft.fillRect(4, 80, 150, 16, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(4, 80);
    tft.printf("Next %02d:%02d", secs / 60, secs % 60);

    uint32_t span = nextFireAt - intervalStart;
    uint32_t done = millis() - intervalStart;
    int filled = span ? (int)((uint64_t)done * (SCR_W - 8) / span) : 0;
    if (filled < 0) filled = 0;
    if (filled > SCR_W - 8) filled = SCR_W - 8;
    tft.fillRect(4, 100, filled, 8, TFT_GREEN);
    tft.fillRect(4 + filled, 100, (SCR_W - 8) - filled, 8, TFT_NAVY);
    lastSecs = secs;
  }

  if (force || typedCount != lastCnt) {              // counters
    tft.fillRect(4, 114, SCR_W - 8, 8, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(4, 114);
    tft.printf("Typed: %lu   Pool: %u",
               (unsigned long)typedCount, (unsigned)PHRASE_COUNT);
    lastCnt = typedCount;
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);          // footer
  tft.setTextSize(1);
  tft.setCursor(4, 126);
  tft.print(armed ? "BOOT=off  hold=panic " : "BOOT=arm            ");
}

void setup() {
  // 1) display (LovyanGFX drives the backlight via Light_PWM on GPIO7)
  tft.init();
  tft.setRotation(1);            // landscape 240x135
  tft.setBrightness(255);
  drawTitle();

  // 2) USB-HID keyboard + CDC console (composite, because CDC_ON_BOOT=1)
  Serial.begin(115200);
  Keyboard.begin();
  USB.begin();

  // 3) BOOT button
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

  // 4) boot announcement: wait for enumeration, type BOOT_MESSAGE, show it
  scheduleNext();
  drawPhrase("...");
  drawDynamic(ST_ARMED, true);
  delay(SETTLE_MS);
  drawPhrase(BOOT_MESSAGE);
  drawDynamic(ST_TYPING, true);
  typeString(BOOT_MESSAGE);
  lastPhrase = BOOT_MESSAGE;
  Serial.printf("typed (boot): %s\n", BOOT_MESSAGE);
  scheduleNext();
}

void loop() {
  pollButton();

  if (armed && (int32_t)(millis() - nextFireAt) >= 0) {
    const char *phrase = PHRASES[esp_random() % PHRASE_COUNT];
    lastPhrase = phrase;
    drawPhrase(lastPhrase);
    drawDynamic(ST_TYPING, true);
    typeString(phrase);
    typedCount++;
    Serial.printf("typed: %s\n", phrase);
    scheduleNext();
  }

  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 200) {        // ~5 Hz UI refresh
    drawDynamic(armed ? ST_ARMED : ST_DISARMED, false);
    lastTick = millis();
  }
}
