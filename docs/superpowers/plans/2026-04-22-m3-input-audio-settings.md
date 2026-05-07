# cardputer-atari800 — Milestone 3: Input, Audio, ROM Browser

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** route Cardputer keyboard into the Atari core (full Fn layer + dual-cluster joystick), add POKEY audio via the ES8311 codec, and ship a usable Fn+L ROM browser for SD file loading — while preserving M2's tight-heap memory pattern.

**Architecture:** M3 stays inside M2's three-layer model (app / port / core) and adds two new peer modules plus a settings/menu layer:

- `src/input/` — polls `M5Cardputer.Keyboard`, resolves layer (Default / Fn / Joystick) from modifiers + mode state, and writes core-facing input globals (`INPUT_key_code`, `INPUT_key_consol`, `PIA_PORT_input[0]`, `GTIA_TRIG[0]`, `POKEY_KBCODE`) directly. The vendored core has **no `input.c`** (it was deliberately excluded in M2/T4), so we own the `INPUT_Frame()` / `INPUT_Scanline()` pump implementations in `port_impl.cpp`.
- `src/audio/` — owns `POKEYSND_Init` + a pair of pre-allocated 16-bit stereo buffers, alternates them via `M5Cardputer.Speaker.playRaw()`. Enables `#define SOUND` in `lib/atari800/src/config.h` so `POKEY_Update` actually calls into `POKEYSND_Update_ptr` instead of the no-op macro shadow.
- `src/settings/` + `src/ui/` — in-memory settings struct (persistence is M4) plus the M3 browser overlay. A fuller Fn+8 in-emulator menu remains M4 polish.

No save-state code (M5) and no on-SD config persistence (M4). The usable ROM browser belongs to M3; M4 can refine browser UX and integrate it with the fuller menu/settings flow.

**Tech Stack:** same as M2 + M5Cardputer's `Speaker_Class` (I²S driver + ES8311 enable callback, M5Unified) + atari800's `pokeysnd.c` (already compiled since M2, just not called because `SOUND` is undef).

**Prerequisites:**
- M2 complete (tag `v0.2-m2` or HEAD of master at/after commit `992de89`). A `.xex` boots from `/sd/atari800/test.xex` via `BINLOAD_Loader`; four display modes cycle with Fn+\\; ~16 KB heap free at steady state.
- **Hardware-on-hand for HUMAN CHECKPOINTS:** headphones or the built-in speaker working (smoke-tested in M1/M2 is not required — `Speaker.begin()` happens for the first time in this milestone).
- A test `.xex` that produces sound. Suggest a BASIC demo that calls `SOUND 0,...` or any standard Atari game that uses the click/beep. Any of the public-domain "10 print" BASIC → xex conversions will do.
- A test `.atr` disk for keyboard-mode verification (a BASIC environment). The embedded AltirraBASIC cart is always available as a fallback — hitting Reset with `basic=on` goes straight to a READY prompt, so an .atr is a nice-to-have, not strictly required.

**Reference docs for agent:**
- Spec: `docs/superpowers/specs/2026-04-20-cardputer-atari800-design.md` — sections 4 (input), 6 (audio), 7 (UI/menus), 9 (settings), 10 (boot behavior).
- M2 plan: `docs/superpowers/plans/2026-04-20-m2-core-and-first-frame.md` — T1 for atari800 vendor subset, T4 for "files excluded from vendored core" (you'll add a tiny slice of input.c-equivalent), T17 for the existing Fn+\\ hook you'll extend.
- atari800 core headers (read these before coding any module that touches them):
  - `lib/atari800/src/akey.h` — keyboard matrix scan codes, `AKEY_SHFT` / `AKEY_CTRL` modifier bits, cursor + console + sentinel values.
  - `lib/atari800/src/input.h` — `INPUT_key_code`, `INPUT_key_consol`, `INPUT_CONSOL_*` masks, `INPUT_STICK_*` nibbles.
  - `lib/atari800/src/pia.h` / `pia.c` — `PIA_PORT_input[2]` (joy 1+2 nibbles packed into `[0]`).
  - `lib/atari800/src/gtia.h` — `GTIA_TRIG[4]` (fire buttons, active-low).
  - `lib/atari800/src/pokey.h` — `POKEY_KBCODE`, `POKEY_IRQEN`, `POKEY_SKSTAT`.
  - `lib/atari800/src/pokeysnd.h` — `POKEYSND_Init`, `POKEYSND_Process`, `POKEYSND_num_pokeys`, `POKEYSND_enable_new_pokey` (⚠️ must set to `FALSE` — mzpokeysnd.c is **not** vendored).
  - `lib/atari800/src/atari.h` — `Atari800_machine_type` (only 3 enum values: `_800`, `_XLXE`, `_5200`), `Atari800_tv_mode`, `MEMORY_ram_size`, `Atari800_builtin_basic`, `Atari800_builtin_game`, `Atari800_SetMachineType()`, `Atari800_SetTVMode()`, `Atari800_InitialiseMachine()`, `Atari800_Coldstart()`.
  - `lib/atari800/src/sound.h` — `Sound_Initialise/Update/Pause/Continue/Exit` (currently NOT vendored; we add stub implementations as part of T10).
- M5Cardputer keyboard + speaker:
  - `.pio/libdeps/cardputer-adv/M5Cardputer/src/utility/Keyboard/Keyboard.h` — `KeysState`, `keyList()`, `getKeyValue()`, `keysState().fn`/`.ctrl`/`.shift`/`.alt` bits (⚠️ **`fn` is NOT in `modifier_keys` / `modifiers` bitmask** — read it directly off the struct).
  - `.pio/libdeps/cardputer-adv/M5Cardputer/src/utility/Keyboard/Keyboard_def.h` — `KEY_FN=0xff`, `KEY_BACKSPACE=0x2a`, `KEY_TAB=0x2b`, `KEY_ENTER=0x28`.
  - `.pio/libdeps/cardputer-adv/M5Unified/src/utility/Speaker_Class.hpp` — `speaker_config_t`, `begin()`, `config(cfg)`, `playRaw(int16_t*, len, rate, stereo, repeat, channel, stop_current)`, `isPlaying(ch)` (returns 0/1/2 — value 2 means both slots full, next call blocks). The header comment at line 175-176 spells out the correct streaming pattern: **two alternating buffers per channel, one call per buffer**.
  - `.pio/libdeps/cardputer-adv/M5Unified/src/M5Unified.cpp:2089-2102` — Cardputer-Adv auto-config (`pin_bck=41, pin_ws=43, pin_data_out=42, i2s_port=I2S_NUM_1, magnification=16`); note `sample_rate` default is **48000** and `stereo` default is **false`**. M3 overrides both.
  - `.pio/libdeps/cardputer-adv/M5Unified/src/M5Unified.cpp:675-698` — `_speaker_enabled_cb_cardputer_adv` — ES8311 bring-up bulk write. Runs automatically on `Speaker.begin()` / `Speaker.end()`. No codec code needed from us.
- M2 hard-won memory pattern, in `CLAUDE.md` under "The hard-won memory-management pattern". Memorize before touching `setup()`.

**Testing approach:**
- Pure-C modules (`input/keymap.c`, `input/mode.c`, `audio/pokey_glue.c`, `settings/settings.c`) have host-side tests via the existing CMake harness (`test/CMakeLists.txt`, already set up for M2).
- Audio + M5Cardputer interaction cannot be host-tested — validated at HUMAN CHECKPOINT (headphones or built-in speaker, audible click on reset + recognisable waveform).
- Menu rendering is hardware-only — HUMAN CHECKPOINT for visual verification.
- Every task with hardware behavior has a HUMAN CHECKPOINT step. Don't proceed past one until the human confirms.

**File structure after M3:**

```
cardputer-atari800/
├── lib/atari800/src/
│   └── config.h                        # modified: add #define SOUND (T9)
├── src/
│   ├── main.cpp                        # modified: pre-alloc audio, wire input, pump audio
│   ├── input/
│   │   ├── keymap.h
│   │   ├── keymap.c                    # C: Cardputer key → AKEY_* (default + Fn layers)
│   │   ├── joystick.h
│   │   ├── joystick.c                  # C: dual-cluster poll → INPUT_STICK_* nibble + fire
│   │   ├── mode.h
│   │   ├── mode.c                      # C: keyboard ↔ joystick mode state + auto-detect
│   │   └── input_port.cpp              # C++: glues M5Cardputer.Keyboard → the C modules and
│   │                                     writes core globals; strong impl of INPUT_Frame()
│   ├── audio/
│   │   ├── pokey_glue.h
│   │   ├── pokey_glue.c                # C: POKEYSND_Init wrapper + Process helpers
│   │   └── audio_out.cpp               # C++: M5Cardputer.Speaker init + two-buffer pump
│   ├── settings/
│   │   ├── settings.h
│   │   └── settings.c                  # C: in-memory settings struct + apply-to-core
│   └── ui/
│       ├── osd.h
│       ├── osd.cpp                     # transient corner text (2 s fade)
│       ├── menu.h
│       └── menu.cpp                    # Fn+8 overlay list menu
└── test/
    ├── test_keymap.c                   # NEW: default + Fn layer mapping
    ├── test_joystick.c                 # NEW: dual-cluster OR'ing + nibble encoding
    ├── test_mode.c                     # NEW: auto-detect from extension + override
    ├── test_pokey_glue.c               # NEW: init parameters round-trip + mono/stereo flag
    ├── test_settings.c                 # NEW: apply-to-core sequencing
    └── CMakeLists.txt                  # modified: add the 5 new test executables
```

Every new file has one responsibility. `src/input/` modules are pure C for host-testability; the `input_port.cpp` bridge is C++ because M5Cardputer's API is C++. Same pattern for audio (pure C glue, C++ bridge).

**Memory budget delta vs M2 steady-state (~16 KB free):**

| Item | Size | Where | Notes |
|---|---|---|---|
| Two POKEY output buffers (stereo, 441 frames each) | 2 × 441 × 4 = 3528 bytes | early `setup()` | 10 ms per buffer → 20 ms latency for two. Stereo (dual-POKEY toggle is per-frame cheap). |
| I²S DMA ring (M5 default 4 bufs × 256 frames × 4 B stereo) | ~4 KB | inside `Speaker.begin()` | We cut `dma_buf_count` from default 8 → 4 to halve this. |
| `spk_task` FreeRTOS stack | ~2.3 KB | inside `Speaker.begin()` | `1280 + dma_buf_len × 4` bytes per Speaker_Class.cpp:930. |
| Menu overlay scratch (text lines, column widths) | < 1 KB | static | Not on heap. |
| Keyboard layer state (per-frame snapshot) | ~256 bytes | static | Not on heap. |

**Total new live heap:** ~10 KB. Must be allocated in the early `setup()` window (before `M5Cardputer.begin()` eats ~200 KB) if it's on the heap. The FreeRTOS task stack + DMA ring happen inside `Speaker.begin()` — ordering that call carefully in `setup()` is a per-task subtopic in T10.

---

### Task 1: Scaffold `src/input/` + keymap default-layer host test

**Files:**
- Create: `src/input/keymap.h`
- Create: `src/input/keymap.c`
- Create: `test/test_keymap.c`
- Modify: `test/CMakeLists.txt`

We start with the default (unshifted, no Fn) typewriter layer: A-Z, 0-9, punctuation, Return, Space, Escape, Tab, Backspace. Each maps to an `AKEY_*` code the Atari core understands.

- [ ] **Step 1: Write the failing test first**

`test/test_keymap.c`:

```c
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
```

- [ ] **Step 2: Create `src/input/keymap.h`**

```c
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
  KM_ACT_LOAD_STATE         /* Fn+0 — M5 stub */
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
```

- [ ] **Step 3: Create `src/input/keymap.c` — default layer only (Fn layer in T3)**

```c
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
  /* a-z */ 0x3f, 0x15, 0x12, 0x3a, 0x2a, 0x38, 0x3d, 0x39,
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
```

- [ ] **Step 4: Add test to `test/CMakeLists.txt`**

Append at the bottom of the existing file:

```cmake
add_executable(test_keymap test_keymap.c ../src/input/keymap.c)
add_test(NAME keymap COMMAND test_keymap)
```

- [ ] **Step 5: Build + run**

```bash
cmake -B build -S test
cmake --build build
ctest --test-dir build --output-on-failure -R keymap
```

Expected: `PASS: keymap default` and `1/1 Test #1: keymap .................... Passed`.

- [ ] **Step 6: Verify firmware still builds**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS, no new warnings. `src/input/keymap.c` is picked up by PlatformIO's auto-discovery (no `platformio.ini` changes needed).

- [ ] **Step 7: Commit**

```bash
git add src/input/keymap.h src/input/keymap.c test/test_keymap.c test/CMakeLists.txt
git commit -m "input: default-layer keymap (a-z, 0-9, punctuation, named keys) + host test"
```

---

### Task 2: Input port bridge — pump `INPUT_Frame()` from M5Cardputer

**Files:**
- Create: `src/input/input_port.cpp`
- Modify: `src/port_impl.cpp` (remove the empty `INPUT_Frame()` stub so our strong impl wins)
- Modify: `src/main.cpp` (call `input_port::poll()` from `loop()` before `Atari800_Frame`)

Plan: on each frame, read `M5Cardputer.Keyboard.keysState()`, translate the first printable char via `keymap_default` into `INPUT_key_code`, and let the core pick it up inside `Atari800_Frame`. Joystick and Fn layer come in T3/T5. This task proves the input path is plumbed.

- [ ] **Step 1: Create `src/input/input_port.cpp`**

```cpp
/* input_port.cpp — bridge between M5Cardputer.Keyboard and the atari800 core.
   Owns the per-frame snapshot logic. All core-facing writes go through here;
   keymap.c stays pure-C and testable. */

#include <M5Cardputer.h>
#include <stdint.h>

#include "keymap.h"

extern "C" {
/* Core globals we drive. Declared in lib/atari800/src/input.h and pokey.h
   but we can avoid including those headers — just declare directly. */
extern int  INPUT_key_code;     /* negative sentinel or POKEY_KBCODE value */
extern int  INPUT_key_shift;    /* unused by vendored core */
extern int  INPUT_key_consol;   /* active-low 3-bit: START|SELECT|OPTION */
}

namespace input_port {

/* Called once per frame from main.cpp, before Atari800_Frame().
   Reads the current M5Cardputer key state and writes INPUT_key_code.
   If no key is pressed, writes AKEY_NONE (-1). */
void poll() {
  auto& ks = M5Cardputer.Keyboard.keysState();

  km_modifiers_t mods = {
    /* .ctrl  = */ ks.ctrl  ? 1u : 0u,
    /* .shift = */ ks.shift ? 1u : 0u,
    /* .fn    = */ ks.fn    ? 1u : 0u,
  };

  /* First printable from keysState().word (already case-corrected by
     M5Cardputer). If multiple keys are down, we pick the first — good enough
     for typing; chords are for the Fn layer (T3). */
  int first_key = 0;
  if (!ks.word.empty()) first_key = (unsigned char)ks.word[0];
  else if (ks.enter)    first_key = KM_KEY_RETURN;
  else if (ks.space)    first_key = KM_KEY_SPACE;
  else if (ks.tab)      first_key = KM_KEY_TAB;
  else if (ks.del)      first_key = KM_KEY_BACKSP;

  int akey = (first_key != 0) ? keymap_default(first_key, &mods) : -1;
  INPUT_key_code = akey;
}

} /* namespace input_port */
```

- [ ] **Step 2: Remove the empty `INPUT_Frame()` stub from `port_impl.cpp`**

In `src/port_impl.cpp`, find the block around line 199:

```cpp
int  INPUT_Initialise(int *argc, char *argv[])  { (void)argc; (void)argv; return 1; }
void INPUT_Exit(void)                           {}
void INPUT_Frame(void)                          {}
void INPUT_Scanline(void)                       {}
```

Leave `INPUT_Initialise`, `INPUT_Exit`, `INPUT_Scanline` intact. Delete the `void INPUT_Frame(void) {}` line — we'll provide a strong definition in `input_port.cpp` next step.

Wait — we don't want `INPUT_Frame()` to be what calls our `input_port::poll()`. The core calls `INPUT_Frame()` from inside `Atari800_Frame()` (atari.c:1334), which means it runs AFTER the frame has been stepped. We want input captured BEFORE the frame. So `INPUT_Frame()` stays a no-op here, and `input_port::poll()` is called explicitly from `main.cpp`'s `loop()` just before `Atari800_Frame()`. Revert the deletion.

Keep `void INPUT_Frame(void) {}` as-is in `port_impl.cpp`. No changes to that file in this task.

- [ ] **Step 3: Call `input_port::poll()` from `main.cpp`**

At the top of `src/main.cpp`, add:

```cpp
#include "input/input_port.cpp.h"  /* see next step */
```

Actually, since `input_port.cpp` only exposes a single function, just forward-declare it at file scope in `main.cpp`:

```cpp
namespace input_port { void poll(); }
```

(Add this near the other `extern "C"` declarations at the top of `main.cpp`, around line 22.)

In `loop()`, modify the frame block around line 302:

```cpp
  // Frame loop — run atari800 at ~50 Hz (PAL) and present.
  static uint32_t last_frame_ms = 0;
  static int frame_count = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) {
    last_frame_ms = now;
    input_port::poll();          // <-- NEW: snapshot keyboard before stepping
    if (frame_count < 3) {
      Serial.printf("frame %d begin\n", frame_count);
      dump_shadow_ptrs("pre-frame");
    }
    Atari800_Frame();
    if (frame_count < 3) {
      Serial.printf("frame %d done\n", frame_count);
    }
    frame_count++;
    renderer::present(reinterpret_cast<const uint8_t*>(Screen_atari));
  }
```

- [ ] **Step 4: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS. Flash usage nudges up by ~1 KB.

- [ ] **Step 5: HUMAN CHECKPOINT — type at the BASIC prompt**

Prep: rename your current `/sd/atari800/test.xex` (or just delete it) so the emulator boots to AltirraBASIC's READY prompt instead of loading the xex. Or keep it loaded — as long as the running app accepts keyboard input.

Bump the splash version to `v0.3-m3-t2` in `main.cpp` (the two places: `Serial.println("FW_VER=...")` and `d.print("v0.3-m3-t2")`).

Build, copy to SD as `cardputer-atari800.m3-t2.bin`, flash via Launcher.

Expected:
- LCD shows BASIC READY prompt.
- Typing `PRINT "HI"` and pressing Return echoes to the screen. Each letter appears as you press it.
- Serial still prints the `keys:` debug lines (M2 left that in).

If nothing echoes:
- Serial should show the `keys:` debug when you press a key — confirms the Keyboard scanner works.
- If `keys:` fires but screen doesn't update: `INPUT_key_code` isn't being seen by the core. Likely `AKEY_*` code wrong OR the core is consuming it faster than we write. Add `Serial.printf("akey=0x%02X\n", akey);` inside `poll()` to inspect.
- If the screen echoes garbage: wrong `AKEY_*` mapping. Cross-check a single letter, e.g. `a` should be `0x3f` after `keymap_default('a', ...)`.

Do not proceed until `PRINT "HELLO"` + Return prints `HELLO` on the next line.

- [ ] **Step 6: Commit**

```bash
git add src/input/input_port.cpp src/main.cpp
git commit -m "input: bridge M5Cardputer.Keyboard → INPUT_key_code each frame

BASIC prompt now accepts typing via the default keymap layer. Fn + joystick
are deferred to T3/T5."
```

---

### Task 3: Fn layer — console buttons, cursor, actions

**Files:**
- Modify: `src/input/keymap.c` (implement `keymap_fn`)
- Modify: `src/input/keymap.h` (already has `km_out_t` + `km_action_t`)
- Modify: `test/test_keymap.c` (extend with Fn cases)
- Modify: `src/input/input_port.cpp` (dispatch on Fn modifier)

Per spec §4.3, `Fn+1..7` → console/reset keys, `Fn+8` → menu (action), `Fn+9/0` → save/load (M5 stubs), `Fn+; . , /` → Atari cursor ↑↓←→, `Fn+[ ]` → brightness, `Fn+- =` → volume, `Fn+\\` → display mode cycle (already wired in M2/T17 — we'll route it through this dispatch too), `Fn+i` → inverse video, `Fn+j` → toggle input mode.

- [ ] **Step 1: Extend `test/test_keymap.c`**

Append before the final `if (fail)` block:

```c
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
```

- [ ] **Step 2: Run test, expect most Fn cases to FAIL**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R keymap
```

Expected: many `FAIL: Fn+…` lines. `keymap_fn` is still a stub.

- [ ] **Step 3: Implement `keymap_fn` in `src/input/keymap.c`**

Replace the stub body at the bottom of `keymap.c`:

```c
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

  /* Digits 0-8 */
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

    default: return mk_none();
  }
}
```

- [ ] **Step 4: Re-run test, expect PASS**

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R keymap
```

