// GhostScribe — compile-time tunables, data pools, and shared constants.
#pragma once
#include <Arduino.h>
#include "esp_random.h"

#if ARDUINO_USB_MODE == 1
#error "Set -D ARDUINO_USB_MODE=0 (USB-OTG/TinyUSB) in platformio.ini — HID is impossible in Hardware-CDC mode."
#endif

// ---- Pin & screen ----
static const int PIN_BTN_BOOT = 0;
static const int SCR_W = 240, SCR_H = 135;

// ---- State enum ----
enum State { ST_ARMED, ST_DISARMED, ST_TYPING };

// ---- Boot message ----
static const char *BOOT_MESSAGE = ":)";

// ---- Phase 1: short "glitch" tokens (ASCII; type directly) ----
static const char *GLITCH[] = {
    ";", ":", ":-)", "...", "-", "/", "!!", "@3$", ":P", "xD", ";)", "^_^",
    "??", "**", ":/", "<3", "8)", ">:(", "o_O", ":|", "...?", "#@!", "~",
};
static const size_t GLITCH_COUNT = sizeof(GLITCH) / sizeof(GLITCH[0]);

// ---- Phase 1: emoji (Linux Ctrl+Shift+U Unicode entry); label shown on LCD ----
static const bool USE_LINUX_EMOJI = true;
static const int  EMOJI_PERCENT   = 25;    // chance a burst item is an emoji
struct Emoji { uint32_t cp; const char *label; };
static const Emoji EMOJI[] = {
    {0x1F434, "[horse]"}, {0x1F40E, "[horse]"}, {0x1F451, "[crown]"},
    {0x1F602, "[lol]"},   {0x1F921, "[clown]"},
};
static const size_t EMOJI_COUNT = sizeof(EMOJI) / sizeof(EMOJI[0]);

// ---- Phase 2: the konj / Dubravko / Duco theme (ASCII, US layout) ----
static const char *PHRASES[] = {
    "dubravko kralj", "duco konj", "konjina sam ja", "ja sam konj",
    "dubravko konj", "duco najbolji", "duco kralj", "dubravko najbolji",
    "konj je kralj", "ja sam konjina", "duco car", "dubravko car",
    "najbolji konj", "konj duco", "kralj dubravko", "iju konju",
};
static const size_t PHRASE_COUNT = sizeof(PHRASES) / sizeof(PHRASES[0]);

// ---- Timing ----
static const uint32_t PHASE1_DURATION_MS = 2UL * 60UL * 60UL * 1000UL;  // 2 hours
static const uint32_t P1_MIN_MS = 20UL * 1000UL;     // phase 1: frequent small bursts
static const uint32_t P1_MAX_MS = 120UL * 1000UL;
static const uint32_t P2_MIN_MS = 30UL * 1000UL;     // phase 2: full phrases
static const uint32_t P2_MAX_MS = 300UL * 1000UL;
static const uint32_t SETTLE_MS  = 2500;              // USB enumeration settle
static const uint16_t KEY_MIN_MS = 40, KEY_MAX_MS = 120;  // per-character jitter
static const bool     PRESS_ENTER_AFTER = false;      // Enter after each phrase (phase 2)?
static const uint32_t LONGPRESS_MS = 1200;            // BOOT long-press = panic disarm
static const uint32_t TRIPLE_CLICK_MS = 600;          // 3 clicks within this window = dim toggle

// ---- Utility ----
static inline uint32_t randInRange(uint32_t lo, uint32_t hi) {
  return lo + (esp_random() % (hi - lo + 1));
}
