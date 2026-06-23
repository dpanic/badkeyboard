// GhostScribe — scheduling state, plan/emit logic, BOOT button handling.
#include "config.h"
#include "scheduler.h"
#include "typer.h"
#include "display.h"

// ---- Runtime state ----
bool     armed         = true;
uint32_t intervalStart = 0;
uint32_t nextFireAt    = 0;
uint32_t typedCount    = 0;
String   currentText   = "-";

Token   nextTokens[3];
int     nextTokenCount = 0;
String  nextText       = "";

bool phase2() { return millis() >= PHASE1_DURATION_MS; }

void scheduleNext() {
  uint32_t lo = phase2() ? P2_MIN_MS : P1_MIN_MS;
  uint32_t hi = phase2() ? P2_MAX_MS : P1_MAX_MS;
  intervalStart = millis();
  nextFireAt = intervalStart + randInRange(lo, hi);
}

// Decide (and remember) what the next emission will be.
void planNext() {
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

// ---- BOOT button: 1x = arm/disarm, 2x = dim, 3x = screen off/on, long = panic disarm ----
void pollButton() {
  static bool prev = HIGH; static uint32_t downAt = 0; static bool longHandled = false;
  static int clickCount = 0; static uint32_t firstClickAt = 0;
  bool now = digitalRead(PIN_BTN_BOOT);
  if (prev == HIGH && now == LOW) { downAt = millis(); longHandled = false; }
  else if (prev == LOW && now == LOW) {
    if (!longHandled && (millis() - downAt) >= LONGPRESS_MS) { armed = false; longHandled = true; }
  } else if (prev == LOW && now == HIGH) {
    if (!longHandled && (millis() - downAt) >= 40) {
      if (clickCount == 0 || (millis() - firstClickAt) > TRIPLE_CLICK_MS) firstClickAt = millis();
      clickCount++;
    }
  }
  // Expire click window — act on the count
  if (clickCount > 0 && (millis() - firstClickAt) > TRIPLE_CLICK_MS) {
    switch (clickCount) {
      case 1: armed = !armed; if (armed) scheduleNext(); break;
      case 2: toggleDim();   break;
      case 3: toggleScreen(); break;
    }
    clickCount = 0;
  }
  prev = now;
}