Expected: `PASS: keymap default` (still) + all Fn cases pass.

- [ ] **Step 5: Dispatch in `input_port.cpp`**

Replace the `poll()` body in `src/input/input_port.cpp`:

```cpp
namespace input_port {

/* Firmware-level callbacks — main.cpp wires these at startup. Using function
   pointers rather than direct calls keeps input_port.cpp decoupled from the
   UI / settings / audio modules (they come later in the plan). */
static void (*on_action)(km_action_t act) = nullptr;

void set_action_handler(void (*fn)(km_action_t act)) { on_action = fn; }

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
  int  akey_out        = -1;      /* AKEY_NONE */
  int  consol_out      = 0x07;    /* INPUT_CONSOL_NONE: all bits set */

  auto dispatch = [&](int key) {
    if (mods.fn) {
      km_out_t r = keymap_fn(key, &mods);
      switch (r.kind) {
        case KM_OUT_AKEY:   akey_out = r.value; break;
        case KM_OUT_CONSOL: consol_out &= ~r.value; break;   /* press = clear bit */
        case KM_OUT_ACTION: if (on_action) on_action((km_action_t)r.value); break;
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

  INPUT_key_code    = akey_out;
  INPUT_key_consol  = consol_out;
}

} /* namespace input_port */
```

Add a header for the action handler hook so `main.cpp` can wire it. Create `src/input/input_port.h`:

```cpp
/* input_port.h — public surface of input_port.cpp */
#pragma once

extern "C" {
#include "keymap.h"
}

namespace input_port {

/* Call once per frame from main.cpp, before Atari800_Frame(). */
void poll();

/* Register a handler for firmware-level Fn actions (menu/reset/brightness/...).
   Called from within poll() on the same (main) thread. */
void set_action_handler(void (*fn)(km_action_t act));

} /* namespace input_port */
```

Replace the forward-declaration in `main.cpp` with:

```cpp
#include "input/input_port.h"
```

- [ ] **Step 6: Expose `renderer::get_mode()` so menu.cpp can read the current mode**

M2's `src/display/renderer.cpp` keeps `current_mode` as a file-static (line ~1200 in the M2 plan). T15's menu needs to read it. Add a one-line accessor.

In `src/display/renderer.h`, under the existing `void set_mode(Mode m);`:

```cpp
Mode get_mode();
```

In `src/display/renderer.cpp`, add next to `set_mode`:

```cpp
Mode get_mode() { return current_mode; }
```

- [ ] **Step 7: Remove `static` from `g_display_mode` in main.cpp — or better, delete it entirely**

In `src/main.cpp`, the M2 code has (around line 39):

```cpp
static renderer::Mode g_display_mode = renderer::Mode::Stretch;
```

We no longer need a separate copy — the renderer is the single source of truth. Delete that line. Keep the `mode_name()` helper (it takes a `Mode` by value).

- [ ] **Step 8: Wire the action handler in `main.cpp`**

Add a handler function at file scope (near `mode_name`, around line 40):

```cpp
static void on_input_action(km_action_t act) {
  switch (act) {
    case KM_ACT_DISPLAY_MODE_CYCLE: {
      renderer::Mode next = static_cast<renderer::Mode>(
        (static_cast<int>(renderer::get_mode()) + 1) % 4);
      renderer::set_mode(next);
      Serial.printf("display: %s\n", mode_name(next));
      break;
    }
    case KM_ACT_BRIGHTNESS_DOWN: {
      uint8_t b = M5Cardputer.Display.getBrightness();
      if (b > 16) M5Cardputer.Display.setBrightness(b - 16);
      Serial.printf("brightness: %u\n", M5Cardputer.Display.getBrightness());
      break;
    }
    case KM_ACT_BRIGHTNESS_UP: {
      uint8_t b = M5Cardputer.Display.getBrightness();
      if (b <= 255 - 16) M5Cardputer.Display.setBrightness(b + 16);
      Serial.printf("brightness: %u\n", M5Cardputer.Display.getBrightness());
      break;
    }
    case KM_ACT_VOLUME_DOWN: case KM_ACT_VOLUME_UP:
    case KM_ACT_MENU_OPEN:
    case KM_ACT_SAVE_STATE: case KM_ACT_LOAD_STATE:
    case KM_ACT_WARM_RESET: case KM_ACT_COLD_RESET: case KM_ACT_BREAK:
    case KM_ACT_INVERSE_VIDEO: case KM_ACT_TOGGLE_INPUT_MODE:
      Serial.printf("action: %d (not yet wired)\n", (int)act);
      break;
    default: break;
  }
}
```

In `setup()`, after `M5Cardputer.begin()`, register the handler:

