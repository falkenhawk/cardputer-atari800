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
  CHECK(keymap_default('~',  &m0) == -1, "'~' unmapped -> AKEY_NONE (tilde; Atari has no direct AKEY)");

  /* Space: Cardputer puts it in keysState().word[] as 0x20 (same as via .space flag).
     Both paths must resolve to AKEY_SPACE. */
  CHECK(keymap_default(' ',            &m0) == 0x21, "Space via ' ' (from word[]) -> AKEY_SPACE");
  CHECK(keymap_default(KM_KEY_SPACE,   &m0) == 0x21, "Space via KM_KEY_SPACE sentinel -> AKEY_SPACE");

  /* Shifted punctuation — M5Cardputer sends the shifted glyph in word[];
     must map back to the AKEY for that specific glyph. These AKEYs have
     SHIFT baked into them directly per akey.h, so the returned value should
     match akey.h's AKEY_<glyph> define exactly — even when the modifier
     struct reports shift=1. */
  km_modifiers_t mSh = { .ctrl = 0, .shift = 1, .fn = 0 };
  CHECK(keymap_default('!', &mSh) == 0x5f, "'!' via word[] -> AKEY_EXCLAMATION");
  CHECK(keymap_default('+', &mSh) == 0x06, "'+' via word[] -> AKEY_PLUS");
  CHECK(keymap_default('?', &mSh) == 0x66, "'?' via word[] -> AKEY_QUESTION");
  CHECK(keymap_default('@', &mSh) == 0x75, "'@' via word[] -> AKEY_AT");
  CHECK(keymap_default('*', &mSh) == 0x07, "'*' via word[] -> AKEY_ASTERISK");
  CHECK(keymap_default('<', &mSh) == 0x36, "'<' via word[] -> AKEY_LESS");
  CHECK(keymap_default('>', &mSh) == 0x37, "'>' via word[] -> AKEY_GREATER");
  CHECK(keymap_default(':', &mSh) == 0x42, "':' via word[] -> AKEY_COLON");
  CHECK(keymap_default('"', &mSh) == 0x5e, "'\"' via word[] -> AKEY_DBLQUOTE");
  CHECK(keymap_default('|', &mSh) == 0x4f, "'|' via word[] -> AKEY_BAR");
  /* '{' / '}' have no direct Atari AKEY — AKEY_BRACKETLEFT is 0x60 which
     already has bit 0x40 set, so SHFT|BRACKETLEFT == BRACKETLEFT. We leave
     them unmapped (AKEY_NONE) rather than silently collapse to '[' / ']'. */
  CHECK(keymap_default('{', &mSh) == -1, "'{' via word[] -> AKEY_NONE (Atari has no brace)");
  CHECK(keymap_default('}', &mSh) == -1, "'}' via word[] -> AKEY_NONE");
  CHECK(keymap_default('_', &mSh) == 0x4e, "'_' via word[] -> AKEY_UNDERSCORE");

  /* Unshifted single-quote — AKEY_QUOTE. */
  CHECK(keymap_default('\'', &m0) == 0x73, "'\\'' (unshifted) -> AKEY_QUOTE");

  /* ---- Fn layer ---- */
  km_modifiers_t mF = { .ctrl = 0, .shift = 0, .fn = 1 };
  km_out_t out;

  /* Fn+1 → Option (via INPUT_key_consol mask).
     INPUT_CONSOL_OPTION = 0x04; spec says clear the bit to press.
     Our encoding: value = the bit mask that should be CLEARED. */
  out = keymap_fn('1', &mF);
  CHECK(out.kind == KM_OUT_CONSOL && out.value == 0x04, "Fn+1 -> CONSOL OPTION");
  out = keymap_fn('2', &mF);
  CHECK(out.kind == KM_OUT_CONSOL && out.value == 0x02, "Fn+2 -> CONSOL SELECT");
  out = keymap_fn('3', &mF);
  CHECK(out.kind == KM_OUT_CONSOL && out.value == 0x01, "Fn+3 -> CONSOL START");

  /* Fn+4 / Fn+5 — firmware actions (warm/cold reset) */
  out = keymap_fn('4', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_WARM_RESET, "Fn+4 -> WARM_RESET");
  out = keymap_fn('5', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_COLD_RESET, "Fn+5 -> COLD_RESET");

  /* Fn+6 — Help (AKEY_HELP = 0x11) */
  out = keymap_fn('6', &mF);
  CHECK(out.kind == KM_OUT_AKEY && out.value == 0x11, "Fn+6 -> AKEY_HELP");

  /* Fn+7 / Fn+8 — Break, Menu (actions) */
  out = keymap_fn('7', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_BREAK, "Fn+7 -> BREAK");
  out = keymap_fn('8', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_MENU_OPEN, "Fn+8 -> MENU_OPEN");

  /* Fn+9 / Fn+0 — save/load (M5 stubs; still routed) */
  out = keymap_fn('9', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_SAVE_STATE, "Fn+9 -> SAVE_STATE");
  out = keymap_fn('0', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_LOAD_STATE, "Fn+0 -> LOAD_STATE");

  /* Fn + ; . , / — cursor */
  out = keymap_fn(';', &mF);
  CHECK(out.kind == KM_OUT_AKEY && out.value == 0x8e, "Fn+; -> AKEY_UP");
  out = keymap_fn('.', &mF);
  CHECK(out.kind == KM_OUT_AKEY && out.value == 0x8f, "Fn+. -> AKEY_DOWN");
  out = keymap_fn(',', &mF);
  CHECK(out.kind == KM_OUT_AKEY && out.value == 0x86, "Fn+, -> AKEY_LEFT");
  out = keymap_fn('/', &mF);
  CHECK(out.kind == KM_OUT_AKEY && out.value == 0x87, "Fn+/ -> AKEY_RIGHT");

  /* Fn+[ / Fn+] → brightness; Fn+- / Fn+= → volume */
  out = keymap_fn('[', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_BRIGHTNESS_DOWN, "Fn+[ -> brightness-");
  out = keymap_fn(']', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_BRIGHTNESS_UP,   "Fn+] -> brightness+");
  out = keymap_fn('-', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_VOLUME_DOWN,     "Fn+- -> volume-");
  out = keymap_fn('=', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_VOLUME_UP,       "Fn+= -> volume+");

  /* Fn+\\ → display mode cycle */
  out = keymap_fn('\\', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_DISPLAY_MODE_CYCLE, "Fn+\\ -> mode cycle");

  /* Fn+i → inverse; Fn+j → toggle input */
  out = keymap_fn('i', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_INVERSE_VIDEO, "Fn+i -> inverse");
  out = keymap_fn('j', &mF);
  CHECK(out.kind == KM_OUT_ACTION && out.value == KM_ACT_TOGGLE_INPUT_MODE, "Fn+j -> toggle input");

  /* Unmapped Fn+key -> NONE */
  out = keymap_fn('x', &mF);
  CHECK(out.kind == KM_OUT_NONE, "Fn+x unmapped -> NONE");

  if (fail) return EXIT_FAILURE;
  printf("PASS: keymap default\n");
  return EXIT_SUCCESS;
}
