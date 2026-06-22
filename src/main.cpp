// GhostScribe — Waveshare ESP32-S3-GEEK USB-HID typer with onboard LCD.
//
// Two phases after boot:
//   Phase 1 (first PHASE1_DURATION_MS, default 2h): emits short "glitch" tokens
//     (and, on Linux, occasional Unicode emoji) in 1-3 token bursts — feels buggy.
//   Phase 2 (after that): types a random phrase from PHRASES[] (the konj theme).
// The next emission is pre-chosen so the LCD can show both the current (just typed)
// and next (upcoming) word. BOOT button arms/disarms; long-press = panic disarm.
//
// USB HID requires USB-OTG / TinyUSB  ->  platformio.ini sets -D ARDUINO_USB_MODE=0.
// A HID keyboard sends keycodes, not UTF-8: ASCII types directly; real emoji are
// entered via the Linux IBus/GTK trick (Ctrl+Shift+U + hex), only in supporting
// apps. Set USE_LINUX_EMOJI=false to disable.

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
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;
public:
  LGFX(void) {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST; c.spi_mode = 0;
      c.freq_write = 40000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 12; c.pin_mosi = 11; c.pin_miso = -1; c.pin_dc = 8;
      _bus.config(c); _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs = 10; c.pin_rst = 9; c.pin_busy = -1;
      c.panel_width = 135; c.panel_height = 240;
      c.offset_x = 52; c.offset_y = 40; c.offset_rotation = 0;
      c.readable = false; c.invert = true; c.rgb_order = false;
      c.dlen_16bit = false; c.bus_shared = false;
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
static const char *BOOT_MESSAGE = "UKLJUCIO SAM SE";   // typed once on boot

// --- Phase 1: short "glitch" tokens (ASCII; type directly) ---
static const char *GLITCH[] = {
    ";", ":", ":-)", "...", "-", "/", "!!", "@3$", ":P", "xD", ";)", "^_^",
    "??", "**", ":/", "<3", "8)", ">:(", "o_O", ":|", "...?", "#@!", "~",
};
static const size_t GLITCH_COUNT = sizeof(GLITCH) / sizeof(GLITCH[0]);

// --- Phase 1: emoji (Linux Ctrl+Shift+U Unicode entry); label is shown on the LCD ---
static const bool USE_LINUX_EMOJI = true;   // false -> ASCII only (works everywhere)
static const int  EMOJI_PERCENT   = 25;     // chance a burst item is an emoji
struct Emoji { uint32_t cp; const char *label; };
static const Emoji EMOJI[] = {
    {0x1F434, "[horse]"}, {0x1F40E, "[horse]"}, {0x1F451, "[crown]"},
    {0x1F602, "[lol]"},   {0x1F921, "[clown]"},
};
static const size_t EMOJI_COUNT = sizeof(EMOJI) / sizeof(EMOJI[0]);

// --- Phase 2: the konj / Dubravko / Duco theme (ASCII, US layout) ---
static const char *PHRASES[] = {
    "dubravko kralj", "duco konj", "konjina sam ja", "ja sam konj",
    "dubravko konj", "duco najbolji", "duco kralj", "dubravko najbolji",
    "konj je kralj", "ja sam konjina", "duco car", "dubravko car",
    "najbolji konj", "konj duco", "kralj dubravko", "iju konju",
};
static const size_t PHRASE_COUNT = sizeof(PHRASES) / sizeof(PHRASES[0]);

static const uint32_t PHASE1_DURATION_MS = 2UL * 60UL * 60UL * 1000UL;  // 2 hours

static const uint32_t P1_MIN_MS = 20UL * 1000UL;    // phase 1: frequent small bursts
static const uint32_t P1_MAX_MS = 120UL * 1000UL;
static const uint32_t P2_MIN_MS = 30UL * 1000UL;    // phase 2: full phrases
static const uint32_t P2_MAX_MS = 300UL * 1000UL;

static const uint32_t SETTLE_MS  = 2500;            // wait for USB enumeration before first keystroke
static const uint16_t KEY_MIN_MS = 40, KEY_MAX_MS = 120;  // per-character jitter
static const bool     PRESS_ENTER_AFTER = false;    // Enter after each phrase (phase 2)?
static const uint32_t LONGPRESS_MS = 1200;          // BOOT long-press = panic disarm
// =================================================================

static const int PIN_BTN_BOOT = 0;
static const int SCR_W = 240, SCR_H = 135;

LGFX           tft;
USBHIDKeyboard Keyboard;

enum State { ST_ARMED, ST_DISARMED, ST_TYPING };

static bool     armed         = true;
static uint32_t intervalStart = 0;
static uint32_t nextFireAt    = 0;
static uint32_t typedCount    = 0;
static String   currentText   = "-";

// Pre-planned next emission (so the LCD can preview it).
struct Token { bool isEmoji; uint32_t cp; const char *text; };
static Token   nextTokens[3];
static int     nextTokenCount = 0;
static String  nextText       = "";

static bool phase2() { return millis() >= PHASE1_DURATION_MS; }

static uint32_t randInRange(uint32_t lo, uint32_t hi) {
  return lo + (esp_random() % (hi - lo + 1));
}

static void scheduleNext() {
  uint32_t lo = phase2() ? P2_MIN_MS : P1_MIN_MS;
  uint32_t hi = phase2() ? P2_MAX_MS : P1_MAX_MS;
  intervalStart = millis();
  nextFireAt = intervalStart + randInRange(lo, hi);
}

// Decide (and remember) what the next emission will be.
static void planNext() {
  if (phase2()) {
    const char *ph = PHRASES[esp_random() % PHRASE_COUNT];
    nextTokens[0] = {false, 0, ph};
    nextTokenCount = 1;
    nextText = ph;
  } else {
    int burst = 1 + (int)(esp_random() % 3);   // 1..3 small tokens
    nextTokenCount = burst;
    nextText = "";
    for (int i = 0; i < burst; i++) {
      if (USE_LINUX_EMOJI && (int)(esp_random() % 100) < EMOJI_PERCENT) {
        const Emoji &e = EMOJI[esp_random() % EMOJI_COUNT];
        nextTokens[i] = {true, e.cp, e.label};
        nextText += e.label;
      } else {
        const char *g = GLITCH[esp_random() % GLITCH_COUNT];
        nextTokens[i] = {false, 0, g};
        nextText += g;
      }
    }
  }
}

static uint16_t stateColor(State s) {
  switch (s) { case ST_ARMED: return TFT_GREEN; case ST_DISARMED: return TFT_ORANGE;
               case ST_TYPING: return TFT_CYAN; } return TFT_WHITE;
}
static const char *stateName(State s) {
  switch (s) { case ST_ARMED: return "ARM"; case ST_DISARMED: return "OFF";
               case ST_TYPING: return "TYP"; } return "?";
}

// ---- typing ----
static void typeAscii(const char *s) {
  for (const char *p = s; *p; ++p) { Keyboard.write((uint8_t)*p); delay(randInRange(KEY_MIN_MS, KEY_MAX_MS)); }
}
// Linux IBus/GTK Unicode entry: Ctrl+Shift+U, hex codepoint, then space to confirm.
static void typeEmojiLinux(uint32_t cp) {
  Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('u');
  delay(40); Keyboard.releaseAll(); delay(40);
  char hex[9]; snprintf(hex, sizeof(hex), "%x", (unsigned)cp);
  for (char *p = hex; *p; ++p) { Keyboard.write((uint8_t)*p); delay(30); }
  Keyboard.write(' ');
}

// ---- BOOT button: short press = arm/disarm, long press = panic disarm ----
static void pollButton() {
  static bool prev = HIGH; static uint32_t downAt = 0; static bool longHandled = false;
  bool now = digitalRead(PIN_BTN_BOOT);
  if (prev == HIGH && now == LOW) { downAt = millis(); longHandled = false; }
  else if (prev == LOW && now == LOW) {
    if (!longHandled && (millis() - downAt) >= LONGPRESS_MS) { armed = false; longHandled = true; }
  } else if (prev == LOW && now == HIGH) {
    if (!longHandled && (millis() - downAt) >= 40) { armed = !armed; if (armed) scheduleNext(); }
  }
  prev = now;
}

// ---- LCD (240x135) ----
static void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(4, 2); tft.print("GhostScribe");
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(4, 22); tft.print("current:");
  tft.setCursor(4, 56); tft.print("next:");
}