```cpp
  input_port::set_action_handler(on_input_action);
```

Remove the duplicate M2 Fn+\\ inline dispatch from `loop()` — the block that reads `status.fn` and switches `g_display_mode` is now redundant (the action handler does it). Delete lines ~284-294 of current `main.cpp`:

```cpp
    /* DELETE THIS BLOCK — action handler replaces it:
    if (status.fn) {
      for (auto c : status.word) {
        if (c == '\\') {
          g_display_mode = static_cast<renderer::Mode>(...);
          ...
        }
      }
    }
    */
```

Keep the rest of the M2 `keys:` debug-print block — it's harmless.

- [ ] **Step 9: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 10: HUMAN CHECKPOINT — Fn layer works**

Bump splash to `v0.3-m3-t3`. Flash.

Verify each Fn binding:
- `Fn+\\` still cycles display modes (action-handler path, not the M2 direct path).
- `Fn+[` / `Fn+]` dim/brighten the LCD noticeably. Serial: `brightness: NN`.
- `Fn+1/2/3` — from BASIC's READY, press these while holding Fn. Option has no visible effect in BASIC, but **Fn+3 (Start)** should trigger a ding or cause screen flicker (AltirraOS uses Start during boot). Most visible: **Fn+6 (Help)** at the BASIC prompt should insert an inverse-video H or beep depending on what BASIC does with HELP key (AKEY_HELP = 0x11). If nothing visible, at least Serial should show `action: ...` or the INPUT_key_code handling — confirm via serial.
- `Fn+; . , /` — type `10 ?` then Fn+; ↑ and you should see BASIC echo the up-arrow glyph, or cursor should move depending on context.

Do not proceed until Fn+\\ (display mode) and Fn+[/] (brightness) both confirmed working. The console-key bindings are best validated in-game (T5).

- [ ] **Step 11: Commit**

```bash
git add src/input/keymap.c test/test_keymap.c src/input/input_port.cpp src/input/input_port.h src/display/renderer.h src/display/renderer.cpp src/main.cpp
git commit -m "input: Fn layer — console buttons, cursor, brightness/mode actions

Fn+1-3 -> INPUT_key_consol (Option/Select/Start), Fn+4-7 -> reset/help/break,
Fn+8 -> menu stub (T15 wires real menu), Fn+9/0 -> save/load stubs (M5),
Fn+; . , / -> cursor AKEY_{UP,DOWN,LEFT,RIGHT}, Fn+[/] / Fn+-/= -> brightness
and volume actions, Fn+\\ -> display mode cycle (replaces M2 inline dispatch)."
```

---

### Task 4: Warm reset + cold reset + Break actions

**Files:**
- Modify: `src/main.cpp` (extend `on_input_action`)

- [ ] **Step 1: Extend the action handler**

```cpp
extern "C" {
void Atari800_Warmstart(void);
void Atari800_Coldstart(void);
}
```

Add these extern declarations near the other `extern "C"` block at the top of `main.cpp`.

In `on_input_action`, replace the catch-all warm/cold/break case:

```cpp
    case KM_ACT_WARM_RESET:
      Serial.println("action: warm reset");
      Atari800_Warmstart();
      break;
    case KM_ACT_COLD_RESET:
      Serial.println("action: cold reset");
      Atari800_Coldstart();
      break;
    case KM_ACT_BREAK:
      Serial.println("action: break");
      INPUT_key_code = -5;  /* AKEY_BREAK sentinel per akey.h:12 */
      break;
```

