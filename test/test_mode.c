#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/input/mode.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Defaults: keyboard mode */
  mode_reset();
  CHECK(mode_current() == MODE_KEYBOARD, "default -> keyboard");

  /* Auto-detect from extension */
  mode_autodetect_for("/atari800/test.xex");
  CHECK(mode_current() == MODE_JOYSTICK, ".xex -> joystick");

  mode_autodetect_for("/atari800/disk.atr");
  CHECK(mode_current() == MODE_KEYBOARD, ".atr -> keyboard");

  mode_autodetect_for("/atari800/demo.CAR");
  CHECK(mode_current() == MODE_JOYSTICK, ".CAR (uppercase) -> joystick");

  mode_autodetect_for("/atari800/prog.bas");
  CHECK(mode_current() == MODE_KEYBOARD, ".bas -> keyboard");

  mode_autodetect_for("/atari800/tape.cas");
  CHECK(mode_current() == MODE_KEYBOARD, ".cas -> keyboard");

  /* Unknown extension leaves mode unchanged */
  mode_reset();  /* keyboard */
  mode_autodetect_for("/atari800/weird.zzz");
  CHECK(mode_current() == MODE_KEYBOARD, "unknown ext leaves mode");

  /* Manual toggle (Fn+J) */
  mode_toggle();
  CHECK(mode_current() == MODE_JOYSTICK, "toggle keyboard -> joystick");
  mode_toggle();
  CHECK(mode_current() == MODE_KEYBOARD, "toggle joystick -> keyboard");

  if (fail) return EXIT_FAILURE;
  printf("PASS: mode\n");
  return EXIT_SUCCESS;
}
