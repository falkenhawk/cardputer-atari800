#include "mode.h"
#include <string.h>
#include <ctype.h>

static input_mode_t current = MODE_KEYBOARD;

input_mode_t mode_current(void) { return current; }

void mode_reset(void) { current = MODE_KEYBOARD; }

void mode_toggle(void) {
  current = (current == MODE_KEYBOARD) ? MODE_JOYSTICK : MODE_KEYBOARD;
}

static int ext_eq(const char* a, const char* b) {
  /* Case-insensitive compare; a is user input, b is our lowercase literal. */
  while (*a && *b) {
    if (tolower((unsigned char)*a) != *b) return 0;
    a++; b++;
  }
  return *a == 0 && *b == 0;
}

void mode_autodetect_for(const char* filename) {
  if (!filename) return;
  const char* dot = strrchr(filename, '.');
  if (!dot) return;
  dot++;

  if (ext_eq(dot, "xex") || ext_eq(dot, "car") || ext_eq(dot, "rom")) {
    current = MODE_JOYSTICK;
  } else if (ext_eq(dot, "atr") || ext_eq(dot, "bas") || ext_eq(dot, "cas") ||
             ext_eq(dot, "exe")) {
    /* .exe is ambiguous (some distros use it for DOS-like xex) but Cardputer
       context treats it as keyboard for BASIC-like runtime environments.
       If a user hits this edge case, Fn+J toggles. */
    current = MODE_KEYBOARD;
  }
  /* else: unknown extension, leave as-is */
}
