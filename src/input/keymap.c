/* keymap.c — default + Fn layer mapping. Pure C.
   AKEY_* values copied from lib/atari800/src/akey.h so this module stays
   standalone on the host test build. */

#include "keymap.h"

/* --- AKEY_* constants (mirror lib/atari800/src/akey.h) --- */
#define AKEY_NONE        -1
#define AKEY_SHFT        0x40
#define AKEY_CTRL        0x80

#define AKEY_a           0x3f
#define AKEY_z           0x17

#define AKEY_0           0x32
#define AKEY_1           0x1f
#define AKEY_2           0x1e
#define AKEY_3           0x1a
#define AKEY_4           0x18
#define AKEY_5           0x1d
#define AKEY_6           0x1b
#define AKEY_7           0x33
#define AKEY_8           0x35
#define AKEY_9           0x30

#define AKEY_EQUAL       0x0f
#define AKEY_MINUS       0x0e
#define AKEY_SLASH       0x26
#define AKEY_COLON       0x42
#define AKEY_SEMICOLON   0x02
#define AKEY_COMMA       0x20
#define AKEY_FULLSTOP    0x22
#define AKEY_BRACKETLEFT 0x60
#define AKEY_BRACKETRIGHT 0x62
#define AKEY_BACKSLASH   0x46

#define AKEY_RETURN      0x0c
#define AKEY_SPACE       0x21
#define AKEY_ESCAPE      0x1c
#define AKEY_TAB         0x2c
#define AKEY_BACKSPACE   0x34

/* Small letter-to-AKEY table. akey.h numbers letters non-sequentially so we
   can't compute it; look up via a 26-entry table. */
static const int letter_akey[26] = {
  /* a-h */ 0x3f, 0x15, 0x12, 0x3a, 0x2a, 0x38, 0x3d, 0x39,
  /* i-p */ 0x0d, 0x01, 0x05, 0x00, 0x25, 0x23, 0x08, 0x0a,
  /* q-x */ 0x2f, 0x28, 0x3e, 0x2d, 0x0b, 0x10, 0x2e, 0x16,
  /* y-z */ 0x2b, 0x17
};

/* Punctuation → AKEY_* (unshifted only — Shift is applied via OR). */
static int punct_to_akey(int c) {
  switch (c) {
    case ';':  return AKEY_SEMICOLON;
    case ',':  return AKEY_COMMA;
    case '.':  return AKEY_FULLSTOP;
    case '/':  return AKEY_SLASH;
    case '\\': return AKEY_BACKSLASH;
    case '-':  return AKEY_MINUS;
    case '=':  return AKEY_EQUAL;
    case '[':  return AKEY_BRACKETLEFT;
    case ']':  return AKEY_BRACKETRIGHT;
    default:   return AKEY_NONE;
  }
}

int keymap_default(int key, const km_modifiers_t* mods) {
  int base = AKEY_NONE;

  if (key >= 'a' && key <= 'z') base = letter_akey[key - 'a'];
  else if (key >= 'A' && key <= 'Z') base = letter_akey[key - 'A']; /* shift applied below */
  else if (key >= '0' && key <= '9') {
    static const int dakey[10] = { AKEY_0, AKEY_1, AKEY_2, AKEY_3, AKEY_4,
                                   AKEY_5, AKEY_6, AKEY_7, AKEY_8, AKEY_9 };
    base = dakey[key - '0'];
  }
  else if (key == KM_KEY_RETURN) base = AKEY_RETURN;
  else if (key == KM_KEY_SPACE)  base = AKEY_SPACE;
  else if (key == KM_KEY_ESCAPE) base = AKEY_ESCAPE;
  else if (key == KM_KEY_TAB)    base = AKEY_TAB;
  else if (key == KM_KEY_BACKSP) base = AKEY_BACKSPACE;
  else                           base = punct_to_akey(key);

  if (base == AKEY_NONE) return AKEY_NONE;

  /* Apply modifiers. The M5Cardputer keymap gives us character values that
     ALREADY reflect shift (uppercase A-Z, shifted punctuation) — but Shift
     is also reported as a separate modifier bit. For Atari, Shift needs to
     be OR'd in explicitly when we want the matrix-level SHIFTed variant.
     We always OR it when the modifier is set; the caller ensures the key
     value is the "unshifted" glyph (we fold A-Z → a-z above, so Shift+'A'
     from the user comes through as key='A', mods.shift=1 → letter table
     gives a-z mapping, then OR AKEY_SHFT). */
  if (mods && mods->ctrl)  base |= AKEY_CTRL;
  if (mods && mods->shift) base |= AKEY_SHFT;
  return base;
}

/* Fn layer is wired in T3. Stub for now so linkage works. */
km_out_t keymap_fn(int key, const km_modifiers_t* mods) {
  (void)key; (void)mods;
  km_out_t out = { KM_OUT_NONE, 0 };
  return out;
}
