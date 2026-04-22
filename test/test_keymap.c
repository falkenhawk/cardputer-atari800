/* test_keymap.c — default layer: Cardputer key → AKEY_* code.
   Modifiers (Ctrl, Shift) OR'd into the base code per atari800/akey.h. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../src/input/keymap.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  km_modifiers_t m0 = { .ctrl = 0, .shift = 0, .fn = 0 };
  km_modifiers_t mS = { .ctrl = 0, .shift = 1, .fn = 0 };
  km_modifiers_t mC = { .ctrl = 1, .shift = 0, .fn = 0 };

  /* lowercase letters: raw AKEY_a..z */
  CHECK(keymap_default('a', &m0) == 0x3f, "'a' -> AKEY_a (0x3f)");
  CHECK(keymap_default('z', &m0) == 0x17, "'z' -> AKEY_z (0x17)");

  /* uppercase via SHIFT bit */
  CHECK(keymap_default('a', &mS) == (0x40 | 0x3f), "SHIFT+'a' -> AKEY_A");

  /* CTRL+letter via CTRL bit */
  CHECK(keymap_default('a', &mC) == (0x80 | 0x3f), "CTRL+'a' -> AKEY_CTRL_a");

  /* digits */
  CHECK(keymap_default('0', &m0) == 0x32, "'0' -> AKEY_0");
  CHECK(keymap_default('1', &m0) == 0x1f, "'1' -> AKEY_1");
  CHECK(keymap_default('9', &m0) == 0x30, "'9' -> AKEY_9");

  /* punctuation (non-Fn) */
  CHECK(keymap_default(';', &m0) == 0x02, "';' -> AKEY_SEMICOLON");
  CHECK(keymap_default(',', &m0) == 0x20, "',' -> AKEY_COMMA");
  CHECK(keymap_default('.', &m0) == 0x22, "'.' -> AKEY_FULLSTOP");
  CHECK(keymap_default('/', &m0) == 0x26, "'/' -> AKEY_SLASH");
  CHECK(keymap_default('\\', &m0) == 0x46, "'\\' -> AKEY_BACKSLASH");

  /* named keys — passed as negative ASCII-ish sentinels by the caller */
  CHECK(keymap_default(KM_KEY_RETURN, &m0) == 0x0c, "Return -> AKEY_RETURN");
  CHECK(keymap_default(KM_KEY_SPACE,  &m0) == 0x21, "Space -> AKEY_SPACE");
  CHECK(keymap_default(KM_KEY_ESCAPE, &m0) == 0x1c, "Esc -> AKEY_ESCAPE");
  CHECK(keymap_default(KM_KEY_TAB,    &m0) == 0x2c, "Tab -> AKEY_TAB");
  CHECK(keymap_default(KM_KEY_BACKSP, &m0) == 0x34, "Backspace -> AKEY_BACKSPACE");

  /* unknown → -1 (AKEY_NONE) */
  CHECK(keymap_default(0x00, &m0) == -1, "0x00 -> AKEY_NONE");
  CHECK(keymap_default('~',  &m0) == -1, "'~' unmapped -> AKEY_NONE");

  if (fail) return EXIT_FAILURE;
  printf("PASS: keymap default\n");
  return EXIT_SUCCESS;
}
