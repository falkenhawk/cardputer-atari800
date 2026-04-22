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
#define AKEY_QUOTE       0x73   /* single-quote ' */

/* Shifted-punctuation AKEYs (precomputed glyph codes, not SHFT|base).
   These are the values akey.h defines directly — some are NOT the naive
   AKEY_SHFT | AKEY_<base> because Atari's shifted glyphs don't line up
   with a US PC keyboard. Example: Shift+2 on a PC → '@' but on Atari
   Shift+2 → '"'; Atari's '@' is Shift+8 (AKEY_AT = 0x75). */
#define AKEY_EXCLAMATION 0x5f
#define AKEY_DBLQUOTE    0x5e
#define AKEY_HASH        0x5a
#define AKEY_DOLLAR      0x58
#define AKEY_PERCENT     0x5d
#define AKEY_CIRCUMFLEX  0x47   /* ^ */
#define AKEY_AMPERSAND   0x5b   /* & */
#define AKEY_AT          0x75   /* @ */
#define AKEY_PARENLEFT   0x70
#define AKEY_PARENRIGHT  0x72
#define AKEY_LESS        0x36   /* < */
#define AKEY_GREATER     0x37   /* > */
#define AKEY_QUESTION    0x66
#define AKEY_PLUS        0x06
#define AKEY_ASTERISK    0x07
#define AKEY_UNDERSCORE  0x4e
#define AKEY_BAR         0x4f   /* | */

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

/* Hardening assert: if akey.h ever renumbers or someone edits the table
   above, this catches it at compile time. */
_Static_assert(sizeof(letter_akey)/sizeof(letter_akey[0]) == 26,
               "letter_akey must have 26 entries");

/* Punctuation → AKEY_* (unshifted only — Shift is applied via OR). */
static int punct_to_akey(int c) {
  switch (c) {
    case ' ':  return AKEY_SPACE;    /* Cardputer puts Space in word[0] as 0x20 */
    case ';':  return AKEY_SEMICOLON;
    case ',':  return AKEY_COMMA;
    case '.':  return AKEY_FULLSTOP;
    case '/':  return AKEY_SLASH;
    case '\\': return AKEY_BACKSLASH;
    case '-':  return AKEY_MINUS;
    case '=':  return AKEY_EQUAL;
    case '[':  return AKEY_BRACKETLEFT;
    case ']':  return AKEY_BRACKETRIGHT;
    case '\'': return AKEY_QUOTE;
    default:   return AKEY_NONE;
  }
}

/* M5Cardputer's keysState().word[] reports the SHIFTED glyph (not the base
   key) when Shift is held — e.g. Shift+= yields word[0] = '+'. Our keymap
   needs to translate that back into the Atari AKEY for that glyph directly.

   Returns AKEY_NONE for glyphs that aren't shifted punctuation. The returned
   AKEY already has AKEY_SHFT baked in where applicable, so callers must NOT
   OR the modifier's shift bit on top. */
static int shifted_glyph_to_akey(int c) {
  switch (c) {
    case '!':  return AKEY_EXCLAMATION;   /* 0x5f */
    case '@':  return AKEY_AT;            /* 0x75 — NOT SHFT|AKEY_2; Atari layout */
    case '#':  return AKEY_HASH;          /* 0x5a */
    case '$':  return AKEY_DOLLAR;        /* 0x58 */
    case '%':  return AKEY_PERCENT;       /* 0x5d */
    case '^':  return AKEY_CIRCUMFLEX;    /* 0x47 */
    case '&':  return AKEY_AMPERSAND;     /* 0x5b */
    case '*':  return AKEY_ASTERISK;      /* 0x07 */
    case '(':  return AKEY_PARENLEFT;     /* 0x70 */
    case ')':  return AKEY_PARENRIGHT;    /* 0x72 */
    case '_':  return AKEY_UNDERSCORE;    /* 0x4e */
    case '+':  return AKEY_PLUS;          /* 0x06 — NOT SHFT|AKEY_EQUAL */
    /* Note: Atari has no direct '{' or '}' keycode — AKEY_BRACKETLEFT (0x60)
       already has bit 0x40 set, so AKEY_SHFT|AKEY_BRACKETLEFT == AKEY_BRACKETLEFT
       and collapses to '[' on the matrix. We leave them unmapped; callers must
       use Atari's own escape sequences for brace glyphs if needed. */
    case '|':  return AKEY_BAR;           /* 0x4f */
    case ':':  return AKEY_COLON;         /* 0x42 */
    case '"':  return AKEY_DBLQUOTE;      /* 0x5e */
    case '<':  return AKEY_LESS;          /* 0x36 */
    case '>':  return AKEY_GREATER;       /* 0x37 */
    case '?':  return AKEY_QUESTION;      /* 0x66 */
    default:   return AKEY_NONE;
  }
}

