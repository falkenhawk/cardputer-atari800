/* keymap.h — Cardputer key → Atari matrix code (AKEY_*).
   Pure C so it's host-testable without pulling in M5Cardputer.

   The caller (input_port.cpp) is responsible for deciding which layer to
   consult based on the `fn` modifier:
     - fn=0 -> keymap_default()
     - fn=1 -> keymap_fn()
   The return value is the complete AKEY_* code (base | AKEY_SHFT | AKEY_CTRL),
   or -1 (AKEY_NONE) for an unmapped key. */

#ifndef CARDPUTER_KEYMAP_H
#define CARDPUTER_KEYMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinel values for keys that don't have a single-char representation.
   Chosen in the 0x100+ range to avoid colliding with valid ASCII. */
#define KM_KEY_RETURN  0x100
#define KM_KEY_SPACE   0x101
#define KM_KEY_ESCAPE  0x102
#define KM_KEY_TAB     0x103
#define KM_KEY_BACKSP  0x104
#define KM_KEY_DELETE  0x105

typedef struct {
  unsigned ctrl  : 1;
  unsigned shift : 1;
  unsigned fn    : 1;
} km_modifiers_t;

/* Default (non-Fn) layer: typewriter. Returns AKEY_* code or -1. */
int keymap_default(int key, const km_modifiers_t* mods);

/* Fn layer: console buttons, cursor, brightness/volume, display mode.
   Output type: some keys produce an AKEY_* matrix code; some produce a
   firmware-level action (display mode cycle, volume up/down). Returned
   via the union below. */

typedef enum {
  KM_OUT_NONE = 0,
  KM_OUT_AKEY,           /* write to INPUT_key_code */
  KM_OUT_CONSOL,         /* write to INPUT_key_consol (value = INPUT_CONSOL_* mask to clear) */
  KM_OUT_ACTION          /* firmware-level action: see km_action_t */
} km_out_kind_t;

typedef enum {
  KM_ACT_NONE = 0,
  KM_ACT_DISPLAY_MODE_CYCLE,
  KM_ACT_BRIGHTNESS_DOWN,
  KM_ACT_BRIGHTNESS_UP,
  KM_ACT_VOLUME_DOWN,
  KM_ACT_VOLUME_UP,
  KM_ACT_TOGGLE_INPUT_MODE,
  KM_ACT_INVERSE_VIDEO,
  KM_ACT_WARM_RESET,        /* Fn+4 */
  KM_ACT_COLD_RESET,        /* Fn+5 */
  KM_ACT_BREAK,             /* Fn+7 */
  KM_ACT_MENU_OPEN,         /* Fn+8 */
  KM_ACT_SAVE_STATE,        /* Fn+9 — M5 stub */
  KM_ACT_LOAD_STATE,        /* Fn+0 — M5 stub */
  KM_ACT_LOAD_XEX,          /* Fn+L — reload /atari800/test.xex from SD */
  KM_ACT_SCREENSHOT         /* Fn+P — dump LCD framebuffer to BMP on SD */
} km_action_t;

typedef struct {
  km_out_kind_t kind;
  int value;               /* AKEY_*, or INPUT_CONSOL_* mask, or km_action_t */
} km_out_t;

/* Fn layer: returns the above structured result. Unmapped -> kind=NONE. */
km_out_t keymap_fn(int key, const km_modifiers_t* mods);

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_KEYMAP_H */