// One auto-sized word line in a 20px band.
static void drawWord(int bandY, const String &word, uint16_t color) {
  tft.fillRect(0, bandY, SCR_W, 20, TFT_BLACK);
  String s = word; if (s.length() > 26) s = s.substring(0, 25) + "~";
  int size = 2; tft.setTextSize(size);
  while (size > 1 && (int)tft.textWidth(s) > SCR_W - 8) tft.setTextSize(--size);
  tft.setTextColor(color, TFT_BLACK);
  int w = (int)tft.textWidth(s), h = 8 * size, x = (SCR_W - w) / 2; if (x < 0) x = 0;
  tft.setCursor(x, bandY + (20 - h) / 2); tft.print(s);
}

static void drawDynamic(State st, bool force) {
  static State lastSt = (State)-1; static int lastSecs = -1; static uint32_t lastCnt = 0xFFFFFFFF;
  int secs = (int)((int32_t)(nextFireAt - millis()) / 1000); if (secs < 0) secs = 0;

  if (force || st != lastSt) {
    uint16_t c = stateColor(st);
    tft.fillRoundRect(SCR_W - 50, 2, 46, 16, 3, c);
    tft.setTextColor(TFT_BLACK, c); tft.setTextSize(2);
    tft.setCursor(SCR_W - 44, 3); tft.print(stateName(st)); lastSt = st;
  }
  if (force || secs != lastSecs) {
    tft.fillRect(4, 88, 170, 8, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(4, 88); tft.printf("Next %02d:%02d  %s", secs / 60, secs % 60, phase2() ? "P2" : "P1");
    uint32_t span = nextFireAt - intervalStart, done = millis() - intervalStart;
    int filled = span ? (int)((uint64_t)done * (SCR_W - 8) / span) : 0;
    if (filled < 0) filled = 0; if (filled > SCR_W - 8) filled = SCR_W - 8;
    tft.fillRect(4, 100, filled, 6, TFT_GREEN);
    tft.fillRect(4 + filled, 100, (SCR_W - 8) - filled, 6, TFT_NAVY);
    lastSecs = secs;
  }
  if (force || typedCount != lastCnt) {
    tft.fillRect(4, 110, SCR_W - 8, 8, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(4, 110); tft.printf("Typed: %lu", (unsigned long)typedCount); lastCnt = typedCount;
  }
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(4, 122); tft.print(armed ? "BOOT=off  hold=panic " : "BOOT=arm            ");
}

static void showCurrentNext() {
  drawWord(30, currentText, TFT_CYAN);
  drawWord(64, nextText, TFT_YELLOW);
}

// Emit the pre-planned next emission, then plan the following one.
static void emit() {
  drawDynamic(ST_TYPING, true);
  for (int i = 0; i < nextTokenCount; i++) {
    if (nextTokens[i].isEmoji) typeEmojiLinux(nextTokens[i].cp);
    else                       typeAscii(nextTokens[i].text);
    if (nextTokenCount > 1) delay(randInRange(80, 220));
  }
  if (phase2() && PRESS_ENTER_AFTER) Keyboard.write('\n');

  currentText = nextText;
  Serial.printf("typed: %s\n", currentText.c_str());
  typedCount++;
  planNext();              // choose the new "next"
  showCurrentNext();
  scheduleNext();
}

void setup() {
  tft.init(); tft.setRotation(1); tft.setBrightness(255); drawHeader();

  Serial.begin(115200);
  Keyboard.begin(); USB.begin();
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

  planNext();
  scheduleNext();
  currentText = "..."; showCurrentNext(); drawDynamic(ST_ARMED, true);

  delay(SETTLE_MS);
  drawDynamic(ST_TYPING, true);
  typeAscii(BOOT_MESSAGE);
  currentText = BOOT_MESSAGE; Serial.printf("typed (boot): %s\n", BOOT_MESSAGE);
  showCurrentNext();
  scheduleNext();
}

void loop() {
  pollButton();
  if (armed && (int32_t)(millis() - nextFireAt) >= 0) emit();

  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 200) { drawDynamic(armed ? ST_ARMED : ST_DISARMED, false); lastTick = millis(); }
}