(`INPUT_key_code` needs an `extern "C" { extern int INPUT_key_code; }` at file scope; it's already referenced in `port_impl.cpp` so declaration is valid — just make sure `main.cpp` sees it. Add it near the other `extern "C"` block.)

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: HUMAN CHECKPOINT — reset works**

Bump splash to `v0.3-m3-t4`. Flash.

From the BASIC prompt:
- Type `PRINT "AAAA"` to prove input is live.
- Press **Fn+4** — screen clears, READY reappears but variables persist. Warm reset confirmed.
- Type `X=42` to store something.
- Press **Fn+5** — screen clears again. Type `PRINT X`; should show `0` (cold reset zeroed RAM). Cold reset confirmed.
- Run a BASIC program (e.g. `10 GOTO 10`), press **Fn+7** — should halt with `STOPPED AT 10`.

Do not proceed until all three reset variants verified.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "input: Fn+4/5/7 trigger Warmstart / Coldstart / AKEY_BREAK"
```

---

### Task 5: Joystick module + host test (dual cluster encoding)

**Files:**
- Create: `src/input/joystick.h`
- Create: `src/input/joystick.c`
- Create: `test/test_joystick.c`
- Modify: `test/CMakeLists.txt`

Per spec §4.2, two clusters (left: `E S A D K L`; right: `; . , / Z X`) are simultaneously polled and OR'd into one logical Joystick-1. Direction bits are active-low nibbles per atari800's `INPUT_STICK_*` constants.

- [ ] **Step 1: Host test first**

`test/test_joystick.c`:

```c
/* test_joystick.c — dual-cluster joystick encoding.
   A pressed direction CLEARS its bit (active-low). Centre = 0x0F. */

#include <stdio.h>
#include <stdlib.h>
#include "../src/input/joystick.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  joy_state_t in = {0};

  /* Idle -> 0x0F (INPUT_STICK_CENTRE), fire=0 */
  uint8_t nib; int fire;
  joystick_resolve(&in, &nib, &fire);
  CHECK(nib == 0x0f, "idle -> CENTRE 0x0F");
  CHECK(fire == 0,   "idle -> fire=0");

  /* Cluster-1 UP only */
  in = (joy_state_t){0};
  in.c1.up = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_FORWARD = 0x0E (bit 0 cleared) */
  CHECK(nib == 0x0e, "cluster1 up -> 0x0E");

  /* Cluster-2 RIGHT only */
  in = (joy_state_t){0};
  in.c2.right = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_RIGHT = 0x07 (bit 3 cleared) */
  CHECK(nib == 0x07, "cluster2 right -> 0x07");

  /* Both clusters press DIFFERENT directions -> OR (diagonal) */
  in = (joy_state_t){0};
  in.c1.up = 1;
  in.c2.right = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_UR = 0x06 (bits 0 and 3 cleared) */
  CHECK(nib == 0x06, "up + right -> UR 0x06");

  /* Opposing directions: up + down should resolve to one of them, not centre.
     Block-opposite-directions policy: if both up AND down, prefer up
     (arbitrary but deterministic). */
  in = (joy_state_t){0};
  in.c1.up = 1;
  in.c1.down = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(nib == 0x0e, "up+down -> up (0x0E) per block-opposite policy");

  /* Fire primary only (cluster 1 K) */
  in = (joy_state_t){0};
  in.c1.fire = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(fire == 1, "cluster1 fire -> 1");

  /* Fire secondary only (cluster 2 X) -> still fire=1 (single logical fire) */
  in = (joy_state_t){0};
  in.c2.fire2 = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(fire == 1, "cluster2 fire2 -> 1");

  if (fail) return EXIT_FAILURE;
  printf("PASS: joystick\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Create `src/input/joystick.h`**

```c
/* joystick.h — dual-cluster joystick resolver.
   Takes booleans for each cluster's direction + fire bits and produces
   one active-low nibble + one fire bit for Joystick-1.

   Pure C so it host-tests without M5Cardputer. */

#ifndef CARDPUTER_JOYSTICK_H
#define CARDPUTER_JOYSTICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned up    : 1;
  unsigned down  : 1;
  unsigned left  : 1;
  unsigned right : 1;
  unsigned fire  : 1;   /* primary: K / Z */
  unsigned fire2 : 1;   /* secondary: L / X */
} joy_cluster_t;

typedef struct {
  joy_cluster_t c1;   /* ESAD + KL */
  joy_cluster_t c2;   /* ;.,/ + ZX */
} joy_state_t;

/* Resolve dual-cluster state to a single Joystick-1 encoding.
   *nibble_out: active-low 4-bit value in [0x00..0x0F]. 0x0F = centre.
                Encoding matches atari800 INPUT_STICK_*:
                  bit 0 cleared = forward (up)
                  bit 1 cleared = back    (down)
                  bit 2 cleared = left
                  bit 3 cleared = right
   *fire_out: 1 if any fire bit (primary or secondary) pressed, else 0.

   Block-opposite policy: if both up+down or both left+right are pressed
   simultaneously, only the first-encountered direction (up, left) is
   emitted. This mirrors atari800's INPUT_joy_block_opposite_directions=1. */
void joystick_resolve(const joy_state_t* in,
                      uint8_t* nibble_out, int* fire_out);

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_JOYSTICK_H */
```

- [ ] **Step 3: Create `src/input/joystick.c`**

```c
#include "joystick.h"

void joystick_resolve(const joy_state_t* in,
                      uint8_t* nibble_out, int* fire_out) {
  /* OR the two cluster's direction bits. */
  unsigned up    = in->c1.up    | in->c2.up;
  unsigned down  = in->c1.down  | in->c2.down;
  unsigned left  = in->c1.left  | in->c2.left;
  unsigned right = in->c1.right | in->c2.right;

  /* Block opposites. */
  if (up && down) down = 0;
  if (left && right) right = 0;

  uint8_t nib = 0x0F;
  if (up)    nib &= (uint8_t)~0x01;    /* clear bit 0 */
  if (down)  nib &= (uint8_t)~0x02;
  if (left)  nib &= (uint8_t)~0x04;
  if (right) nib &= (uint8_t)~0x08;

  *nibble_out = nib;
  *fire_out   = (in->c1.fire | in->c1.fire2 | in->c2.fire | in->c2.fire2) ? 1 : 0;
}
```

- [ ] **Step 4: Add test to `test/CMakeLists.txt`**

Append:

```cmake
add_executable(test_joystick test_joystick.c ../src/input/joystick.c)
add_test(NAME joystick COMMAND test_joystick)
```

- [ ] **Step 5: Build + run**

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R joystick
```

Expected: `PASS: joystick`.

Verify firmware still builds:

```bash
pio run -e cardputer-adv
```

- [ ] **Step 6: Commit**

```bash
git add src/input/joystick.h src/input/joystick.c test/test_joystick.c test/CMakeLists.txt
git commit -m "input: dual-cluster joystick resolver (OR'd into Joystick-1) + host test"
```

---

### Task 6: Mode module + auto-detect on xex load

**Files:**
- Create: `src/input/mode.h`
- Create: `src/input/mode.c`
- Create: `test/test_mode.c`
- Modify: `test/CMakeLists.txt`

Keyboard vs Joystick input mode is a per-session state. Auto-detect on file load: `.xex/.car` → joystick, `.atr/.bas/.cas` → keyboard. `Fn+J` manually toggles.

- [ ] **Step 1: Host test first**

`test/test_mode.c`:

```c
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
```

- [ ] **Step 2: Create `src/input/mode.h`**

```c
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
   .xex / .car -> MODE_JOYSTICK
   .atr / .bas / .cas -> MODE_KEYBOARD
   unknown -> leave mode unchanged */
void mode_autodetect_for(const char* filename);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: Create `src/input/mode.c`**

```c
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
    /* .exe is an alias for .xex on some distributions, but spec 1.2 lists
       it under "executables" (joystick). If you treat it as keyboard mode
       for consistency with BASIC-like runtime environments, switch this
       branch. I'm leaving it as keyboard because BASIC test files sometimes
       rename to .exe. */
    current = MODE_KEYBOARD;
  }
  /* else: unknown extension, leave as-is */
}
```

- [ ] **Step 4: Add to CMakeLists**

```cmake
add_executable(test_mode test_mode.c ../src/input/mode.c)
add_test(NAME mode COMMAND test_mode)
```

- [ ] **Step 5: Build + test**

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R mode
pio run -e cardputer-adv
```

Both should succeed.

- [ ] **Step 6: Commit**

```bash
git add src/input/mode.h src/input/mode.c test/test_mode.c test/CMakeLists.txt
git commit -m "input: keyboard/joystick mode with ext-based auto-detect + host test"
```

---

### Task 7: Wire joystick + mode into input_port + main

**Files:**
- Modify: `src/input/input_port.cpp`
- Modify: `src/input/input_port.h`
- Modify: `src/main.cpp`

Poll both clusters every frame. In `MODE_JOYSTICK`, write `PIA_PORT_input[0] = 0xF0 | nib` and `GTIA_TRIG[0] = !fire`. In `MODE_KEYBOARD`, idle the joystick (centre + no-fire) so games that read stick don't see ghost presses.

- [ ] **Step 1: Extend `input_port.h`**

Add:

```cpp
namespace input_port {
  /* Current input mode flows through here so the joystick can be idled
     in keyboard mode. input_port knows nothing about auto-detect — that's
     mode.c; main.cpp calls mode_autodetect_for() on xex load. */
}
```

No API additions needed — `mode_current()` is used directly inside `input_port.cpp`.

- [ ] **Step 2: Update `input_port.cpp`**

Replace the file with this (full body, incorporating T2's default-layer + T3's Fn dispatch + T7's joystick):

```cpp
#include <M5Cardputer.h>
#include <cstring>
#include "input_port.h"
#include "joystick.h"
#include "mode.h"

extern "C" {
extern int  INPUT_key_code;
extern int  INPUT_key_shift;
extern int  INPUT_key_consol;

/* Core joystick/fire registers (declared in pia.c and gtia.c). */
extern unsigned char PIA_PORT_input[2];
extern unsigned char GTIA_TRIG[4];
}

namespace input_port {

static void (*on_action)(km_action_t act) = nullptr;
void set_action_handler(void (*fn)(km_action_t act)) { on_action = fn; }

/* Key-char sentinels for the joystick layout (spec 4.2). Read as the
   'word[]' / raw keys against the keymap grid. M5Cardputer reports
   unmodified case for word[] when no Shift is held, so a lowercase test
   suffices. */
static bool cluster_read_bit(const std::vector<char>& word, char target_lc) {
  for (char c : word) {
    if (c == target_lc || c == (char)(target_lc - 0x20)) return true;
  }
  return false;
}

static joy_state_t read_joy_clusters(const m5::Keyboard_Class::KeysState& ks) {
  joy_state_t s;
  std::memset(&s, 0, sizeof(s));

  /* Cluster 1: E up, S down, A left, D right, K fire1, L fire2 */
  s.c1.up    = cluster_read_bit(ks.word, 'e');
  s.c1.down  = cluster_read_bit(ks.word, 's');
  s.c1.left  = cluster_read_bit(ks.word, 'a');
  s.c1.right = cluster_read_bit(ks.word, 'd');
  s.c1.fire  = cluster_read_bit(ks.word, 'k');
  s.c1.fire2 = cluster_read_bit(ks.word, 'l');

  /* Cluster 2: ; up, . down, , left, / right, Z fire1, X fire2 */
  s.c2.up    = cluster_read_bit(ks.word, ';');
  s.c2.down  = cluster_read_bit(ks.word, '.');
  s.c2.left  = cluster_read_bit(ks.word, ',');
  s.c2.right = cluster_read_bit(ks.word, '/');
  s.c2.fire  = cluster_read_bit(ks.word, 'z');
  s.c2.fire2 = cluster_read_bit(ks.word, 'x');

  return s;
}

void poll() {
  auto& ks = M5Cardputer.Keyboard.keysState();

  km_modifiers_t mods = {
    /* .ctrl  = */ ks.ctrl  ? 1u : 0u,
    /* .shift = */ ks.shift ? 1u : 0u,
    /* .fn    = */ ks.fn    ? 1u : 0u,
  };

  int  akey_out   = -1;     /* AKEY_NONE */
  int  consol_out = 0x07;   /* CONSOL_NONE: all bits set */

  auto dispatch = [&](int key) {
    if (mods.fn) {
      km_out_t r = keymap_fn(key, &mods);
      switch (r.kind) {
        case KM_OUT_AKEY:   akey_out = r.value; break;
        case KM_OUT_CONSOL: consol_out &= ~r.value; break;
        case KM_OUT_ACTION: if (on_action) on_action((km_action_t)r.value); break;
        default: break;
      }
    } else if (mode_current() == MODE_KEYBOARD) {
      /* In keyboard mode, ALL non-Fn keys go through the default keymap. */
      int a = keymap_default(key, &mods);
      if (a != -1) akey_out = a;
    }
    /* In joystick mode without Fn, typewriter keys are consumed by the
       joystick cluster reader below — don't double-route into INPUT_key_code. */
  };

  for (char c : ks.word) dispatch((unsigned char)c);
  if (ks.enter) dispatch(KM_KEY_RETURN);
  if (ks.space) dispatch(KM_KEY_SPACE);
  if (ks.tab)   dispatch(KM_KEY_TAB);
  if (ks.del)   dispatch(KM_KEY_BACKSP);

  INPUT_key_code   = akey_out;
  INPUT_key_consol = consol_out;

  /* Joystick: always compute, but only apply when in joystick mode.
     In keyboard mode we still want the PIA/TRIG idle so a stale press
     from a previous frame doesn't linger. */
  uint8_t nib; int fire;
  if (mode_current() == MODE_JOYSTICK && !mods.fn) {
    joy_state_t j = read_joy_clusters(ks);
    joystick_resolve(&j, &nib, &fire);
  } else {
    nib  = 0x0F;    /* centre */
    fire = 0;
  }

  /* PIA_PORT_input[0]: low nibble = Joy-1, high nibble = Joy-2 (idle 0xF). */
  PIA_PORT_input[0] = (uint8_t)(0xF0 | nib);
  /* GTIA_TRIG[0]: active-low (0 = pressed). */
  GTIA_TRIG[0] = fire ? 0 : 1;
}

} /* namespace input_port */
```

- [ ] **Step 3: Auto-detect mode on successful file load**

After any successful loader call, set the input mode from the selected filename:

```cpp
  int ok = load_selected_file(vfs_path);
  if (ok) {
    mode_autodetect_for(path);
    Serial.printf("input: mode = %s\n",
                  mode_current() == MODE_JOYSTICK ? "joystick" : "keyboard");
  }
  return ok;
```

Add the include at the top of `main.cpp`:

```cpp
extern "C" {
#include "input/mode.h"
}
```

In `on_input_action`, wire the toggle:

```cpp
    case KM_ACT_TOGGLE_INPUT_MODE:
      mode_toggle();
      Serial.printf("input: mode = %s\n",
                    mode_current() == MODE_JOYSTICK ? "joystick" : "keyboard");
      break;
```

- [ ] **Step 4: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 5: HUMAN CHECKPOINT — joystick mode moves a game's player**

Prep: you need a `.xex` that uses joystick input. Suggestion: a small paddle/spaceship PD demo. If you have no such file, search "atari 8-bit joystick xex download" (lots of classic demos are 8-bit-era freebies). Drop as `/sd/atari800/test.xex`.

Bump splash to `v0.3-m3-t7`. Flash.

Expected:
- Boot triggers xex load; serial: `input: mode = joystick`.
- Game starts. Press `E`, `S`, `A`, `D` — sprite should move (spacing depends on game).
- Press `K` — fire.
- Press `;`, `.`, `,`, `/` on the right cluster — same sprite moves.
- Press `Fn+J` — serial says `input: mode = keyboard`. Game probably freezes movement (stick is idle in keyboard mode). Type at the BASIC-style console and it should reach the game.
- Press `Fn+J` again — back to joystick.

Do not proceed until sprite movement via BOTH clusters confirmed and `Fn+J` toggle confirmed.

- [ ] **Step 6: Commit**

```bash
git add src/input/input_port.cpp src/input/input_port.h src/main.cpp
git commit -m "input: dual-cluster joystick + mode auto-detect on xex load + Fn+J toggle

Cluster-1 (ESAD + KL) and cluster-2 (;.,/ + ZX) are OR'd into Joystick-1
every frame; idle when mode is MODE_KEYBOARD. mode_autodetect_for()
promotes xex boot to joystick, .atr/.bas fall back to keyboard.
PIA_PORT_input[0] and GTIA_TRIG[0] are the core-facing registers — confirmed
by the agent's atari800 header audit."
```

---

### Task 8: Scaffold `src/audio/` — POKEY glue module + host test

**Files:**
- Create: `src/audio/pokey_glue.h`
- Create: `src/audio/pokey_glue.c`
- Create: `test/test_pokey_glue.c`
- Modify: `test/CMakeLists.txt`

This is a thin C wrapper around `POKEYSND_Init` + `POKEYSND_Process`. Host-testable by mocking the atari800 symbols.

⚠️ Gotchas encoded here (per the agent's audit):
1. `POKEYSND_enable_new_pokey` defaults to TRUE in upstream but `mzpokeysnd.c` is NOT vendored — we MUST set it to FALSE before Init or the link will fail at runtime (call to NULL function pointer).
2. `POKEYSND_Process` takes `sndn` = number of SAMPLES (not bytes, not frames). For stereo-16-bit output, that means `num_channels × num_frames` samples; the function writes `2 × sndn` bytes.

- [ ] **Step 1: Host test**

`test/test_pokey_glue.c`:

```c
/* test_pokey_glue.c — verify our glue correctly parameterises POKEYSND_Init
   and safely handles the mono/stereo toggle. Uses a mock of the atari800
   POKEYSND_* symbols. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/audio/pokey_glue.h"

/* ---- Mock POKEYSND globals + functions ---- */
int  POKEYSND_enable_new_pokey = 1;   /* default-TRUE per upstream */
int  POKEYSND_stereo_enabled   = 0;
unsigned char POKEYSND_num_pokeys = 1;
int  POKEYSND_snd_flags        = 0;
long POKEYSND_playback_freq    = 44100;

static int mock_init_called_with_freq = 0;
static int mock_init_called_with_num_pokeys = 0;
static int mock_init_called_with_flags = 0;
static int mock_init_return_val = 1;

int POKEYSND_Init(unsigned long freq17, int playback_freq,
                  unsigned char num_pokeys, int flags) {
  (void)freq17;
  mock_init_called_with_freq = playback_freq;
  mock_init_called_with_num_pokeys = num_pokeys;
  mock_init_called_with_flags = flags;
  return mock_init_return_val;
}

static int process_total_samples = 0;
void POKEYSND_Process(void* buf, int sndn) {
  (void)buf;
  process_total_samples += sndn;
}

/* ---- Tests ---- */
static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Mono init */
  int ok = pokey_glue_init(44100, 0 /* stereo=false */);
  CHECK(ok == 1, "init mono success");
  CHECK(POKEYSND_enable_new_pokey == 0, "enable_new_pokey forced to 0");
  CHECK(mock_init_called_with_freq == 44100, "freq passed through");
  CHECK(mock_init_called_with_num_pokeys == 1, "mono -> num_pokeys=1");
  CHECK((mock_init_called_with_flags & 0x01) != 0, "flags has BIT16");

  /* Stereo init */
  ok = pokey_glue_init(44100, 1 /* stereo=true */);
  CHECK(ok == 1, "init stereo success");
  CHECK(mock_init_called_with_num_pokeys == 2, "stereo -> num_pokeys=2");
  CHECK(POKEYSND_stereo_enabled == 1, "stereo flag set");

  /* Init failure propagates */
  mock_init_return_val = 0;
  ok = pokey_glue_init(44100, 0);
  CHECK(ok == 0, "init failure propagates");
  mock_init_return_val = 1;

  /* Process: 441 frames stereo = 882 samples */
  int16_t buf[882];
  pokey_glue_fill(buf, 441, 1);
  CHECK(process_total_samples == 882, "stereo 441 frames -> 882 samples");

  process_total_samples = 0;
  pokey_glue_fill(buf, 441, 0);
  CHECK(process_total_samples == 441, "mono 441 frames -> 441 samples");

  if (fail) return EXIT_FAILURE;
  printf("PASS: pokey_glue\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Create `src/audio/pokey_glue.h`**

```c
/* pokey_glue.h — thin wrapper over atari800's POKEYSND_* audio engine.

   Handles the two non-obvious gotchas:
   1. POKEYSND_enable_new_pokey must be FALSE because mzpokeysnd.c is NOT
      vendored. Setting this implicitly selects the classic Ron Fries code
      path inside pokeysnd.c.
   2. POKEYSND_Process's 'sndn' parameter is SAMPLES, not frames. For stereo,
      that's 2x the frame count. This wrapper converts for you. */

#ifndef CARDPUTER_POKEY_GLUE_H
#define CARDPUTER_POKEY_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize POKEY audio.
   playback_freq: Hz (typical 44100).
   stereo: 0 = single POKEY (mono output), 1 = dual POKEY (stereo L/R).
   Returns 1 on success, 0 on failure. */
int pokey_glue_init(int playback_freq, int stereo);

/* Fill `buf` with `frames` audio frames. `stereo` MUST match the value
   passed to pokey_glue_init. Output is 16-bit signed PCM interleaved
   (L R L R ... for stereo). */
void pokey_glue_fill(int16_t* buf, int frames, int stereo);

/* Reset the stereo bit WITHOUT re-initialising — cheap path for the
   menu toggle. Re-Init is the safe one if you need a full reset. */
void pokey_glue_set_stereo(int stereo);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: Create `src/audio/pokey_glue.c`**

```c
#include "pokey_glue.h"

/* Declared in atari800's pokeysnd.h; re-declared here so this module can be
   host-built with the mock test rig without pulling in all of pokeysnd.h. */
extern int          POKEYSND_enable_new_pokey;
extern int          POKEYSND_stereo_enabled;
extern unsigned char POKEYSND_num_pokeys;
extern int          POKEYSND_snd_flags;
extern long         POKEYSND_playback_freq;

extern int  POKEYSND_Init(unsigned long freq17, int playback_freq,
                          unsigned char num_pokeys, int flags);
extern void POKEYSND_Process(void* buf, int sndn);

/* POKEYSND_FREQ_17_APPROX (pokeysnd.h:63) — even-dividing 1.79 MHz clock. */
#define FREQ_17_APPROX 1787520UL

/* POKEYSND_BIT16 (pokeysnd.h). */
#define BIT16 0x01

int pokey_glue_init(int playback_freq, int stereo) {
  /* Step 1: mzpokeysnd is absent — force the classic path. */
  POKEYSND_enable_new_pokey = 0;
  POKEYSND_stereo_enabled   = stereo ? 1 : 0;

  int num_pokeys = stereo ? 2 : 1;
  int flags      = BIT16;
  int ok = POKEYSND_Init(FREQ_17_APPROX, playback_freq,
                         (unsigned char)num_pokeys, flags);
  return ok;
}

void pokey_glue_fill(int16_t* buf, int frames, int stereo) {
  int samples = frames * (stereo ? 2 : 1);
  POKEYSND_Process(buf, samples);
}

void pokey_glue_set_stereo(int stereo) {
  POKEYSND_stereo_enabled = stereo ? 1 : 0;
  POKEYSND_num_pokeys     = stereo ? 2 : 1;
  /* NOTE: changing num_pokeys without re-Init leaves the internal filter
     state mid-compute. Use pokey_glue_init(freq, stereo) if you want a
     clean swap. This lightweight path is fine when the menu toggle is
     followed by a brief silence (audio::pause)+resume. */
}
```

- [ ] **Step 4: Add to CMakeLists**

```cmake
add_executable(test_pokey_glue test_pokey_glue.c ../src/audio/pokey_glue.c)
add_test(NAME pokey_glue COMMAND test_pokey_glue)
```

- [ ] **Step 5: Build + test (host-only)**

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R pokey_glue
```

Expected: `PASS: pokey_glue`.

Do NOT try `pio run -e cardputer-adv` yet — the device build will fail until we wire sound into the core in T9. We'll test the device build there.

- [ ] **Step 6: Commit**

```bash
git add src/audio/pokey_glue.h src/audio/pokey_glue.c test/test_pokey_glue.c test/CMakeLists.txt
git commit -m "audio: POKEY init/process glue (forces non-MZ path) + host test"
```

---

### Task 9: Enable SOUND in core, provide Sound_* stubs

**Files:**
- Modify: `lib/atari800/src/config.h`
- Modify: `src/port_impl.cpp`

Turn on `#define SOUND` in the core so `POKEY_Update` stops being a no-op. The core expects `Sound_Initialise/Update/Pause/Continue/Exit` to exist (declared in `sound.h`, previously satisfied by the M2 build because `SOUND` was undef). We add minimal implementations in `port_impl.cpp` — the actual audio pump lives in `audio_out.cpp` (T10).

- [ ] **Step 1: Enable SOUND in config**

In `lib/atari800/src/config.h`, find the block:

```c
#undef SOUND                  /* M3 will enable */
```

Replace with:

```c
#define SOUND
#undef  STEREO_SOUND
#undef  SERIO_SOUND
#undef  CONSOLE_SOUND
#undef  SYNCHRONIZED_SOUND
#undef  SOUND_THIN_API   /* we do NOT use the thin-API path — that would
                            require a PLATFORM_SoundSetup we don't have */
```

`STEREO_SOUND` stays undefined — we implement dual-POKEY by flipping `POKEYSND_num_pokeys` at runtime, not at compile time.

- [ ] **Step 2: Add Sound_* stubs to `port_impl.cpp`**

In `src/port_impl.cpp`, append inside the existing `extern "C" { ... }` block (just before the closing brace on line 209):

```cpp
/* --- sound.h stubs — real I2S pump is in audio_out.cpp (T10).
   These are placeholders so the core links with SOUND enabled; they do
   nothing at first, then audio_out.cpp overrides Sound_Update weakly. */

int  Sound_enabled = 1;

int Sound_Initialise(int* argc, char* argv[]) {
  (void)argc; (void)argv;
  Serial.println("Sound_Initialise (stub — real init in audio_out)");
  return 1;
}
void Sound_Exit(void) {}

__attribute__((weak))
void Sound_Update(void) {
  /* Overridden by audio_out.cpp in T10. Weak to keep linkage clean. */
}

void Sound_Pause(void)    {}
void Sound_Continue(void) {}
```

- [ ] **Step 3: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS. Flash goes up by a few KB (pokeysnd.c's classic path pulls in ~4 KB of generated code). Heap stays the same.

If the build fails with `undefined reference to POKEYSND_Update`, the core's `POKEY_Update` macro path is still wrong — verify `pokey.c:181-183` guards on `#ifdef SOUND` and that our `config.h` now defines it.

- [ ] **Step 4: Verify all host tests still pass**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: 6/6 pass (sanity + palette + projector + loader + keymap + joystick + mode + pokey_glue = 8 actually — count grows each task).

- [ ] **Step 5: Commit**

```bash
git add lib/atari800/src/config.h src/port_impl.cpp
git commit -m "core: enable #define SOUND; Sound_Update is weak so T10 can override

Also: keep STEREO_SOUND undef'd — dual-POKEY is a runtime flag, not a
compile-time switch, because M3's audio output is always a stereo frame
(duplicated mono or true dual-POKEY). SOUND_THIN_API path is not used."
```

---

### Task 10: Audio output — Speaker init + two-buffer pump

**Files:**
- Create: `src/audio/audio_out.cpp`
- Modify: `src/main.cpp`

Call `M5Cardputer.Speaker.begin()` after the heap-pinned allocations but BEFORE `M5Cardputer.begin()` has eaten the heap further. Pre-allocate two POKEY buffers (441 frames × 2 channels × 2 bytes = 1764 bytes each). Pump per frame via `playRaw` alternation, per `Speaker_Class.hpp:175-176`'s documented pattern.

⚠️ Heap ordering: the I²S DMA buffers (~4 KB) AND the FreeRTOS task stack (~2.3 KB) are allocated inside `Speaker.begin()`, NOT in our code. They need to fit in the heap AT THAT MOMENT. Our setup() ordering becomes:

1. Pre-allocate `Screen_atari` (92 KB)
2. `ensure_memory_mem_allocated()` (65 KB)
3. `ensure_under_buffers_allocated()` (32 KB)
4. Pre-allocate POKEY output buffers (2 × 1764 B = 3528 B for stereo)
5. `mount_sd()` (~27 KB FATFS)
6. `M5Cardputer.begin()` (~200 KB)
7. `M5Cardputer.Speaker.config({...}); M5Cardputer.Speaker.begin()` (~7 KB)

Speaker.begin at step 7 uses post-M5-init heap. Try to pick `dma_buf_count=4` in config to keep the I²S ring small.

- [ ] **Step 1: Create `src/audio/audio_out.cpp`**

```cpp
/* audio_out.cpp — I2S speaker pump fed from POKEY.
   Strategy: two fixed-size int16_t buffers, alternated via playRaw. The
   frame-loop side calls audio_out::pump() once per Atari frame; that
   generates one buffer worth of POKEY output and queues it. The speaker
   task drains it via DMA. */

#include <M5Cardputer.h>
#include <stdint.h>
#include <stddef.h>

extern "C" {
#include "pokey_glue.h"
}

namespace {

constexpr int SAMPLE_RATE  = 44100;
constexpr int FRAMES_PER_BUFFER = 882;   /* 20 ms of audio at 44.1 kHz */

bool g_stereo = true;
bool g_muted  = false;

/* Two buffers pre-allocated at setup time. Size for stereo: 882 frames *
   2 channels * 2 bytes = 3528 bytes each, total 7056 bytes. Mono would be
   1764 + 1764. Allocate as stereo-sized and use half for mono. */
int16_t* g_buf[2] = { nullptr, nullptr };
uint8_t  g_flip   = 0;

/* Virtual channel assigned by the speaker's internal round-robin. We pin
   it to 0 so isPlaying() tracks the right channel. */
constexpr int AUDIO_CHANNEL = 0;

} /* anon namespace */

namespace audio_out {

/* Must be called EARLY in setup() — before M5Cardputer.begin() — so the
   buffer malloc lands in a known heap state. Returns true on success. */
bool preallocate_buffers() {
  size_t bytes = FRAMES_PER_BUFFER * 2 /* stereo */ * sizeof(int16_t);
  g_buf[0] = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  g_buf[1] = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_buf[0] || !g_buf[1]) return false;
  memset(g_buf[0], 0, bytes);
  memset(g_buf[1], 0, bytes);
  return true;
}

/* Must be called AFTER M5Cardputer.begin(). Configures the I²S driver,
   sets sample rate + stereo + small DMA buffer count, then begin()s.
   This allocates ~7 KB (DMA buffers + FreeRTOS task stack). */
bool start(int initial_stereo) {
  auto cfg = M5Cardputer.Speaker.config();
  cfg.sample_rate  = SAMPLE_RATE;
  cfg.stereo       = true;                /* always true at I²S level; we
                                             duplicate mono into L+R when
                                             g_stereo is false */
  cfg.dma_buf_count = 4;                  /* default is 8; halve to save 4 KB */
  cfg.dma_buf_len   = 256;                /* 5.8 ms per DMA buf */
  M5Cardputer.Speaker.config(cfg);
  bool ok = M5Cardputer.Speaker.begin();
  if (!ok) return false;

  g_stereo = (initial_stereo != 0);

  /* Init POKEY engine. Must happen AFTER Atari800_Initialise so pokeysnd's
     internal state is set up; main.cpp ensures this ordering. */
  if (!pokey_glue_init(SAMPLE_RATE, g_stereo)) return false;

  M5Cardputer.Speaker.setVolume(128);     /* mid */
  return true;
}

void set_muted(bool muted) {
  g_muted = muted;
  M5Cardputer.Speaker.setVolume(muted ? 0 : 128);
}

void set_stereo(bool stereo) {
  if (stereo == g_stereo) return;
  /* Re-init POKEY cleanly to avoid filter-state artifacts. */
  pokey_glue_init(SAMPLE_RATE, stereo ? 1 : 0);
  g_stereo = stereo;
}

void set_volume_delta(int8_t delta) {
  int v = (int)M5Cardputer.Speaker.getVolume() + (int)delta;
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  M5Cardputer.Speaker.setVolume((uint8_t)v);
}

/* Call once per Atari frame (50 Hz PAL, 60 Hz NTSC).
   882 frames / 44100 Hz = 20 ms — matches PAL frame exactly. For NTSC
   (16.7 ms/frame) we under-fill slightly; the Speaker task smooths with
   the DMA ring, so small rate mismatches bleed off as drift, not clicks. */
void pump() {
  if (g_muted) return;

  /* Avoid blocking: if both slots full, skip this frame — the DMA ring is
     already full, audio is already flowing. A skipped pump() just means
     one 20 ms window of POKEY state isn't rendered; on the next frame
     we'll catch up from a fresh POKEY register state. */
  if (M5Cardputer.Speaker.isPlaying(AUDIO_CHANNEL) >= 2) return;

  int16_t* b = g_buf[g_flip];
  pokey_glue_fill(b, FRAMES_PER_BUFFER, g_stereo ? 1 : 0);

  if (!g_stereo) {
    /* Expand mono samples in-place: input is 882 samples in first half;
       walk backwards writing [2i]=src, [2i+1]=src to make stereo. */
    for (int i = FRAMES_PER_BUFFER - 1; i >= 0; i--) {
      int16_t s = b[i];
      b[2 * i]     = s;
      b[2 * i + 1] = s;
    }
  }

  M5Cardputer.Speaker.playRaw(b,
                              FRAMES_PER_BUFFER * 2 /* stereo samples */,
                              SAMPLE_RATE,
                              true /* stereo */,
                              1    /* repeat */,
                              AUDIO_CHANNEL,
                              false /* do not stop current */);
  g_flip ^= 1;
}

} /* namespace audio_out */

/* Override the weak Sound_Update stub from port_impl.cpp. The core calls
   Sound_Update() inside Atari800_Frame() after each frame has been
   generated — natural hook for pumping audio. */
extern "C" void Sound_Update(void) {
  audio_out::pump();
}
```

- [ ] **Step 2: Add public header for main.cpp**

`src/audio/audio_out.h`:

```cpp
#pragma once
#include <stdint.h>

namespace audio_out {
  bool preallocate_buffers();             /* call EARLY in setup() */
  bool start(int initial_stereo);         /* call AFTER M5Cardputer.begin() */
  void set_muted(bool muted);
  void set_stereo(bool stereo);
  void set_volume_delta(int8_t delta);
  void pump();
}
```

- [ ] **Step 3: Wire into `main.cpp`**

At the top, add:

```cpp
#include "audio/audio_out.h"
```

In `setup()`, after `ensure_under_buffers_allocated()` but BEFORE `mount_sd()`, add the audio buffer pre-alloc:

```cpp
  // Audio buffers: two stereo 882-frame buffers (3528 bytes each).
  // Slotted before SD-mount like the other big contiguous needs; once
  // FATFS chews ~27 KB of fragments, these would still fit (they're only
  // ~3.5 KB each) but we'd rather not race with it.
  if (!audio_out::preallocate_buffers()) {
    Serial.println("audio: preallocate_buffers FAILED");
  } else {
    Serial.println("audio: buffers pre-allocated");
  }
  Serial.printf("heap@post-audio-prealloc: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
```

In `setup()`, after `Atari800_Initialise()` and the splash/SD-list, add the Speaker start:

```cpp
  // Audio: Speaker.begin() happens AFTER core init so pokeysnd is ready
  // to produce samples when our pump kicks in on the first frame.
  if (audio_out::start(0 /* mono (single POKEY) by default */)) {
    Serial.println("audio: started (mono)");
  } else {
    Serial.println("audio: start FAILED");
  }
  Serial.printf("heap@post-audio-start: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
```

- [ ] **Step 4: Volume hook in `on_input_action`**

In `on_input_action` in `main.cpp`, replace the volume cases:

```cpp
    case KM_ACT_VOLUME_DOWN:
      audio_out::set_volume_delta(-16);
      Serial.printf("volume: %u\n", M5Cardputer.Speaker.getVolume());
      break;
    case KM_ACT_VOLUME_UP:
      audio_out::set_volume_delta(+16);
      Serial.printf("volume: %u\n", M5Cardputer.Speaker.getVolume());
      break;
```

- [ ] **Step 5: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS. Flash usage jumps ~15-25 KB (pokeysnd's classic synth path is the biggest new code).

- [ ] **Step 6: HUMAN CHECKPOINT — sound comes out**

Bump splash to `v0.3-m3-t10`. Flash. Have headphones ready (or confirm the built-in speaker is enabled — it should be by default on Cardputer-Adv).

Expected:
- Boot into BASIC (or your xex). **Audible click** when the core first writes to POKEY registers (AltirraBASIC emits a startup ding; some xex programs do too).
- Type `SOUND 0,121,10,15` in BASIC — **a pure tone plays**.
- `SOUND 0,0,0,0` silences it.
- `Fn+-` lowers volume, `Fn+=` raises it. Each press should be audibly different.

If no sound:
- Check `Serial` for `audio: started (mono)`. If missing, preallocate or start failed.
- Check volume isn't 0.
- Probe: `POKEYSND_playback_freq` should report 44100 if you printf it.
- Crackling/stuttering: the frame-pacing is off. Check that `loop()` is still running every 20 ms; `Serial` heartbeat should fire normally.

Do not proceed until a BASIC `SOUND` command produces audible output.

- [ ] **Step 7: Commit**

```bash
git add src/audio/audio_out.cpp src/audio/audio_out.h src/main.cpp
git commit -m "audio: M5 Speaker.begin + two-buffer pump fed from POKEYSND_Process

Pre-allocate buffers early in setup(), start Speaker after M5.begin() and
Atari800_Initialise, override weak Sound_Update with audio_out::pump().
Volume keys (Fn+-/=) cycle setVolume in ±16 steps. Mono today; dual-POKEY
toggle is T16. DMA: 4 bufs × 256 frames (~4 KB), half of M5 default."
```

---

### Task 11: Mute on reset (and idle when core isn't producing)

**Files:**
- Modify: `src/audio/audio_out.cpp`
- Modify: `src/main.cpp`

Reset (`Fn+5` cold / `Fn+4` warm) transiently re-inits POKEY. Without mute, you hear a loud click. Mute before reset, unmute on the next frame.

- [ ] **Step 1: Wrap reset in `on_input_action`**

```cpp
    case KM_ACT_WARM_RESET:
      Serial.println("action: warm reset");
      audio_out::set_muted(true);
      Atari800_Warmstart();
      audio_out::set_muted(false);
      break;
    case KM_ACT_COLD_RESET:
      Serial.println("action: cold reset");
      audio_out::set_muted(true);
      Atari800_Coldstart();
      audio_out::set_muted(false);
      break;
```

- [ ] **Step 2: Build + HUMAN CHECKPOINT (quick)**

Flash (splash v0.3-m3-t11). Play a `SOUND 0,121,10,15` in BASIC, then press Fn+5 — should NOT hear a loud transient. If you still hear a click, it's the DMA buffer draining the last 20 ms of state; that's not a bug to fix here.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "audio: mute during warm/cold reset to squash POKEY re-init click"
```

---

### Task 12: Settings module — in-memory struct + apply-to-core

**Files:**
- Create: `src/settings/settings.h`
- Create: `src/settings/settings.c`
- Create: `test/test_settings.c`
- Modify: `test/CMakeLists.txt`

The struct captures region / machine / audio / display state. `settings_apply()` sequences the atari800 core calls needed to actually change the running emulator. Persistence to SD is M4.

- [ ] **Step 1: Host test first**

`test/test_settings.c`:

```c
/* test_settings.c — settings struct defaults + validation.
   Core calls are mocked; we just verify sequencing + value passing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/settings/settings.h"

/* ---- Mocks for atari800 core ---- */
int Atari800_machine_type = 1; /* XLXE default */
int Atari800_tv_mode      = 312; /* PAL */
int MEMORY_ram_size       = 64;
int Atari800_builtin_basic = 1;
int Atari800_builtin_game  = 0;
int Atari800_keyboard_leds = 0;
int Atari800_f_keys        = 0;
int Atari800_jumper        = 0;
int Atari800_keyboard_detached = 0;

static int mock_set_machine_calls = 0;
static int mock_set_tv_calls = 0;
static int mock_init_machine_calls = 0;
static int mock_coldstart_calls = 0;

void Atari800_SetMachineType(int t) { Atari800_machine_type = t; mock_set_machine_calls++; }
void Atari800_SetTVMode(int m)      { Atari800_tv_mode = m; mock_set_tv_calls++; }
int  Atari800_InitialiseMachine(void) { mock_init_machine_calls++; return 1; }
void Atari800_Coldstart(void)       { mock_coldstart_calls++; }

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  settings_t s;
  settings_load_defaults(&s);

  /* Defaults per spec 1.1 / 1.3: 65XE + PAL + BASIC on */
  CHECK(s.model  == SETTINGS_MODEL_65XE, "default model 65XE");
  CHECK(s.region == SETTINGS_REGION_PAL, "default region PAL");
  CHECK(s.boot_basic == 1,               "default basic on");
  CHECK(s.dual_pokey == 0,               "default dual_pokey off");
  CHECK(s.input_mode_auto == 1,          "default input_mode auto");

  /* Apply 130XE + NTSC */
  mock_set_machine_calls = mock_set_tv_calls = mock_init_machine_calls = mock_coldstart_calls = 0;
  s.model = SETTINGS_MODEL_130XE;
  s.region = SETTINGS_REGION_NTSC;
  settings_apply(&s);

  CHECK(Atari800_machine_type == 1 /* XLXE */,  "130XE -> XLXE enum");
  CHECK(MEMORY_ram_size == 128,                  "130XE -> 128 KB");
  CHECK(Atari800_tv_mode == 262 /* NTSC */,      "region NTSC -> 262");
  CHECK(mock_init_machine_calls == 1,            "InitialiseMachine called once");
  CHECK(mock_coldstart_calls == 1,               "Coldstart called after Init");

  /* XEGS: XLXE + 64 KB + builtin_game */
  settings_load_defaults(&s);
  s.model = SETTINGS_MODEL_XEGS;
  settings_apply(&s);
  CHECK(Atari800_builtin_game == 1, "XEGS -> builtin_game=1");
  CHECK(MEMORY_ram_size == 64,      "XEGS -> 64 KB");

  /* 800XL: XLXE + 64 KB + no builtin_game */
  settings_load_defaults(&s);
  s.model = SETTINGS_MODEL_800XL;
  settings_apply(&s);
  CHECK(Atari800_builtin_game == 0, "800XL -> builtin_game=0");

  if (fail) return EXIT_FAILURE;
  printf("PASS: settings\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Create `src/settings/settings.h`**

```c
/* settings.h — in-memory settings for the running emulator.
   Persistence (writing these to /atari800/config/atari800.cfg) is M4. */

#ifndef CARDPUTER_SETTINGS_H
#define CARDPUTER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SETTINGS_MODEL_800XL = 0,
  SETTINGS_MODEL_65XE  = 1,
  SETTINGS_MODEL_130XE = 2,
  SETTINGS_MODEL_XEGS  = 3,
  SETTINGS_MODEL_COUNT
} settings_model_t;

typedef enum {
  SETTINGS_REGION_PAL  = 0,
  SETTINGS_REGION_NTSC = 1
} settings_region_t;

typedef struct {
  settings_model_t  model;
  settings_region_t region;
  int boot_basic;           /* 1 = boot with BASIC (default 1 for 65XE) */
  int dual_pokey;           /* 1 = true stereo (two POKEYs), 0 = mono->L+R */
  int input_mode_auto;      /* 1 = auto-detect from file ext, 0 = force */
  /* volume / brightness are held by the hardware subsystems; not in settings */
} settings_t;

/* Populate `s` with the spec's first-boot defaults. */
void settings_load_defaults(settings_t* s);

/* Apply `s` to the running atari800 core. Sequence:
     Atari800_SetMachineType(XLXE)
     MEMORY_ram_size = model's RAM
     Atari800_builtin_basic = s->boot_basic
     Atari800_builtin_game  = (model == XEGS)
     Atari800_SetTVMode(PAL|NTSC)
     Atari800_InitialiseMachine()
     Atari800_Coldstart()

   Caller is responsible for muting audio + pausing rendering around this
   call; it resets the machine. */
void settings_apply(const settings_t* s);

/* Pretty label for a model enum value. */
const char* settings_model_label(settings_model_t m);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: Create `src/settings/settings.c`**

```c
#include "settings.h"

extern int Atari800_machine_type;
extern int Atari800_tv_mode;
extern int MEMORY_ram_size;
extern int Atari800_builtin_basic;
extern int Atari800_builtin_game;
extern int Atari800_keyboard_leds;
extern int Atari800_f_keys;
extern int Atari800_jumper;
extern int Atari800_keyboard_detached;

extern void Atari800_SetMachineType(int);
extern void Atari800_SetTVMode(int);
extern int  Atari800_InitialiseMachine(void);
extern void Atari800_Coldstart(void);

#define ATARI800_MACHINE_XLXE  1
#define ATARI800_TV_PAL        312
#define ATARI800_TV_NTSC       262

void settings_load_defaults(settings_t* s) {
  s->model           = SETTINGS_MODEL_65XE;
  s->region          = SETTINGS_REGION_PAL;
  s->boot_basic      = 1;
  s->dual_pokey      = 0;
  s->input_mode_auto = 1;
}

void settings_apply(const settings_t* s) {
  /* XL/XE family tuple per atari.c:488-556 (agent's audit).
     Only the fields that differ across our 4 models need to be set. */
  int ram_size     = 64;
  int builtin_game = 0;
  switch (s->model) {
    case SETTINGS_MODEL_800XL: ram_size = 64;  builtin_game = 0; break;
    case SETTINGS_MODEL_65XE:  ram_size = 64;  builtin_game = 0; break;
    case SETTINGS_MODEL_130XE: ram_size = 128; builtin_game = 0; break;
    case SETTINGS_MODEL_XEGS:  ram_size = 64;  builtin_game = 1; break;
    default: break;
  }

  Atari800_SetMachineType(ATARI800_MACHINE_XLXE);
  MEMORY_ram_size            = ram_size;
  Atari800_builtin_basic     = s->boot_basic ? 1 : 0;
  Atari800_builtin_game      = builtin_game;
  Atari800_keyboard_leds     = 0;    /* 1200XL-only; always 0 for our models */
  Atari800_f_keys            = 0;    /* 1200XL-only */
  Atari800_jumper            = 0;
  Atari800_keyboard_detached = 0;

  Atari800_SetTVMode(s->region == SETTINGS_REGION_PAL ? ATARI800_TV_PAL
                                                      : ATARI800_TV_NTSC);

  Atari800_InitialiseMachine();
  Atari800_Coldstart();
}

const char* settings_model_label(settings_model_t m) {
  switch (m) {
    case SETTINGS_MODEL_800XL: return "800XL";
    case SETTINGS_MODEL_65XE:  return "65XE";
    case SETTINGS_MODEL_130XE: return "130XE";
    case SETTINGS_MODEL_XEGS:  return "XEGS";
    default: return "?";
  }
}
```

- [ ] **Step 4: CMakeLists + test**

Append to `test/CMakeLists.txt`:

```cmake
add_executable(test_settings test_settings.c ../src/settings/settings.c)
add_test(NAME settings COMMAND test_settings)
```

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R settings
```

Expected: `PASS: settings`.

- [ ] **Step 5: Verify firmware still builds**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 6: Commit**

```bash
git add src/settings/settings.h src/settings/settings.c test/test_settings.c test/CMakeLists.txt
git commit -m "settings: in-memory machine/region/basic/dual-pokey struct + apply-to-core

Canonicalizes the XL/XE tuple (machine_type/ram_size/builtin_basic/builtin_game)
that atari.c's -xl/-xe/-xegs switches apply. No SD persistence yet (M4)."
```

---

### Task 13: Region + machine apply wired to `main.cpp`

**Files:**
- Modify: `src/main.cpp`

Add a module-level `g_settings` and wire the apply path. Visual before/after confirmation comes next task with the menu; here we just prove the plumbing compiles and the initial coldstart path hits settings_apply.

- [ ] **Step 1: Declare + init settings in main.cpp**

Near the top of `src/main.cpp` (after other includes):

```cpp
extern "C" {
#include "settings/settings.h"
}

/* File-scope (NOT static) — menu.cpp reaches this via extern in T15. */
settings_t g_settings;
```

In `setup()`, after `Atari800_Initialise()` succeeds but before `lcd::init()`:

```cpp
  settings_load_defaults(&g_settings);
  /* At this point Atari800_Initialise has already used its internal defaults
     (atari.c:154-164 — XLXE + PAL + BASIC). Our defaults match, so no
     re-apply is necessary on first boot. T14/T15 will apply on menu change. */

  // set renderer region to match
  renderer::set_region_ntsc(g_settings.region == SETTINGS_REGION_NTSC);
```

Replace the unconditional `renderer::set_region_ntsc(false)` that M2 added in `setup()` around line 255 — that becomes redundant.

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "main: wire settings_t default-load at boot; renderer region sourced from it"
```

---

### Task 14: Apply settings changes — reset + audio mute + renderer region

**Files:**
- Create: `src/settings/settings_apply_runtime.h` — helper for "apply and handle side effects"
- Create: `src/settings/settings_apply_runtime.cpp`
- Modify: `src/main.cpp`

`settings_apply()` is pure C (host-testable) and resets the atari800 core. In the firmware we also need to: mute audio, update renderer region, update `pokey_glue` stereo, coldstart once.

- [ ] **Step 1: Create runtime helper**

`src/settings/settings_apply_runtime.h`:

```cpp
#pragma once

extern "C" {
#include "settings.h"
}

namespace settings_runtime {
  /* Applies `s` to the core and handles firmware-level side effects:
     mute audio, update renderer region, update audio channel count
     (dual-POKEY), update xex auto-reload if the session had one. */
  void apply(const settings_t& s);
}
```

`src/settings/settings_apply_runtime.cpp`:

```cpp
#include "settings_apply_runtime.h"
#include "../audio/audio_out.h"
#include "../display/renderer.h"

namespace settings_runtime {

void apply(const settings_t& s) {
  audio_out::set_muted(true);

  settings_apply(&s);

  renderer::set_region_ntsc(s.region == SETTINGS_REGION_NTSC);
  audio_out::set_stereo(s.dual_pokey != 0);

  audio_out::set_muted(false);
}

} /* namespace settings_runtime */
```

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/settings/settings_apply_runtime.h src/settings/settings_apply_runtime.cpp
git commit -m "settings: runtime apply helper (audio mute + renderer region + stereo)"
```

---

### Task 15: Overlay menu — M4 follow-up

**Files:**
- Create: `src/ui/menu.h`
- Create: `src/ui/menu.cpp`
- Modify: `src/main.cpp`

Minimum viable menu per spec §7.3: modal overlay that dims the screen, presents a vertical list of items, navigates with cursor keys (AKEY_UP/DOWN) or Fn+; / Fn+., selects with Enter, dismisses with Esc. Items mutate `g_settings`; exiting re-applies via `settings_runtime::apply`.

The menu runs in the main loop as a state machine: `menu_open()` returns immediately; `menu_is_open()` returns true while open; the main loop pauses `Atari800_Frame()` and calls `menu_update()` + `menu_draw()` instead. Audio is muted while open.

Items for the later full menu:

```
▶ Resume
  Machine: <model>   →
  Region:  <region>
  Display: <mode>    →
  Input:   <mode>    →
  Dual POKEY: <on|off>
  ─────────
  Cold reset
  Exit to browser
```

- [ ] **Step 1: Create `src/ui/menu.h`**

```cpp
#pragma once
#include <stdint.h>

namespace menu {

void open();
void close();
bool is_open();

/* Call each frame while is_open(); handles navigation input. */
void update();

/* Call each frame while is_open(); draws the overlay on M5Cardputer.Display. */
void draw();

} /* namespace menu */
```

- [ ] **Step 2: Create `src/ui/menu.cpp`**

```cpp
#include "menu.h"
#include <M5Cardputer.h>
#include <stdio.h>

extern "C" {
#include "../settings/settings.h"
#include "../input/mode.h"
}
#include "../settings/settings_apply_runtime.h"
#include "../audio/audio_out.h"
#include "../input/input_port.h"
#include "../display/renderer.h"

/* g_settings is a file-scope variable in main.cpp (T13 removed the static).
   We read/write it here; settings_runtime::apply pushes it to the core. */
extern settings_t g_settings;

namespace menu {

namespace {

enum ItemId {
  ITEM_RESUME,
  ITEM_MACHINE,
  ITEM_REGION,
  ITEM_DISPLAY,
  ITEM_INPUT,
  ITEM_DUAL_POKEY,
  ITEM_SEPARATOR,
  ITEM_COLD_RESET,
  ITEM_COUNT
};

bool g_open = false;
int  g_cursor = 0;
/* Debounce nav keys — M5Cardputer reports key *state* not edge. We only
   advance on the transition from "not held" to "held". */
bool g_prev_up = false, g_prev_down = false, g_prev_enter = false, g_prev_esc = false;

void on_navigate_down() {
  for (int tries = 0; tries < ITEM_COUNT; tries++) {
    g_cursor = (g_cursor + 1) % ITEM_COUNT;
    if (g_cursor != ITEM_SEPARATOR) break;
  }
}

void on_navigate_up() {
  for (int tries = 0; tries < ITEM_COUNT; tries++) {
    g_cursor = (g_cursor - 1 + ITEM_COUNT) % ITEM_COUNT;
    if (g_cursor != ITEM_SEPARATOR) break;
  }
}

void cycle_machine() {
  int m = (int)g_settings.model + 1;
  if (m >= (int)SETTINGS_MODEL_COUNT) m = 0;
  g_settings.model = (settings_model_t)m;
}

void cycle_region() {
  g_settings.region = (g_settings.region == SETTINGS_REGION_PAL)
                      ? SETTINGS_REGION_NTSC : SETTINGS_REGION_PAL;
}

void cycle_display() {
  renderer::Mode m = renderer::get_mode();
  int next = ((int)m + 1) % 4;
  renderer::set_mode((renderer::Mode)next);
}

void cycle_input_mode() {
  /* 3-way: auto / keyboard / joystick. In M3 we simplify to keyboard/joystick
     since auto-detect is file-triggered (mode_autodetect_for). */
  mode_toggle();
}

void toggle_dual_pokey() {
  g_settings.dual_pokey = !g_settings.dual_pokey;
  audio_out::set_stereo(g_settings.dual_pokey != 0);
}

void activate(int item) {
  switch (item) {
    case ITEM_RESUME:     close(); break;
    case ITEM_MACHINE:    cycle_machine(); break;
    case ITEM_REGION:     cycle_region(); break;
    case ITEM_DISPLAY:    cycle_display(); break;
    case ITEM_INPUT:      cycle_input_mode(); break;
    case ITEM_DUAL_POKEY: toggle_dual_pokey(); break;
    case ITEM_COLD_RESET: extern "C" void Atari800_Coldstart(void);
                          Atari800_Coldstart(); close(); break;
    default: break;
  }
}

const char* item_label(int id, const settings_t& s, char* scratch, size_t n) {
  switch (id) {
    case ITEM_RESUME:   return "Resume";
    case ITEM_MACHINE: {
      snprintf(scratch, n, "Machine:   %s", settings_model_label(s.model));
      return scratch;
    }
    case ITEM_REGION: {
      snprintf(scratch, n, "Region:    %s",
               s.region == SETTINGS_REGION_PAL ? "PAL" : "NTSC");
      return scratch;
    }
    case ITEM_DISPLAY: {
      const char* dn;
      switch (renderer::get_mode()) {
        case renderer::Mode::PixelPerfect: dn = "Pixel-perfect"; break;
        case renderer::Mode::Pillarbox:    dn = "Pillarbox";     break;
        case renderer::Mode::Cover:        dn = "Cover";         break;
        default:                           dn = "Stretch";       break;
      }
      snprintf(scratch, n, "Display:   %s", dn);
      return scratch;
    }
    case ITEM_INPUT: {
      snprintf(scratch, n, "Input:     %s",
               mode_current() == MODE_JOYSTICK ? "Joystick" : "Keyboard");
      return scratch;
    }
    case ITEM_DUAL_POKEY: {
      snprintf(scratch, n, "Dual POKEY: %s", s.dual_pokey ? "on" : "off");
      return scratch;
    }
    case ITEM_SEPARATOR:  return "---";
    case ITEM_COLD_RESET: return "Cold reset";
    default: return "?";
  }
}

} /* anon namespace */

void open() {
  g_open = true;
  g_cursor = ITEM_RESUME;
  audio_out::set_muted(true);
}

void close() {
  g_open = false;
  audio_out::set_muted(false);
  /* Apply machine/region changes if the user touched those items — the safe
     bet is to always apply on close, since apply() is idempotent and only
     costs one Coldstart when nothing changed is cheap enough at 50 Hz. */
  settings_runtime::apply(g_settings);
}

bool is_open() { return g_open; }

void update() {
  auto& ks = M5Cardputer.Keyboard.keysState();

  /* Cursor nav: up/down via Fn+; / Fn+. — but while menu is open we also
     accept the plain ; . keys (no Fn chord needed — easier to reach). */
  bool up    = false, down = false, enter = ks.enter, esc = false;
  for (char c : ks.word) {
    if (c == ';') up = true;
    if (c == '.') down = true;
    if (c == 0x1B /* ASCII ESC */) esc = true;
  }

  /* Debounce: only fire on rising edge. */
  if (up    && !g_prev_up)    on_navigate_up();
  if (down  && !g_prev_down)  on_navigate_down();
  if (enter && !g_prev_enter) activate(g_cursor);
  if (esc   && !g_prev_esc)   close();

  g_prev_up    = up;
  g_prev_down  = down;
  g_prev_enter = enter;
  g_prev_esc   = esc;
}

void draw() {
  auto& d = M5Cardputer.Display;
  /* Dim overlay: fill a semi-transparent rect. M5GFX doesn't do alpha — we
     just use a dark color to replace the pixel. */
  d.fillRect(0, 0, 240, 135, 0x1082 /* RGB565 dark blue-grey */);

  d.setTextSize(1);
  d.setCursor(8, 8);
  d.setTextColor(0xFFFF, 0x1082);
  d.print("-- MENU --");

  char scratch[32];
  for (int i = 0; i < ITEM_COUNT; i++) {
    int y = 24 + i * 12;
    if (i == ITEM_SEPARATOR) {
      d.drawLine(16, y + 4, 224, y + 4, 0x7BEF);
      continue;
    }
    const char* label = item_label(i, g_settings, scratch, sizeof(scratch));
    d.setCursor(24, y);
    if (i == g_cursor) {
      d.setTextColor(0x1082, 0xFFE0 /* yellow bg */);
      d.fillRect(16, y - 1, 208, 11, 0xFFE0);
    } else {
      d.setTextColor(0xFFFF, 0x1082);
    }
    d.print(label);
  }
}

} /* namespace menu */
```

- [ ] **Step 3: Wire Fn+8 to open the menu, gate the frame loop while it's open**

In `src/main.cpp`, add includes:

```cpp
#include "ui/menu.h"
```

In `on_input_action`, replace the `KM_ACT_MENU_OPEN` branch:

```cpp
    case KM_ACT_MENU_OPEN:
      menu::open();
      break;
```

In `loop()`, wrap the existing "frame pacing + Atari800_Frame + renderer::present" block:

```cpp
  static uint32_t last_frame_ms = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) {
    last_frame_ms = now;
    input_port::poll();                  /* always — so Fn+8 can be seen */

    if (menu::is_open()) {
      menu::update();
      menu::draw();
    } else {
      Atari800_Frame();
      renderer::present(reinterpret_cast<const uint8_t*>(Screen_atari));
    }
  }
```

- [ ] **Step 4: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS. If `mode_name_wrapper` produces a linker error (the earlier snippet is illustrative), drop that line from menu.cpp — use the local switch statement directly.

- [ ] **Step 5: HUMAN CHECKPOINT — menu opens and navigates**

Bump splash to `v0.3-m3-t15`. Flash.

Expected:
- Boot into BASIC or xex.
- Press `Fn+8` — screen dims; a list appears with "▶ Resume" highlighted.
- Press `.` or Fn+. — cursor moves down one item.
- Press `;` or Fn+; — cursor moves up.
- Navigate to "Region: PAL" — press `Enter` — now "NTSC". The core has coldstarted; screen may briefly flash, then BASIC's READY reappears.
- Navigate to "Machine: 65XE" — press `Enter` — cycles through 130XE / XEGS / 800XL / 65XE.
- Press `Esc` — menu closes, emulator resumes. Changes persist.

Do not proceed until the menu opens, Enter activates at least one cycle-type item, and Esc closes cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/ui/menu.h src/ui/menu.cpp src/main.cpp
git commit -m "ui: Fn+8 overlay menu — machine/region/display/input/dual-pokey + cold reset

Minimal text-list menu that mutates g_settings, applies on close, and
swallows the frame loop while open (audio muted, no Atari800_Frame calls)."
```

---

### Task 16: OSD transient overlay (volume, brightness, mode)

**Files:**
- Create: `src/ui/osd.h`
- Create: `src/ui/osd.cpp`
- Modify: `src/main.cpp`

Corner text that auto-fades. Not persistent state — just visual feedback for Fn+\\ / Fn+[/]/Fn+-/= and the future "SAVED" / "LOADED" M5 stubs.

- [ ] **Step 1: Create `src/ui/osd.h`**

```cpp
#pragma once

namespace osd {
  /* Show `text` for `duration_ms` in the top-right corner.
     Overwrites any existing message. */
  void show(const char* text, uint32_t duration_ms = 2000);

  /* Called once per frame from main.cpp — draws/erases the text if active. */
  void tick();
}
```

- [ ] **Step 2: Create `src/ui/osd.cpp`**

```cpp
#include "osd.h"
#include <M5Cardputer.h>
#include <string.h>

namespace osd {

namespace {
char g_text[32] = {0};
uint32_t g_until_ms = 0;
bool g_drawn = false;
}

void show(const char* text, uint32_t duration_ms) {
  strncpy(g_text, text, sizeof(g_text) - 1);
  g_text[sizeof(g_text) - 1] = 0;
  g_until_ms = millis() + duration_ms;
  g_drawn = false;   /* force redraw on next tick */
}

void tick() {
  auto& d = M5Cardputer.Display;
  uint32_t now = millis();
  bool should_show = g_text[0] && now < g_until_ms;

  if (should_show && !g_drawn) {
    d.fillRect(120, 2, 118, 12, 0x0000);       /* black backing */
    d.setTextSize(1);
    d.setCursor(124, 4);
    d.setTextColor(0xFFE0, 0x0000);             /* yellow on black */
    d.print(g_text);
    g_drawn = true;
  } else if (!should_show && g_drawn) {
    d.fillRect(120, 2, 118, 12, 0x0000);
    g_drawn = false;
    g_text[0] = 0;
  }
  /* else: already drawn or not showing — no-op */
}
```

Note: OSD uses a fixed 118×12 px strip in the top-right; content underneath is erased to black. That's a compromise — we don't have alpha or an off-screen backbuffer; when the message fades, the Atari frame renderer's next `present()` call will overwrite that strip back to game content.

- [ ] **Step 3: Wire `osd::show` + `osd::tick` in main.cpp**

Include: `#include "ui/osd.h"` near other includes.

Call `osd::tick()` at the end of `loop()`:

```cpp
  osd::tick();

  // heartbeat (keep existing block)
  static uint32_t last_hb = 0;
  ...
```

In `on_input_action`, add OSD messages (replacing the T3 versions):

```cpp
    case KM_ACT_DISPLAY_MODE_CYCLE: {
      renderer::Mode next = static_cast<renderer::Mode>(
        (static_cast<int>(renderer::get_mode()) + 1) % 4);
      renderer::set_mode(next);
      Serial.printf("display: %s\n", mode_name(next));
      osd::show(mode_name(next));
      break;
    }
    case KM_ACT_VOLUME_DOWN: {
      audio_out::set_volume_delta(-16);
      char buf[24];
      snprintf(buf, sizeof(buf), "VOL %u", M5Cardputer.Speaker.getVolume());
      osd::show(buf);
      break;
    }
    case KM_ACT_VOLUME_UP: {
      audio_out::set_volume_delta(+16);
      char buf[24];
      snprintf(buf, sizeof(buf), "VOL %u", M5Cardputer.Speaker.getVolume());
      osd::show(buf);
      break;
    }
    case KM_ACT_BRIGHTNESS_DOWN:
    case KM_ACT_BRIGHTNESS_UP: {
      uint8_t b = M5Cardputer.Display.getBrightness();
      int nb = (int)b + ((act == KM_ACT_BRIGHTNESS_UP) ? 16 : -16);
      if (nb < 0) nb = 0;
      if (nb > 255) nb = 255;
      M5Cardputer.Display.setBrightness((uint8_t)nb);
      char buf[24];
      snprintf(buf, sizeof(buf), "BRT %d", nb);
      osd::show(buf);
      break;
    }
    case KM_ACT_SAVE_STATE: osd::show("NO SAVE STATE"); break;
    case KM_ACT_LOAD_STATE: osd::show("NO SAVE STATE"); break;
```

- [ ] **Step 4: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 5: HUMAN CHECKPOINT — OSD visible**

Bump splash to `v0.3-m3-t16`. Flash.

Expected:
- Press `Fn+\\` — top-right briefly shows `Stretch` / `Pixel-perfect` / etc. for ~2 s.
- Press `Fn+-` — shows `VOL 112` (or similar). Sound gets quieter.
- Press `Fn+[` — shows `BRT 80` (or similar). LCD dims.
- Press `Fn+9` — shows `NO SAVE STATE` (since M3 doesn't save).

Do not proceed until OSD visibly appears for at least `Fn+\\` and `Fn+-`.

- [ ] **Step 6: Commit**

```bash
git add src/ui/osd.h src/ui/osd.cpp src/main.cpp
git commit -m "ui: transient OSD (top-right, 2s) for mode/volume/brightness + save-state stub"
```

---

### Task 17: Pause audio while menu is open; resume on close

**Files:**
- Modify: `src/ui/menu.cpp` (already calls `audio_out::set_muted(true)` in `open()` per T15)

Already implemented in T15 — this task is a verification-only step, not code.

- [ ] **Step 1: Verify on device**

Flash whatever build is current. From a playing xex that's making sound:

- Press `Fn+8` — audio should instantly cut to silence.
- Menu navigates normally.
- `Esc` or "Resume" — audio resumes within 20 ms.

If audio continues while menu is open: inspect `menu::open()` in `src/ui/menu.cpp` — is `audio_out::set_muted(true)` being called? If volume dropped to 0 but you still hear sound, it's the DMA ring draining its last 20 ms of queued samples; that's acceptable.

- [ ] **Step 2: If verified, no-op commit skipped.**

If you needed to patch the mute path, commit whatever fix:

```bash
git commit -m "audio: verify mute-on-menu works end-to-end"
```

---

### Task 18: Milestone acceptance + tag

**Files:** none

- [ ] **Step 1: Final splash bump**

In `src/main.cpp`, set the splash to `v0.3-m3` (no suffix). Both `FW_VER=...` Serial print and the LCD splash text.

```bash
pio run -e cardputer-adv
git add src/main.cpp
git commit -m "main: splash v0.3-m3 — M3 milestone"
```

- [ ] **Step 2: Full host test suite**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: all host tests pass.

- [ ] **Step 3: Fresh-boot hardware acceptance checklist**

Flash a clean `cardputer-atari800.m3.bin` to the device. Walk through each item:

- [ ] Boot: LCD shows `v0.3-m3` splash; Serial `FW_VER=v0.3-m3`.
- [ ] BASIC READY prompt reachable (no xex on SD or xex does not auto-run).
- [ ] Typing `PRINT "HELLO"` + Return echoes `HELLO`.
- [ ] `SOUND 0,121,10,15` produces audible tone; `SOUND 0,0,0,0` silences.
- [ ] `Fn+-` / `Fn+=` change volume; OSD `VOL nnn` shows.
- [ ] `Fn+[` / `Fn+]` change brightness; OSD `BRT nnn` shows.
- [ ] `Fn+\\` cycles display modes; OSD shows current name.
- [ ] `Fn+4` warm reset clears screen, preserves BASIC variables.
- [ ] `Fn+5` cold reset clears screen and RAM (variables gone).
- [ ] `Fn+7` break halts a running `10 GOTO 10` program.
- [ ] Drop a joystick-driven xex; it auto-loads; `E/S/A/D` move sprite on cluster 1; `; . , /` on cluster 2; `K`/`L`/`Z`/`X` fire.
- [ ] `Fn+J` toggles to keyboard mode (sprite stops responding), back to joystick.
- [ ] `Fn+L` opens the ROM browser; audio mutes while the browser owns the screen.
- [ ] Browser directory enumeration is responsive; held navigation repeats without skipping wildly.
- [ ] Selecting `.xex` / `.atr` / `.car` style files dispatches through atari800's loader path and returns to emulation.
- [ ] Loading game-oriented extensions selects joystick mode; disk/BASIC-oriented extensions select keyboard mode. `Fn+J` still toggles manually.

Any failure → iterate; do not tag.

- [ ] **Step 4: Git tag**

```bash
git status   # expect: clean
git tag -a v0.3-m3 -m "Milestone 3: input, audio, usable ROM browser

- Full Cardputer keyboard wired into atari800 core (default + Fn layers)
- Dual-cluster joystick (ESAD+KL / ;.,/+ ZX) OR'd into Joystick-1
- Auto-detect keyboard/joystick mode from file extension; Fn+J manual toggle
- Fast POKEY audio via ES8311/raw I2S, with console speaker events for system sounds
- Volume/brightness/display-mode Fn bindings
- Fn+L ROM browser for SD file loading, with fast enumeration and atari800 loader dispatch

M4 adds: browser UX polish, full in-emulator menu, settings persistence,
and copy-on-write .atr work. M5 adds save/load states."
```

---

## M3 acceptance checklist

- [ ] `pio run -e cardputer-adv` builds clean; flash under 80% of OTA slot
- [ ] `ctest` passes (current baseline: 17 host tests)
- [ ] Every item in Task 18's hardware checklist is ticked
- [ ] No heap regression: steady-state free heap ≥ 12 KB (was ~16 KB in M2; we expect ~4-8 KB consumed by audio buffers + Speaker task)
- [ ] Git tag `v0.3-m3` present

## What's NOT in M3 (deferred)

- Copy-on-write disk mounts (M4)
- atari800.cfg / last.json persistence (M4)
- Save states (M5)
- Multi-slot save states, quick-resume, auto-resume (M5)
- Full in-emulator settings menu, including "Swap disk" (M4)
- Browser UX refinements beyond the usable Fn+L baseline (M4)
- Full 3-way input mode (auto / keyboard / joystick) — M3 simplifies to keyboard/joystick since auto-detect is only triggered on file load
- Stereo POKEY authoring tools (not planned)
- IMU tilt-as-joystick (not planned for v1)

## M4 readiness notes for future-you

- The M3 `g_settings` struct is the ground truth for user preferences. M4's persistence should serialize it to INI and read it back before applying runtime settings.
- Fn+L already opens a usable ROM browser. M4 should refine browser UX and wire it into the full menu flow rather than reintroducing a hardcoded load path.
- `mode_autodetect_for` already handles all five extensions per the spec (`.xex/.car/.rom` -> joystick, `.atr/.bas/.cas/.exe` -> keyboard). Format-specific work should respect that mode choice.
- The full menu can include "Exit to browser" as the last item before "Cold reset".
- `audio_out::pump()` assumes 50 Hz PAL timing. When M4 adds NTSC-per-game overrides + the region toggle, `pump()`'s buffer-fill size (currently 882 samples = 20 ms) becomes slightly off for NTSC (16.7 ms/frame). The DMA ring tolerates this as drift; if you hear crackling specifically on NTSC games, drop the buffer to 735 frames for NTSC or accept the small over-fill.
- Dual-POKEY stereo via `pokey_glue_set_stereo` is a cheap runtime flip, but the filter state inside pokeysnd.c is not cleared — expect 1-2 frames of filter transient on toggle. That's audible as a brief tick; mute-on-toggle is worth adding in M4 polish.
