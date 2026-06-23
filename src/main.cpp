// GhostScribe — Waveshare ESP32-S3-GEEK USB-HID typer with onboard LCD.
//
// Two phases after boot:
//   Phase 1 (first PHASE1_DURATION_MS, default 2h): emits short "glitch" tokens
//     (and, on Linux, occasional Unicode emoji) in 1-3 token bursts — feels buggy.
//   Phase 2 (after that): types a random phrase from PHRASES[] (the konj theme).
// The next emission is pre-chosen so the LCD can show both the current (just typed)
// and next (upcoming) word. BOOT button arms/disarms; long-press = panic disarm.

#include "config.h"
#include "display.h"
#include "typer.h"
#include "scheduler.h"

// Emit the pre-planned next emission, then plan the following one.
static void emit() {
  drawDynamic(ST_TYPING, true, armed, nextFireAt, intervalStart, typedCount, phase2());
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
  showCurrentNext(currentText, nextText);
  scheduleNext();
}

void setup() {
  tft.init(); tft.setRotation(1); tft.setBrightness(255); drawHeader();

  Serial.begin(115200);
  Keyboard.begin(); USB.begin();
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

  planNext();
  scheduleNext();
  currentText = "..."; showCurrentNext(currentText, nextText); drawDynamic(ST_ARMED, true, armed, nextFireAt, intervalStart, typedCount, phase2());

  delay(SETTLE_MS);
  drawDynamic(ST_TYPING, true, armed, nextFireAt, intervalStart, typedCount, phase2());
  typeAscii(BOOT_MESSAGE);
  currentText = BOOT_MESSAGE; Serial.printf("typed (boot): %s\n", BOOT_MESSAGE);
  showCurrentNext(currentText, nextText);
  scheduleNext();
}

void loop() {
  pollButton();
  if (armed && (int32_t)(millis() - nextFireAt) >= 0) emit();

  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 200) { drawDynamic(armed ? ST_ARMED : ST_DISARMED, false, armed, nextFireAt, intervalStart, typedCount, phase2()); lastTick = millis(); }
}