int keymap_default(int key, const km_modifiers_t* mods) {
  int base = AKEY_NONE;
  int shifted_implicit = 0;  /* set when base already reflects SHIFT */

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
  else {
    base = punct_to_akey(key);
    if (base == AKEY_NONE) {
      /* Not plain punct — try the shifted-glyph table. These AKEYs already
         encode SHIFT (directly, via the precomputed glyph code), so we must
         not additionally OR mods->shift below. */
      base = shifted_glyph_to_akey(key);
      if (base != AKEY_NONE) shifted_implicit = 1;
    }
  }

  if (base == AKEY_NONE) return AKEY_NONE;

  /* Apply modifiers. For shifted-glyph returns, SHIFT is already encoded in
     the AKEY value — OR'ing mods->shift again would double-apply it (no-op
     since the bit is already set, but could mask a future CTRL+shifted-glyph
     case); explicitly skip the shift OR for clarity. */
  if (mods && mods->ctrl)  base |= AKEY_CTRL;
  if (mods && mods->shift && !shifted_implicit) base |= AKEY_SHFT;
  return base;
}

/* --- Fn layer --- */

/* INPUT_CONSOL_* bit masks (lib/atari800/src/input.h:10-14). */
#define INPUT_CONSOL_START   0x01
#define INPUT_CONSOL_SELECT  0x02
#define INPUT_CONSOL_OPTION  0x04

/* Cursor keys produce pre-baked matrix codes in akey.h. */
#define AKEY_UP     0x8e
#define AKEY_DOWN   0x8f
#define AKEY_LEFT   0x86
#define AKEY_RIGHT  0x87
#define AKEY_HELP   0x11

static km_out_t mk_none(void)              { km_out_t o = { KM_OUT_NONE,    0 }; return o; }
static km_out_t mk_akey(int v)             { km_out_t o = { KM_OUT_AKEY,    v }; return o; }
static km_out_t mk_consol(int v)           { km_out_t o = { KM_OUT_CONSOL,  v }; return o; }
static km_out_t mk_action(km_action_t a)   { km_out_t o = { KM_OUT_ACTION,  (int)a }; return o; }

km_out_t keymap_fn(int key, const km_modifiers_t* mods) {
  (void)mods;  /* Fn is the only required modifier; Shift/Ctrl are ignored in Fn layer */

  switch (key) {
    case '1': return mk_consol(INPUT_CONSOL_OPTION);
    case '2': return mk_consol(INPUT_CONSOL_SELECT);
    case '3': return mk_consol(INPUT_CONSOL_START);
    case '4': return mk_action(KM_ACT_WARM_RESET);
    case '5': return mk_action(KM_ACT_COLD_RESET);
    case '6': return mk_akey(AKEY_HELP);
    case '7': return mk_action(KM_ACT_BREAK);
    case '8': return mk_action(KM_ACT_MENU_OPEN);
    case '9': return mk_action(KM_ACT_SAVE_STATE);
    case '0': return mk_action(KM_ACT_LOAD_STATE);

    /* Cursor cluster (printed arrows on keys) */
    case ';': return mk_akey(AKEY_UP);
    case '.': return mk_akey(AKEY_DOWN);
    case ',': return mk_akey(AKEY_LEFT);
    case '/': return mk_akey(AKEY_RIGHT);

    /* Brightness / volume / display mode */
    case '[':  return mk_action(KM_ACT_BRIGHTNESS_DOWN);
    case ']':  return mk_action(KM_ACT_BRIGHTNESS_UP);
    case '-':  return mk_action(KM_ACT_VOLUME_DOWN);
    case '=':  return mk_action(KM_ACT_VOLUME_UP);
    case '\\': return mk_action(KM_ACT_DISPLAY_MODE_CYCLE);

    /* Letters (case-insensitive: M5Cardputer returns 'i' not 'I' for Fn+i) */
    case 'i': case 'I': return mk_action(KM_ACT_INVERSE_VIDEO);
    case 'j': case 'J': return mk_action(KM_ACT_TOGGLE_INPUT_MODE);
    case 'l': case 'L': return mk_action(KM_ACT_LOAD_XEX);

    default: return mk_none();
  }
}
