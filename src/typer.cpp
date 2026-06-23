// GhostScribe — USB-HID keyboard typing implementation.
#include "config.h"
#include "typer.h"

USBHIDKeyboard Keyboard;

void typeAscii(const char *s) {
  for (const char *p = s; *p; ++p) {
    Keyboard.write((uint8_t)*p);
    delay(randInRange(KEY_MIN_MS, KEY_MAX_MS));
  }
}

// Linux IBus/GTK Unicode entry: Ctrl+Shift+U, hex codepoint, then space to confirm.
void typeEmojiLinux(uint32_t cp) {
  Keyboard.press(KEY_LEFT_CTRL); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('u');
  delay(40); Keyboard.releaseAll(); delay(40);
  char hex[9]; snprintf(hex, sizeof(hex), "%x", (unsigned)cp);
  for (char *p = hex; *p; ++p) { Keyboard.write((uint8_t)*p); delay(30); }
  Keyboard.write(' ');
}
