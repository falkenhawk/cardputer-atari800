/* input_port.cpp — bridge between M5Cardputer.Keyboard and the atari800 core.
   Owns the per-frame snapshot logic. All core-facing writes go through here;
   keymap.c stays pure-C and testable. */

#include <M5Cardputer.h>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <vector>

#include "input_port.h"
#include "keymap.h"

extern "C" {
#include "joystick.h"
#include "mode.h"
}

extern "C" {
/* Core globals we drive. Defined in src/port_impl.cpp; declared in
   lib/atari800/src/input.h. We forward-declare here to avoid pulling in
   the full core headers (and the atari800 UBYTE etc. type landmine).
   INPUT_key_code: AKEY_* code — negative = control action (AKEY_UI, WARMSTART),
   non-negative = POKEY matrix scancode (low 7 bits) with AKEY_SHFT (0x40) /
   AKEY_CTRL (0x80) optionally OR'd in bits 6-7.
   INPUT_key_consol: active-low 3-bit mask for Start/Select/Option — written
   by the Fn layer in T3. */
extern int  INPUT_key_code;     /* AKEY_* — see above */
extern int  INPUT_key_consol;   /* active-low 3-bit: START|SELECT|OPTION */

/* Joystick-1 output registers — declared as UBYTE[] in pia.h / gtia.h;
   using unsigned char here to avoid dragging in atari.h's UBYTE typedef
   (same trick port_impl.cpp uses for POKEY_KBCODE). Low nibble of
   PIA_PORT_input[0] is Joy-1 direction (active-low; 0x0F = centre).
   GTIA_TRIG[0] is Joy-1 trigger (0 = pressed, 1 = released). */
extern unsigned char PIA_PORT_input[2];
extern unsigned char GTIA_TRIG[4];

/* Anchor asserts to catch future drift of the vendored atari800 core's
   AKEY numbering — keymap.c embeds literal values; if the core renumbers,
   these fire at compile time so we notice before hardware. Including
   akey.h directly pulls in atari.h/UBYTE etc., which doesn't actually
   cost anything here since we're already in C++ and just want the macros. */
#include "../../lib/atari800/src/akey.h"
}

static_assert(AKEY_a       == 0x3f, "akey.h AKEY_a drifted — update keymap.c");
static_assert(AKEY_SHFT    == 0x40, "akey.h AKEY_SHFT drifted");
static_assert(AKEY_CTRL    == 0x80, "akey.h AKEY_CTRL drifted");
static_assert(AKEY_RETURN  == 0x0c, "akey.h AKEY_RETURN drifted");
static_assert(AKEY_BREAK   == -5,   "akey.h AKEY_BREAK drifted");

