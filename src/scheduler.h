// GhostScribe — scheduling state, plan/emit logic, BOOT button handling.
#pragma once
#include "config.h"

// Pre-planned next emission (so the LCD can preview it).
struct Token { bool isEmoji; uint32_t cp; const char *text; };

extern bool     armed;
extern uint32_t intervalStart;
extern uint32_t nextFireAt;
extern uint32_t typedCount;
extern String   currentText;
extern Token    nextTokens[3];
extern int      nextTokenCount;
extern String   nextText;

bool  phase2();
void  scheduleNext();
void  planNext();
void  pollButton();
