// GhostScribe — display instance and LCD drawing routines.
#include "config.h"
#include "display.h"

LGFX tft;

// ---- Header (drawn once at boot) ----
void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(4, 2); tft.print("GhostScribe");
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(4, 22); tft.print("current:");
  tft.setCursor(4, 56); tft.print("next:");
}

// One auto-sized word line in a 20px band.
void drawWord(int bandY, const String &word, uint16_t color) {
  tft.fillRect(0, bandY, SCR_W, 20, TFT_BLACK);
  String s = word; if (s.length() > 26) s = s.substring(0, 25) + "~";
  int size = 2; tft.setTextSize(size);
  while (size > 1 && (int)tft.textWidth(s) > SCR_W - 8) tft.setTextSize(--size);
  tft.setTextColor(color, TFT_BLACK);
  int w = (int)tft.textWidth(s), h = 8 * size, x = (SCR_W - w) / 2; if (x < 0) x = 0;
  tft.setCursor(x, bandY + (20 - h) / 2); tft.print(s);
}

static uint16_t stateColor(State s) {
  switch (s) { case ST_ARMED: return TFT_GREEN; case ST_DISARMED: return TFT_ORANGE;
               case ST_TYPING: return TFT_CYAN; } return TFT_WHITE;
}
static const char *stateName(State s) {
  switch (s) { case ST_ARMED: return "ARM"; case ST_DISARMED: return "OFF";
               case ST_TYPING: return "TYP"; } return "?";
}

// Format uptime seconds into the most readable unit string.
// < 60s  -> "23s"       < 1h   -> "47m"         < 1d   -> "2h 15m"
// >= 1d  -> "3d 2h"
static void fmtUptime(char *buf, size_t len, uint32_t secs) {
  if (secs < 60)
    snprintf(buf, len, "%lus", (unsigned long)secs);
  else if (secs < 3600)
    snprintf(buf, len, "%lum", (unsigned long)(secs / 60));
  else if (secs < 86400)
    snprintf(buf, len, "%luh %lum", (unsigned long)(secs / 3600), (unsigned long)((secs / 60) % 60));
  else
    snprintf(buf, len, "%lud %luh", (unsigned long)(secs / 86400), (unsigned long)((secs / 3600) % 24));
}

void drawDynamic(State st, bool force, bool armed,
                 uint32_t nextFireAt, uint32_t intervalStart,
                 uint32_t typedCount, bool phase2) {
  static State lastSt = (State)-1; static int lastSecs = -1; static uint32_t lastCnt = 0xFFFFFFFF;
  static uint32_t lastUptimeSecs = 0xFFFFFFFF;
  int secs = (int)((int32_t)(nextFireAt - millis()) / 1000); if (secs < 0) secs = 0;
  uint32_t upSecs = millis() / 1000;

  if (force || st != lastSt) {
    uint16_t c = stateColor(st);
    tft.fillRoundRect(SCR_W - 50, 2, 46, 16, 3, c);
    tft.setTextColor(TFT_BLACK, c); tft.setTextSize(2);
    tft.setCursor(SCR_W - 44, 3); tft.print(stateName(st)); lastSt = st;
  }
  if (force || secs != lastSecs) {
    tft.fillRect(4, 88, 170, 8, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(4, 88); tft.printf("Next %02d:%02d  %s", secs / 60, secs % 60, phase2 ? "P2" : "P1");
    uint32_t span = nextFireAt - intervalStart, done = millis() - intervalStart;
    int filled = span ? (int)((uint64_t)done * (SCR_W - 8) / span) : 0;
    if (filled < 0) filled = 0; if (filled > SCR_W - 8) filled = SCR_W - 8;
    tft.fillRect(4, 100, filled, 6, TFT_GREEN);
    tft.fillRect(4 + filled, 100, (SCR_W - 8) - filled, 6, TFT_NAVY);
    lastSecs = secs;
  }
  if (force || upSecs != lastUptimeSecs) {
    char up[16]; fmtUptime(up, sizeof(up), upSecs);
    tft.fillRect(4, 110, SCR_W / 2 - 4, 8, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(4, 110); tft.printf("Up: %s", up);
    lastUptimeSecs = upSecs;
  }
  if (force || typedCount != lastCnt) {
    tft.fillRect(SCR_W / 2 + 4, 110, SCR_W / 2 - 4, 8, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(SCR_W / 2 + 4, 110); tft.printf("Typed: %lu", (unsigned long)typedCount);
    lastCnt = typedCount;
  }
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(4, 122); tft.print(armed ? "BOOT=off  hold=panic " : "BOOT=arm            ");
}

void showCurrentNext(const String &currentText, const String &nextText) {
  drawWord(30, currentText, TFT_CYAN);
  drawWord(64, nextText, TFT_YELLOW);
}
