// GhostScribe — USB-HID keyboard typing (ASCII + Linux emoji).
#pragma once
#include "USB.h"
#include "USBHIDKeyboard.h"

extern USBHIDKeyboard Keyboard;

void typeAscii(const char *s);
void typeEmojiLinux(uint32_t cp);