namespace input_port {

/* Firmware-level callbacks — main.cpp wires these at startup. Using function
   pointers rather than direct calls keeps input_port.cpp decoupled from the
   UI / settings / audio modules (they come later in the plan). */
static void (*on_action)(km_action_t act) = nullptr;

void set_action_handler(void (*fn)(km_action_t act)) { on_action = fn; }

/* Edge-detect Fn-chord ACTION keys so holding Fn+\\ doesn't cycle display
   modes 50 times per second. CONSOL writes are idempotent (held OPTION
   reports as pressed each frame, which is correct) and AKEY emissions go
   through INPUT_Frame's own edge detection, so only ACTIONs need this.
   Small fixed-size set is enough — Fn actions in practice are 1-2 keys. */
static constexpr size_t MAX_FN_KEYS = 8;
static int  prev_fn_action_keys[MAX_FN_KEYS] = {0};
static size_t prev_fn_action_count = 0;

static bool was_action_key_held_last_frame(int key) {
  for (size_t i = 0; i < prev_fn_action_count; i++) {
    if (prev_fn_action_keys[i] == key) return true;
  }
  return false;
}

/* True if any element of `word` matches `target_lc` (lowercase) or its
   uppercase form (ASCII Shift flip). The Cardputer reports Shift+E as 'E'
   in word[], so we have to accept both so holding Shift while moving the
   sprite doesn't silently drop the direction. */
static bool cluster_read_bit(const std::vector<char>& word, char target_lc) {
  for (char c : word) {
    if (c == target_lc || c == (char)(target_lc - 0x20)) return true;
  }
  return false;
}

/* Called once per frame from main.cpp, before Atari800_Frame().
   Reads the current M5Cardputer key state and writes INPUT_key_code /
   INPUT_key_consol. Joystick polling (T7) is NOT wired here yet. */
void poll() {
  auto& ks = M5Cardputer.Keyboard.keysState();

  km_modifiers_t mods = {
    /* .ctrl  = */ ks.ctrl  ? 1u : 0u,
    /* .shift = */ ks.shift ? 1u : 0u,
    /* .fn    = */ ks.fn    ? 1u : 0u,
  };

  /* Drain chords: walk ALL pressed characters this frame, not just the first.
     Fn layer uses chords (Fn+1..8) so a single-char view would miss them
     when the user is mid-rolling. But we still only write ONE code to
     INPUT_key_code — the LAST one we process. Actions and CONSOL writes
     accumulate. */
  int akey_out   = -1;      /* AKEY_NONE */
  int consol_out = 0x07;    /* INPUT_CONSOL_NONE: all bits set */

  /* Track Fn-chord keys pressed this frame so we can compute next frame's
     edge-detect set. */
  int this_fn_action_keys[MAX_FN_KEYS] = {0};
  size_t this_fn_action_count = 0;

  auto dispatch = [&](int key) {
    if (mods.fn) {
      km_out_t r = keymap_fn(key, &mods);
      switch (r.kind) {
        case KM_OUT_AKEY:   akey_out = r.value; break;
        case KM_OUT_CONSOL: consol_out &= ~r.value; break;   /* press = clear bit */
        case KM_OUT_ACTION: {
          /* Record as pressed-this-frame for next-frame comparison. */
          if (this_fn_action_count < MAX_FN_KEYS) {
            this_fn_action_keys[this_fn_action_count++] = key;
          }
          /* Only FIRE on the transition from not-held to held. */
          if (!was_action_key_held_last_frame(key)) {
            if (on_action) on_action((km_action_t)r.value);
          }
          break;
        }
        default: break;
      }
    } else {
      int a = keymap_default(key, &mods);
      if (a != -1) akey_out = a;
    }
  };

  /* Source chars: word[] holds printable, then named keys (enter/space/etc). */
  for (char c : ks.word) dispatch((unsigned char)c);
  if (ks.enter) dispatch(KM_KEY_RETURN);
  if (ks.space) dispatch(KM_KEY_SPACE);
  if (ks.tab)   dispatch(KM_KEY_TAB);
  if (ks.del)   dispatch(KM_KEY_BACKSP);

  INPUT_key_code   = akey_out;
  INPUT_key_consol = consol_out;

  /* Joystick: compute every frame (so we don't leak a stale press if mode
     flips from JOYSTICK to KEYBOARD mid-session). Only APPLY when in
     joystick mode AND Fn is not held (Fn+<key> chords don't move the
     stick). */
  uint8_t nib; int fire;
  if (mode_current() == MODE_JOYSTICK && !mods.fn) {
    joy_state_t j;
    std::memset(&j, 0, sizeof(j));

    /* Cluster 1: E up, S down, A left, D right, K fire1, L fire2. */
    j.c1.up    = cluster_read_bit(ks.word, 'e');
    j.c1.down  = cluster_read_bit(ks.word, 's');
    j.c1.left  = cluster_read_bit(ks.word, 'a');
    j.c1.right = cluster_read_bit(ks.word, 'd');
    j.c1.fire  = cluster_read_bit(ks.word, 'k');
    j.c1.fire2 = cluster_read_bit(ks.word, 'l');

    /* Cluster 2: ; up, . down, , left, / right, Z fire1, X fire2. */
    j.c2.up    = cluster_read_bit(ks.word, ';');
    j.c2.down  = cluster_read_bit(ks.word, '.');
    j.c2.left  = cluster_read_bit(ks.word, ',');
    j.c2.right = cluster_read_bit(ks.word, '/');
    j.c2.fire  = cluster_read_bit(ks.word, 'z');
    j.c2.fire2 = cluster_read_bit(ks.word, 'x');

    joystick_resolve(&j, &nib, &fire);

    /* In joystick mode, also squelch the character path so pressing E/S/A/D
       doesn't type into BASIC simultaneously with moving the sprite. */
    INPUT_key_code = -1;
  } else {
    nib  = 0x0F;    /* centre */
    fire = 0;
  }

  /* PIA_PORT_input[0]: low nibble = Joy-1, high nibble = Joy-2 (idle 0xF0). */
  PIA_PORT_input[0] = (unsigned char)(0xF0 | nib);
  /* GTIA_TRIG[0]: active-low (0 = pressed). */
  GTIA_TRIG[0] = fire ? 0 : 1;

  /* Commit the "pressed this frame" set as "pressed last frame" for the
     next poll. */
  for (size_t i = 0; i < MAX_FN_KEYS; i++) prev_fn_action_keys[i] = this_fn_action_keys[i];
  prev_fn_action_count = this_fn_action_count;
}

} /* namespace input_port */
