/* mode.h — input mode state (Keyboard vs Joystick) with auto-detect.
   Module-level state; simpler than threading it through every function. */

#ifndef CARDPUTER_INPUT_MODE_H
#define CARDPUTER_INPUT_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MODE_KEYBOARD = 0,
  MODE_JOYSTICK = 1
} input_mode_t;

input_mode_t mode_current(void);

/* Reset to default (MODE_KEYBOARD). */
void mode_reset(void);

/* Flip the current mode. */
void mode_toggle(void);

/* Inspect extension of filename and set mode per spec 4.1:
   .xex / .car / .rom -> MODE_JOYSTICK
   .atr / .bas / .cas / .exe -> MODE_KEYBOARD
   unknown -> leave mode unchanged */
void mode_autodetect_for(const char* filename);

#ifdef __cplusplus
}
#endif

#endif
