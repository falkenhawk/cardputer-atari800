# cardputer-atari800 — Milestone 2: atari800 Core + First Rendered Frame

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor the upstream `atari800` emulator core into the firmware, embed AltirraOS + AltirraBASIC as fallback ROMs, and render the first Atari frame on the Cardputer-Adv LCD by loading a hardcoded `.xex` from SD. End state: a known XL/XE demo or simple game boots and its frame is visible in Stretch mode.

**Architecture:** Three-layer separation per the spec (section 2.2):

1. **Application** — `src/main.cpp` wires everything together.
2. **Port layer** — `lib/atari800_port/` implements the hooks the core calls (timing, I/O, keyboard/joystick stubs for M2, audio stub, display via our renderer).
3. **Core** — `lib/atari800/` is upstream atari800 treated as a black-box library with three entry points we care about: `Atari800_Initialise()`, `Atari800_Frame()`, and `Screen_atari[]` buffer for pixel output.

Plus a display pipeline (palette, renderer, projector, lcd) and a minimal file loader for `.xex` executables. No input, no audio, no UI/menu, no save states yet — those are M3/M4/M5.

**Tech Stack:** Same as M1 + vendored `atari800` sources (pure C, GPLv2) + AltirraOS / AltirraBASIC ROM images (redistributable).

**Prerequisites:**
- M1 complete (tag `v0.1-m1`). Firmware boots, LCD draws, SD mounts.
- `git` (for cloning upstream atari800)
- A test `.xex` file that's known to run on 65XE and produces visible graphics. Suggested: a public-domain demo from the atari8bit archive (e.g. a 5-second "Hello" .xex — any simple executable will do for first-frame verification). User will be asked to drop it on the SD card in T14.

**Reference docs for agent:**
- Spec: `docs/superpowers/specs/2026-04-20-cardputer-atari800-design.md` sections 2, 3, 5, 14, 15
- M1 plan for context: `docs/superpowers/plans/2026-04-20-m1-bootstrap-and-hal.md`
- atari800 upstream: https://github.com/atari800/atari800
- AltirraOS: https://www.virtualdub.org/altirra.html (includes `AltirraOS` and `AltirraBASIC` binaries in the distribution's `Kernel` folder, redistributable)
- M5GFX / LGFX DMA push reference: `.pio/libdeps/cardputer-adv/M5GFX/src/lgfx/v1/LGFX_Button.hpp` (neighboring files show the `pushImage16` pattern we'll use)

**Testing approach:**
- Pure-C modules (palette, projector, loader) get host-side TDD via the CMake harness from M1.
- Core integration (getting atari800 to compile/link) is not TDD-able — validated by "does the binary produce a Frame()-stable serial-alive build?"
- Display pipeline is hardware-verified at HUMAN CHECKPOINT: the agent does not see pixels, you do.
- Flashing workflow identical to M1: bin goes to SD `/downloads/`, M5Launcher flashes it to the OTA app slot.

**File structure after M2:**

```
cardputer-atari800/
├── lib/
│   ├── atari800/                       # vendored upstream core
│   │   ├── library.json                # PlatformIO library metadata
│   │   ├── src/
│   │   │   ├── (selected upstream .c files — see T1 for the list)
│   │   │   └── roms/
│   │   │       ├── altirra_os_xl.h     # embedded AltirraOS (16 KB)
│   │   │       └── altirra_basic.h     # embedded AltirraBASIC (8 KB)
│   │   └── port.h                      # hook declarations
│   └── atari800_port/
│       ├── library.json
│       ├── src/
│       │   ├── port_impl.cpp           # implements port.h: timing, I/O, kbd/joy/snd stubs
│       │   └── port_display.cpp        # implements display hook: Screen_atari → our renderer
│       └── port.h                      # same as lib/atari800/port.h (shared)
├── src/
│   ├── main.cpp                        # modified: wire core boot + frame loop
│   ├── display/
│   │   ├── palette.h
│   │   ├── palette.cpp                 # PAL + NTSC 256-entry RGB565 LUTs
│   │   ├── lcd.h
│   │   ├── lcd.cpp                     # M5Cardputer.Display wrapper, line push
│   │   ├── renderer.h
│   │   ├── renderer.cpp                # Screen_atari line → RGB565 line
│   │   ├── projector.h
│   │   └── projector.cpp               # 4 display modes (A/B/C/D)
│   └── storage/
│       ├── loader.h
│       └── loader.cpp                  # minimal .xex parser (M2 subset)
└── test/
    ├── test_palette.c
    ├── test_projector.c
    └── test_loader.c
```

Every source file has one responsibility. Files ≤ 200 lines where possible; the scanline renderer can exceed that if it's tight.

---

### Task 1: Vendor the atari800 core into `lib/atari800/`

**Files:**
- Create: `lib/atari800/library.json`
- Create: `lib/atari800/src/` (populate via upstream clone and curated copy)
- Create: `lib/atari800/port.h`

- [ ] **Step 1: Clone upstream atari800 to a sibling directory**

```bash
cd /tmp
git clone --depth 1 --branch ATARI800_5_2_0 https://github.com/atari800/atari800 atari800-upstream
cd atari800-upstream
```

Why tag 5.2.0: it's the most recent stable release, known to compile cleanly against C89/C99 without ESP-hostile dependencies. If the clone fails with "branch not found", use `--branch master` — the upstream HEAD is acceptable.

- [ ] **Step 2: Identify the minimal source set**

atari800 upstream has ~120 `.c` files; many are platform-specific (X11, curses, DirectDraw) and we don't want any of them. The portable core is in `src/`. We need these files only (confirmed by inspecting upstream's `Makefile.am` and the `libretro-atari800` fork's list):

```
afile.c       antic.c      atari.c       binload.c     cartridge.c
cassette.c    compfile.c   cpu.c         crc32.c       devices.c
esc.c         gtia.c       img_tape.c    log.c         memory.c
monitor.c     pbi.c        pia.c         pokey.c       pokeysnd.c
rtime.c       sio.c        sio_patch.c   sysrom.c      util.c
```

Plus headers: all `.h` files in `src/` at the same level.

**Do NOT copy:** `src/sdl/`, `src/curses/`, `src/win32/`, `src/dos/`, `src/unix.c`, `src/videomode.c`, `src/screen.c` (we replace with our renderer), `src/ui*`, `src/colours*`, `src/sound.c` (we replace with our pokey glue in M3), `src/remote_console.c`.

Copy just those files from `/tmp/atari800-upstream/src/` into `lib/atari800/src/`:

```bash
mkdir -p lib/atari800/src
cd /tmp/atari800-upstream/src
cp afile.c antic.c atari.c binload.c cartridge.c cassette.c compfile.c \
   cpu.c crc32.c devices.c esc.c gtia.c img_tape.c log.c memory.c \
   monitor.c pbi.c pia.c pokey.c pokeysnd.c rtime.c sio.c sio_patch.c \
   sysrom.c util.c \
   lib/atari800/src/
cp *.h lib/atari800/src/
```

Upstream files may pull in configuration via `config.h` — we'll provide a hand-written `config.h` in Step 4.

- [ ] **Step 3: Create `lib/atari800/library.json`**

```json
{
  "name": "atari800_core",
  "version": "5.2.0-cardputer",
  "description": "atari800 emulator core (upstream vendored, C-only subset for ESP32-S3)",
  "license": "GPL-2.0",
  "build": {
    "srcFilter": ["+<src/>"],
    "includeDir": "src",
    "flags": [
      "-I$PROJECT_INCLUDE_DIR",
      "-DBASIC",
      "-DSTEREO_SOUND=0",
      "-DCARDPUTER_ATARI800",
      "-Wno-implicit-function-declaration",
      "-Wno-unused-variable",
      "-Wno-unused-parameter"
    ]
  }
}
```

`-DCARDPUTER_ATARI800` is our own feature flag so upstream code can gate on it if we need to patch anything small. The `-Wno-*` flags suppress legitimate warnings in pristine upstream code that we don't want to rewrite (see T5 for a targeted patch list).

- [ ] **Step 4: Create `lib/atari800/src/config.h`**

atari800 expects `config.h` to define platform features. Hand-roll a minimal ESP32-S3-appropriate version:

```c
/* config.h — hand-written for the cardputer-atari800 embedded build.
   Replaces the auto-generated config.h from upstream's autotools build. */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "atari800"
#define PACKAGE_VERSION "5.2.0-cardputer"

/* Emulated machines we enable */
#define BASIC                 /* built-in BASIC cartridge support */

/* Features we disable for embedded build */
#undef SOUND                  /* M3 will enable */
#undef STEREO_SOUND
#undef SERIO_SOUND
#undef CONSOLE_SOUND
#undef SYNCHRONIZED_SOUND
#undef NETSIO
#undef MONITOR_BREAK
#undef MONITOR_BREAKPOINTS
#undef MONITOR_HINTS
#undef VOICEBOX
#undef XEP80_EMULATION
#undef AF80
#undef BIT3
#undef PBI_MIO
#undef PBI_BB
#undef PBI_XLD
#undef PBI_PROTO80

/* Target characteristics */
#define WORDS_BIGENDIAN 0     /* ESP32 is little-endian */
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_TIME_H 1

/* Embedded: no file I/O beyond what we wrap */
#undef HAVE_DIRENT_H
#undef HAVE_UNISTD_H
#undef HAVE_OPENDIR
#undef HAVE_GETTIMEOFDAY

#endif /* CONFIG_H */
```

- [ ] **Step 5: Create `lib/atari800/port.h` — hook declarations**

This header declares the functions the core will call out to (and expects us to implement in `lib/atari800_port/`). It's **declarations only**, no implementation.

```c
/* port.h — cardputer-atari800 port shim declarations.
   The atari800 core calls these when it needs to reach hardware or the host
   environment. Implementations live in lib/atari800_port/src/.
   The core does NOT include this file directly; it's included by our
   replacements for the files we chose not to copy from upstream (e.g. our
   own Screen_atari access and our sound init). */

#ifndef CARDPUTER_ATARI800_PORT_H
#define CARDPUTER_ATARI800_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- timing ---- */
/* Returns milliseconds since boot. Wraps at ~49 days; core doesn't care. */
uint32_t port_millis(void);

/* ---- ROM loading ---- */
/* Called once during Atari800_Initialise. Fills *rom_out with a pointer to
   a 16 KB OS ROM image (XL/XE) and *len_out with 16384.
   Returns 1 on success, 0 if no ROM available. */
int port_load_os_rom(const uint8_t** rom_out, size_t* len_out);

/* Same as above for the 8 KB BASIC ROM. Returns 1 on success, 0 otherwise. */
int port_load_basic_rom(const uint8_t** rom_out, size_t* len_out);

/* ---- display ---- */
/* Called after each frame. core_screen is a pointer to the Atari800 core's
   Screen_atari buffer (384 x 240 bytes of 8-bit palette indices). The port
   is free to render it however it likes, including line-by-line.
   M2 renders to the Cardputer LCD. */
void port_present_frame(const uint8_t* core_screen);

/* ---- input stubs (M3 will replace) ---- */
int port_get_key(void);            /* returns AKEY_NONE (-1) in M2 */
int port_get_joy0(void);           /* returns 0xFF (idle) in M2 */
int port_get_joy_fire0(void);      /* returns 0 in M2 */

/* ---- sound stubs (M3 will replace) ---- */
void port_sound_init(int freq);    /* no-op in M2 */
void port_sound_write(const int16_t* buf, size_t n_frames); /* no-op in M2 */

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_ATARI800_PORT_H */
```

- [ ] **Step 6: Commit vendored core**

```bash
cd .
git add lib/atari800/
git commit -m "vendor atari800 5.2.0 core (portable subset) + port.h declarations

~25 .c files from upstream src/ needed for XL/XE emulation (cpu, antic,
gtia, pokey, memory, sio, etc.). Hand-written config.h for embedded
build (no sound, no monitor, no X11/curses UI). port.h declares hooks
the core will call — implementations in lib/atari800_port/ (T3)."
```

---

### Task 2: Embed AltirraOS and AltirraBASIC as `.h` byte arrays

**Files:**
- Create: `lib/atari800/src/roms/altirra_os_xl.h`
- Create: `lib/atari800/src/roms/altirra_basic.h`

AltirraOS and AltirraBASIC are free, redistributable Atari OS/BASIC replacements by Avery Lee (author of the Altirra emulator). They are drop-in compatible with the real Atari OS/BASIC ROMs.

- [ ] **Step 1: Download AltirraOS binaries**

```bash
cd /tmp
curl -L -o altirra-src.zip "https://www.virtualdub.org/downloads/Altirra-src-4.20.zip"
unzip -q altirra-src.zip -d altirra-src
# Altirra ships prebuilt ROM images under src/Kernel/ or similar — find them:
find altirra-src -name 'altirraos_xl.rom' -o -name 'altirra_basic.rom' 2>/dev/null
```

If the URL is stale or the archive layout differs, check https://www.virtualdub.org/altirra.html for the latest release. The specific files we need are:

- `altirraos_xl.rom` (16384 bytes) — AltirraOS for 800XL/XE
- `altirra_basic.rom` (8192 bytes) — AltirraBASIC

If the exact filenames differ, identify the two binaries by their sizes (16384 and 8192 bytes respectively).

- [ ] **Step 2: Convert the 16 KB OS ROM to a C byte array**

```bash
cd .
mkdir -p lib/atari800/src/roms
xxd -i -n altirra_os_xl /tmp/altirra-src/path/to/altirraos_xl.rom > lib/atari800/src/roms/altirra_os_xl.h
```

Verify the generated header starts with:
```c
unsigned char altirra_os_xl[] = {
  0xXX, 0xXX, ...
};
unsigned int altirra_os_xl_len = 16384;
```

Wrap it with include guards and `const`-qualify:

```c
/* lib/atari800/src/roms/altirra_os_xl.h
   AltirraOS for 800XL/XE, 16 KB. Redistributable per Altirra license. */
#ifndef ALTIRRA_OS_XL_H
#define ALTIRRA_OS_XL_H

static const unsigned char altirra_os_xl[] = {
  /* ... the xxd output bytes ... */
};
static const unsigned int altirra_os_xl_len = 16384;

#endif
```

- [ ] **Step 3: Same for BASIC (8 KB)**

```bash
xxd -i -n altirra_basic /tmp/altirra-src/path/to/altirra_basic.rom > lib/atari800/src/roms/altirra_basic.h
```

Wrap the same way, with `altirra_basic_len = 8192`.

- [ ] **Step 4: Verify byte counts**

```bash
grep -c '^  0x' lib/atari800/src/roms/altirra_os_xl.h
# Expect: 1024 lines (each line has 16 bytes, 16384 / 16 = 1024)

grep -c '^  0x' lib/atari800/src/roms/altirra_basic.h
# Expect: 512 lines (8192 / 16 = 512)
```

- [ ] **Step 5: Commit**

```bash
git add lib/atari800/src/roms/
git commit -m "embed AltirraOS-XL (16K) and AltirraBASIC (8K) as const byte arrays

Redistributable free replacements for the real Atari OS/BASIC ROMs.
Used as fallback when user hasn't dropped their own dumps on SD at
/atari800/roms/{atarixl,ataribas}.rom."
```

---

### Task 3: Port layer stubs — get the core to link

**Files:**
- Create: `lib/atari800_port/library.json`
- Create: `lib/atari800_port/src/port_impl.cpp`

- [ ] **Step 1: Library metadata**

`lib/atari800_port/library.json`:

```json
{
  "name": "atari800_port",
  "version": "0.1.0",
  "description": "Port layer: core ↔ Cardputer hardware shim",
  "dependencies": {
    "atari800_core": "*"
  },
  "build": {
    "srcFilter": ["+<src/>"],
    "includeDir": "src"
  }
}
```

- [ ] **Step 2: Implement the port stubs**

`lib/atari800_port/src/port_impl.cpp`:

```cpp
/* port_impl.cpp — M2 minimum viable port layer.
   Implements timing + ROM loaders + empty input/sound stubs so the core
   links. Display hook lives in port_display.cpp (T8). */

#include <Arduino.h>
#include "../../atari800/port.h"
#include "../../atari800/src/roms/altirra_os_xl.h"
#include "../../atari800/src/roms/altirra_basic.h"

extern "C" {

uint32_t port_millis(void) {
  return millis();
}

int port_load_os_rom(const uint8_t** rom_out, size_t* len_out) {
  *rom_out = altirra_os_xl;
  *len_out = altirra_os_xl_len;
  return 1;
}

int port_load_basic_rom(const uint8_t** rom_out, size_t* len_out) {
  *rom_out = altirra_basic;
  *len_out = altirra_basic_len;
  return 1;
}

/* Display hook is in port_display.cpp — defined weak here so linkage works
   even before T8 lands.  Once T8 lands, the real implementation there
   overrides this. */
__attribute__((weak))
void port_present_frame(const uint8_t* core_screen) {
  (void)core_screen;
  /* no-op until T8 */
}

int port_get_key(void)       { return -1; }   /* AKEY_NONE */
int port_get_joy0(void)      { return 0xFF; } /* idle stick */
int port_get_joy_fire0(void) { return 0; }

void port_sound_init(int freq)                                    { (void)freq; }
void port_sound_write(const int16_t* buf, size_t n_frames)        { (void)buf; (void)n_frames; }

} /* extern "C" */
```

- [ ] **Step 3: Wire the core library into platformio.ini**

Open `platformio.ini` and verify the `lib_deps` section. PlatformIO auto-discovers libraries under `lib/`, so nothing to add explicitly — but double-check with:

```bash
pio pkg list -e cardputer-adv 2>&1 | grep -E 'atari800'
```

If the discovery doesn't pick up our local libraries, add to `platformio.ini`:

```ini
lib_extra_dirs =
  lib
```

(Actually PlatformIO's default behavior already includes `lib/` — only add this line if the pkg list doesn't show `atari800_core` and `atari800_port`.)

- [ ] **Step 4: Attempt first build — expect failures**

```bash
pio run -e cardputer-adv
```

Expected outcome: the build will fail. Typical categories of error we'll encounter:

1. Missing `config.h` — we created one in T1. Should be found via include path.
2. `Screen_atari`, `ANTIC_screenbuffer`, `SCREEN_WIDTH` etc. — these are defined in `screen.c` / `videomode.c`, which we deliberately excluded. The core modules reference them. We'll either include the data declarations or write tiny stubs (T5).
3. `Sound_init`, `Sound_Update` — we excluded `sound.c`. Provide empty stubs (T5).
4. `Input_Update`, `Input_Frame` — we excluded input files. Stubs (T5).
5. C++ name mangling — fix by ensuring all our port code wraps in `extern "C"` (already done above).

Capture the first 30 missing-symbol errors from the link step:

```bash
pio run -e cardputer-adv 2>&1 | grep "undefined reference" | head -30 > /tmp/undefined-symbols.txt
cat /tmp/undefined-symbols.txt
```

This list drives T5. DO NOT commit yet — we commit once the core compiles.

---

### Task 4: Remove files we actually need to keep (post-T3 correction)

The Step 2 of T1 excluded some files ("we replace with our renderer / our sound") — but those files declare symbols used by other core modules, so they can't be simply absent. We'll keep them but replace their contents with minimal stubs. This task is here rather than in T1 because you only know which files need this treatment after seeing the T3 build errors.

**Files:**
- Create stub versions of: `lib/atari800/src/screen.c`, `lib/atari800/src/sound.c`, `lib/atari800/src/input.c`, `lib/atari800/src/videomode.c`, `lib/atari800/src/colours.c`, `lib/atari800/src/colours_ntsc.c`, `lib/atari800/src/colours_pal.c`, `lib/atari800/src/platform.c` — **only if** the T3 error log references symbols from these files.

- [ ] **Step 1: Read `/tmp/undefined-symbols.txt` from T3 Step 4**

Each line will look like `undefined reference to \`Screen_atari'` or similar.

- [ ] **Step 2: For each missing symbol, locate its original file in upstream**

```bash
cd /tmp/atari800-upstream/src
grep -l 'Screen_atari' *.c | head -5
grep -l 'Sound_Initialise' *.c | head -5
grep -l 'Input_Initialise' *.c | head -5
grep -l 'Colours_Initialise' *.c | head -5
```

- [ ] **Step 3: Copy those files into our lib**

```bash
# Copy exactly the files the error log points to:
cp /tmp/atari800-upstream/src/screen.c   lib/atari800/src/
cp /tmp/atari800-upstream/src/sound.c    lib/atari800/src/
cp /tmp/atari800-upstream/src/input.c    lib/atari800/src/
cp /tmp/atari800-upstream/src/platform.c lib/atari800/src/
cp /tmp/atari800-upstream/src/colours.c           lib/atari800/src/
cp /tmp/atari800-upstream/src/colours_pal.c       lib/atari800/src/
cp /tmp/atari800-upstream/src/colours_ntsc.c      lib/atari800/src/
```

- [ ] **Step 4: Commit**

```bash
git add lib/atari800/src/
git commit -m "add screen/sound/input/colours/platform modules from upstream

These declare symbols referenced by cpu/antic/gtia/pokey so they're
required for the core to link — initial exclusion in T1 was too
aggressive."
```

---

### Task 5: Targeted patches to make the core link

With all needed files present, the build will still fail on specific hostile-to-embedded patterns: `stdout`/`stderr` printf calls, file system assumptions, mutex headers, etc. Apply minimal patches — **only to resolve link/compile errors**, not to improve code we don't need to touch.

**Files:**
- Modify: specific files in `lib/atari800/src/` as determined by the next build

- [ ] **Step 1: Attempt a build, capture errors**

```bash
pio run -e cardputer-adv 2>&1 | tee /tmp/m2-build.log
grep -E "error:|undefined reference" /tmp/m2-build.log | head -40
```

- [ ] **Step 2: Handle each error category**

Expected categories and the standard fix for each:

**A. `printf`, `fprintf(stderr, ...)` in `log.c`:**
Redirect to Serial. In `lib/atari800/src/log.c`, replace the file's contents with:

```c
#include "log.h"
#include <stdarg.h>
#include <stdio.h>

/* External hook provided by the port layer */
extern void port_log_write(const char* msg);

void Log_print(const char* format, ...) {
  static char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  port_log_write(buf);
}
```

And add to `lib/atari800_port/src/port_impl.cpp`:

```cpp
extern "C" void port_log_write(const char* msg) {
  Serial.print(msg);
}
```

**B. `open()`, `read()`, file I/O:**
The core might pull these in via `atari.c` or `binload.c` for loading ATR/XEX from disk. For M2, we load .xex via our `storage/loader.cpp` (T13), not the core's file I/O. Provide stub functions that return "file not found":

In `lib/atari800_port/src/port_impl.cpp`:

```cpp
extern "C" {
FILE* fopen(const char* path, const char* mode) { (void)path; (void)mode; return NULL; }
int fclose(FILE* f)                             { (void)f; return 0; }
size_t fread(void* p, size_t s, size_t n, FILE* f)  { (void)p; (void)s; (void)n; (void)f; return 0; }
size_t fwrite(const void* p, size_t s, size_t n, FILE* f) { (void)p; (void)s; (void)n; (void)f; return 0; }
int fseek(FILE* f, long o, int w)               { (void)f; (void)o; (void)w; return -1; }
long ftell(FILE* f)                             { (void)f; return 0; }
}
```

Only add these stubs **if the build errors reference them**. ESP-IDF's newlib may already provide working versions.

**C. `Colours_SetVideoSystem` and friends referenced but with missing argument types:**
Core modules expect a `struct COLOURS_Video` — keep `colours_pal.c` / `colours_ntsc.c` as we copied them in T4; they provide this.

**D. Each fix is one commit**

For each class of error resolved, commit separately:

```bash
git add lib/atari800/src/log.c lib/atari800_port/src/port_impl.cpp
git commit -m "core: route Log_print through port_log_write → Serial"
```

etc.

- [ ] **Step 3: Iterate until `pio run -e cardputer-adv` succeeds**

Expected: clean compile, no undefined references. Flash usage will jump significantly (core adds ~200–400 KB of code). Confirm flash is still under 80% of the 3.8 MB OTA slot.

Do not proceed until the build succeeds. If after 2 hours of patching you're still seeing errors that don't fall into the categories above, STOP and escalate with the full error log.

- [ ] **Step 4: Final commit for this task**

```bash
git add lib/atari800/ lib/atari800_port/
git commit -m "core: apply remaining embedded-build patches; links cleanly"
```

---

### Task 6: Initialize the core and run one frame (no display yet)

**Files:**
- Modify: `src/main.cpp`

This is where we discover whether the core actually runs on ESP32-S3 at all. We only need to prove that `Atari800_Initialise()` returns, `Atari800_Frame()` runs once without crashing, and the heap survives.

- [ ] **Step 1: Include the core header in main.cpp and call init/frame**

Modify `src/main.cpp`. Add at the top:

```cpp
extern "C" {
#include "../lib/atari800/src/atari.h"
#include "../lib/atari800/port.h"
}
```

In `setup()`, after the existing SD mount code, add:

```cpp
  // M2: init the atari800 core
  Serial.println("core: initialising atari800…");
  int argc = 1;
  char* argv[] = {(char*)"atari800"};
  int init_ok = Atari800_Initialise(&argc, argv);
  Serial.printf("core: init_ok=%d\n", init_ok);

  if (init_ok) {
    Serial.println("core: running 1 frame as smoke test…");
    Atari800_Frame();
    Serial.println("core: frame 1 done, heap OK");
  }
```

(We do not loop yet — one frame only, to prove the core doesn't crash.)

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS. Flash usage roughly 50-70%. RAM should still be under 30% (core uses ~60 KB for 128 KB XE RAM + buffers).

- [ ] **Step 3: HUMAN CHECKPOINT — flash via M5Launcher and observe serial**

Copy `firmware.bin` to SD as `cardputer-atari800.m2-t6.bin`, flash via Launcher, monitor serial.

Expected serial output:

```
cardputer-atari800 — boot
splash rendered
SD: mount failed   (or SD info if card inserted)
core: initialising atari800…
core: init_ok=1
core: running 1 frame as smoke test…
core: frame 1 done, heap OK
uptime 10000 ms
uptime 20000 ms
```

If `init_ok=0`, the ROMs aren't loading correctly — verify T2 byte counts (16384 and 8192).

If the chip hangs between "initialising" and "init_ok", the core is crashing in init. Capture what was printed; likely a port_load_* function is returning unexpected pointer. Walk back through T3.

If `core: frame 1 done, heap OK` appears: **this is the milestone**. The core runs.

Do not proceed until human confirms this output.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "core: initialise + single frame smoke test (no display yet)"
```

---

### Task 7: Palette module with host-side tests (PAL first)

**Files:**
- Create: `src/display/palette.h`
- Create: `src/display/palette.cpp`
- Create: `test/test_palette.c`
- Modify: `test/CMakeLists.txt`

The Atari palette is 256 colors — 16 hues × 16 luminance levels. PAL and NTSC differ in how hue maps to RGB (because of phase differences). We precompute both 256-entry RGB565 lookup tables at boot.

- [ ] **Step 1: Write the failing test first**

`test/test_palette.c`:

```c
/* test_palette.c — verify the Atari palette LUT generation. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Forward declarations matching the palette.h interface */
extern const uint16_t* palette_get_pal(void);
extern const uint16_t* palette_get_ntsc(void);

static int fail = 0;

#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  const uint16_t* pal  = palette_get_pal();
  const uint16_t* ntsc = palette_get_ntsc();

  /* Both LUTs must be non-null */
  CHECK(pal  != NULL, "palette_get_pal returned NULL");
  CHECK(ntsc != NULL, "palette_get_ntsc returned NULL");

  /* Color index 0 is always pure black on Atari (luma=0, any hue) */
  CHECK(pal[0] == 0x0000,  "pal[0] should be black");
  CHECK(ntsc[0] == 0x0000, "ntsc[0] should be black");

  /* Index 0x0E (hue=0, luma=E) is near-white on Atari */
  CHECK((pal[0x0E]  & 0xF800) > 0xD000, "pal[0x0E] R channel should be bright");
  CHECK((ntsc[0x0E] & 0xF800) > 0xD000, "ntsc[0x0E] R channel should be bright");

  /* PAL and NTSC differ — not identical tables */
  int any_diff = 0;
  for (int i = 0; i < 256; i++) {
    if (pal[i] != ntsc[i]) { any_diff = 1; break; }
  }
  CHECK(any_diff, "PAL and NTSC tables should differ");

  if (fail) return EXIT_FAILURE;
  printf("PASS: palette\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Add to `test/CMakeLists.txt`**

```cmake
add_executable(test_palette test_palette.c ../src/display/palette.cpp)
set_target_properties(test_palette PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME palette COMMAND test_palette)
```

(Note `LINKER_LANGUAGE CXX` — palette.cpp is C++ even though the test is C.)

- [ ] **Step 3: Run test, expect it to fail to build (palette.cpp doesn't exist yet)**

```bash
cmake -B build -S test && cmake --build build 2>&1 | tail -5
```

Expected: build fails with "can't find ../src/display/palette.cpp".

- [ ] **Step 4: Create `src/display/palette.h`**

```cpp
// palette.h — Atari 256-color palette lookup tables (PAL + NTSC) in RGB565.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lazy-compute on first call, then return a cached pointer to 256 uint16_t. */
const uint16_t* palette_get_pal(void);
const uint16_t* palette_get_ntsc(void);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 5: Implement `src/display/palette.cpp`**

```cpp
// palette.cpp — Generate Atari 256-color palette LUTs in RGB565.
// The Atari palette is 16 hues × 16 luminance levels. PAL and NTSC use
// different hue phases; we compute both.
//
// Reference: atari800 upstream colours_pal.c / colours_ntsc.c. This is
// a simplified YUV → RGB → RGB565 derivation.

#include "palette.h"
#include <math.h>

namespace {

uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | ((uint16_t)(b & 0xF8) >> 3);
}

uint8_t clamp_u8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

// Atari hue angle (degrees) for a given 4-bit hue index, for a given region.
// PAL and NTSC differ in the base phase offset.
double hue_angle(int hue_idx, bool is_pal) {
  const double base = is_pal ? 14.0 : 27.0; // degrees; approximation of real phase
  const double step = 360.0 / 15.0;         // hues 1–15 are evenly spaced
  if (hue_idx == 0) return 0.0;             // hue 0 = greyscale
  return base + step * (hue_idx - 1);
}

void compute_palette(uint16_t out[256], bool is_pal) {
  for (int i = 0; i < 256; i++) {
    int hue_idx  = (i >> 4) & 0x0F;
    int luma_idx =  i       & 0x0F;

    double y = luma_idx / 15.0; // 0.0 to 1.0

    if (hue_idx == 0) {
      // Greyscale
      uint8_t g = clamp_u8((int)(y * 255.0));
      out[i] = rgb888_to_565(g, g, g);
      continue;
    }

    double angle = hue_angle(hue_idx, is_pal) * M_PI / 180.0;
    double chroma = is_pal ? 0.50 : 0.55;  // saturation

    double u = chroma * cos(angle);
    double v = chroma * sin(angle);

    // YUV → RGB (BT.601-ish, good enough for 8-bit palette approximation)
    double r = y + 1.140 * v;
    double g = y - 0.395 * u - 0.581 * v;
    double b = y + 2.032 * u;

    out[i] = rgb888_to_565(
      clamp_u8((int)(r * 255.0)),
      clamp_u8((int)(g * 255.0)),
      clamp_u8((int)(b * 255.0))
    );
  }
}

uint16_t pal_lut[256];
uint16_t ntsc_lut[256];
bool pal_computed  = false;
bool ntsc_computed = false;

} // anon namespace

extern "C" const uint16_t* palette_get_pal(void) {
  if (!pal_computed) {
    compute_palette(pal_lut, true);
    pal_computed = true;
  }
  return pal_lut;
}

extern "C" const uint16_t* palette_get_ntsc(void) {
  if (!ntsc_computed) {
    compute_palette(ntsc_lut, false);
    ntsc_computed = true;
  }
  return ntsc_lut;
}
```

- [ ] **Step 6: Build and run the test**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R palette
```

Expected: `PASS: palette`, 1/1 passed.

- [ ] **Step 7: Commit**

```bash
git add src/display/palette.h src/display/palette.cpp test/test_palette.c test/CMakeLists.txt
git commit -m "palette: PAL + NTSC 256-color LUTs with host test

Lazy-computed on first access, cached in static arrays. YUV→RGB
derivation with region-specific phase offset and saturation."
```

---

### Task 8: LCD driver wrapper

**Files:**
- Create: `src/display/lcd.h`
- Create: `src/display/lcd.cpp`

A thin wrapper over `M5Cardputer.Display` exposing a single line-push operation. Splitting it out now lets later components (renderer, UI overlays) use a stable API.

- [ ] **Step 1: Header**

`src/display/lcd.h`:

```cpp
// lcd.h — minimal LCD wrapper for line-buffer blitting.
#pragma once
#include <stdint.h>

namespace lcd {

void init();                                              // clear screen, set rotation
void push_line(int y, const uint16_t* rgb565, int width); // blit one horizontal line
void fill_rect(int x, int y, int w, int h, uint16_t color);

}
```

- [ ] **Step 2: Implementation**

`src/display/lcd.cpp`:

```cpp
#include "lcd.h"
#include <M5Cardputer.h>

namespace lcd {

void init() {
  auto& d = M5Cardputer.Display;
  d.setRotation(1);
  d.fillScreen(0x0000); // black
}

void push_line(int y, const uint16_t* rgb565, int width) {
  M5Cardputer.Display.pushImage(0, y, width, 1, rgb565);
}

void fill_rect(int x, int y, int w, int h, uint16_t color) {
  M5Cardputer.Display.fillRect(x, y, w, h, color);
}

}
```

- [ ] **Step 3: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/display/lcd.h src/display/lcd.cpp
git commit -m "lcd: thin wrapper over M5Cardputer.Display with push_line"
```

---

### Task 9: Projector with 1 mode (Stretch) and host tests

**Files:**
- Create: `src/display/projector.h`
- Create: `src/display/projector.cpp`
- Create: `test/test_projector.c`
- Modify: `test/CMakeLists.txt`

Projector maps an Atari line (width up to 384) down to a Cardputer LCD line (width 240) for the currently-selected display mode. In M2 we only implement Stretch (mode D, default). Modes A/B/C come in T15.

- [ ] **Step 1: Test**

`test/test_projector.c`:

```c
/* test_projector.c — Stretch mode sanity. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Forward from projector.h */
extern void projector_stretch_line(const uint8_t* atari_line,
                                   uint16_t*       out_line,
                                   const uint16_t* palette);

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Build a 320-wide Atari line: all pixels = palette index 0x10 (hue 1, luma 0). */
  uint8_t  atari_line[320];
  for (int i = 0; i < 320; i++) atari_line[i] = 0x10;

  /* Palette where 0x10 maps to 0xABCD, all other entries to 0x0000. */
  uint16_t palette[256] = {0};
  palette[0x10] = 0xABCD;

  uint16_t out_line[240] = {0};
  projector_stretch_line(atari_line, out_line, palette);

  /* All 240 output pixels should have been sampled from 0x10 → 0xABCD. */
  for (int i = 0; i < 240; i++) {
    CHECK(out_line[i] == 0xABCD, "out_line should be uniform 0xABCD");
    if (out_line[i] != 0xABCD) break;
  }

  /* Alternating pattern: pixel i=0,2,4,... → 0x10 (0xABCD), i=1,3,5,... → 0x20 (0x1234) */
  for (int i = 0; i < 320; i++) atari_line[i] = (i & 1) ? 0x20 : 0x10;
  palette[0x20] = 0x1234;

  projector_stretch_line(atari_line, out_line, palette);

  /* Output should contain BOTH colors at ~50/50 (approximately) — proves
     stretch isn't doing something degenerate like taking only even source
     pixels. Simplest check: at least one pixel of each color. */
  int saw_abcd = 0, saw_1234 = 0;
  for (int i = 0; i < 240; i++) {
    if (out_line[i] == 0xABCD) saw_abcd = 1;
    if (out_line[i] == 0x1234) saw_1234 = 1;
  }
  CHECK(saw_abcd, "stretch missed 0xABCD pixels");
  CHECK(saw_1234, "stretch missed 0x1234 pixels");

  if (fail) return EXIT_FAILURE;
  printf("PASS: projector stretch\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Add test target**

`test/CMakeLists.txt`:

```cmake
add_executable(test_projector test_projector.c ../src/display/projector.cpp)
set_target_properties(test_projector PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME projector COMMAND test_projector)
```

- [ ] **Step 3: Header**

`src/display/projector.h`:

```cpp
// projector.h — maps Atari 320-wide scanline → LCD 240-wide line per display mode.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each function writes exactly 240 pixels into out_line.
   atari_line is 320 bytes of 8-bit palette indices.
   palette is 256 entries of RGB565. */
void projector_stretch_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);

/* M2 only implements stretch. Others are stubs that return Stretch for now. */
void projector_pixel_perfect_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);
void projector_pillarbox_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);
void projector_cover_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Implementation**

`src/display/projector.cpp`:

```cpp
// projector.cpp — M2: only Stretch mode implemented; others fall through.
#include "projector.h"

namespace {
// Source-width constants. Atari active picture is 320 pixels wide (visible
// area out of 384-pixel raster). We only sample from the center 320.
constexpr int ATARI_W = 320;
constexpr int ATARI_CENTER_OFFSET = 32; // (384 - 320) / 2
constexpr int LCD_W = 240;
}

extern "C" {

void projector_stretch_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Non-integer scale 240/320 = 0.75. For each output column, sample the
  // corresponding Atari column using nearest-neighbor. Fixed-point to avoid
  // floats in the hot path:
  //   src_x = (out_x * 320 + LCD_W/2) / LCD_W
  for (int x = 0; x < LCD_W; x++) {
    int src = (x * ATARI_W + LCD_W / 2) / LCD_W;
    if (src >= ATARI_W) src = ATARI_W - 1;
    out_line[x] = palette[atari_line[ATARI_CENTER_OFFSET + src]];
  }
}

// Stubs — T15 replaces these with real implementations.
void projector_pixel_perfect_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}
void projector_pillarbox_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}
void projector_cover_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}

}
```

- [ ] **Step 5: Build and test**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R projector
```

Expected: `PASS: projector stretch`.

- [ ] **Step 6: Verify firmware still builds**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 7: Commit**

```bash
git add src/display/projector.h src/display/projector.cpp test/test_projector.c test/CMakeLists.txt
git commit -m "projector: Stretch mode + host test; A/B/C stubbed to Stretch"
```

---

### Task 10: Renderer: Atari screen buffer → LCD

**Files:**
- Create: `src/display/renderer.h`
- Create: `src/display/renderer.cpp`

Connects the three pieces: takes the atari800 core's `Screen_atari` buffer output, walks it line by line, calls projector per line, pushes to LCD.

- [ ] **Step 1: Header**

`src/display/renderer.h`:

```cpp
// renderer.h — drives the per-line conversion from Screen_atari → LCD.
#pragma once
#include <stdint.h>

namespace renderer {

enum class Mode { PixelPerfect, Pillarbox, Cover, Stretch };

void set_mode(Mode m);
void set_region_ntsc(bool ntsc);
void present(const uint8_t* screen_atari);  // 384×240 bytes

}
```

- [ ] **Step 2: Implementation**

`src/display/renderer.cpp`:

```cpp
#include "renderer.h"
#include "projector.h"
#include "palette.h"
#include "lcd.h"

namespace renderer {

namespace {
Mode current_mode = Mode::Stretch;
bool use_ntsc = false;

constexpr int ATARI_SRC_H     = 240;
constexpr int LCD_H           = 135;
constexpr int ATARI_STRIDE    = 384;
constexpr int ATARI_VERT_OFF  = (ATARI_SRC_H - LCD_H) / 2 + 24; // roughly center on active picture
constexpr int LCD_W           = 240;
}

void set_mode(Mode m) { current_mode = m; }
void set_region_ntsc(bool ntsc) { use_ntsc = ntsc; }

void present(const uint8_t* screen_atari) {
  const uint16_t* pal = use_ntsc ? palette_get_ntsc() : palette_get_pal();
  uint16_t line_buf[LCD_W];

  for (int y = 0; y < LCD_H; y++) {
    int src_y = ATARI_VERT_OFF + y;
    if (src_y < 0) src_y = 0;
    if (src_y >= ATARI_SRC_H) src_y = ATARI_SRC_H - 1;
    const uint8_t* atari_line = &screen_atari[src_y * ATARI_STRIDE];

    switch (current_mode) {
      case Mode::PixelPerfect:  projector_pixel_perfect_line(atari_line, line_buf, pal); break;
      case Mode::Pillarbox:     projector_pillarbox_line    (atari_line, line_buf, pal); break;
      case Mode::Cover:         projector_cover_line        (atari_line, line_buf, pal); break;
      case Mode::Stretch:       projector_stretch_line      (atari_line, line_buf, pal); break;
    }

    lcd::push_line(y, line_buf, LCD_W);
  }
}

}
```

- [ ] **Step 3: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/display/renderer.h src/display/renderer.cpp
git commit -m "renderer: drive Screen_atari → projector → lcd per-line"
```

---

### Task 11: Port display hook → renderer

**Files:**
- Create: `lib/atari800_port/src/port_display.cpp`

Replaces the weak `port_present_frame` stub from T3 with a real implementation that calls our renderer.

- [ ] **Step 1: Implementation**

`lib/atari800_port/src/port_display.cpp`:

```cpp
// port_display.cpp — implements port_present_frame as a call to renderer::present.
#include "../../atari800/port.h"
#include "../../../src/display/renderer.h"

extern "C" void port_present_frame(const uint8_t* core_screen) {
  renderer::present(core_screen);
}
```

The previous weak stub in `port_impl.cpp` is overridden at link time by this strong definition.

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add lib/atari800_port/src/port_display.cpp
git commit -m "port: implement port_present_frame → renderer::present"
```

---

### Task 12: Wire core into main loop; first frame on LCD (BASIC prompt)

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Update main.cpp loop**

Replace the T6 one-shot `Atari800_Frame()` call with a loop driven at ~50 Hz (PAL). Call our renderer's `present()` after each frame by reaching into the core's exported `Screen_atari` buffer.

In `src/main.cpp`, add at top:

```cpp
#include "display/lcd.h"
#include "display/renderer.h"
extern "C" {
#include "../lib/atari800/src/screen.h"  // declares Screen_atari
}
```

Replace the T6 smoke-test block in `setup()`:

```cpp
  // M2: init the atari800 core
  Serial.println("core: initialising atari800…");
  int argc = 1;
  char* argv[] = {(char*)"atari800"};
  int init_ok = Atari800_Initialise(&argc, argv);
  Serial.printf("core: init_ok=%d\n", init_ok);
  if (!init_ok) {
    auto& d = M5Cardputer.Display;
    d.setCursor(8, 100);
    d.setTextColor(TFT_RED, TFT_BLACK);
    d.print("core init failed");
    return;
  }
  lcd::init();
  renderer::set_mode(renderer::Mode::Stretch);
  renderer::set_region_ntsc(false);  // PAL default
```

Replace `loop()` — keep M1's keyboard polling and heartbeat, add the frame tick:

```cpp
void loop() {
  M5Cardputer.update();

  // keyboard debug (from T5) — unchanged
  if (M5Cardputer.Keyboard.isChange()) {
    auto status = M5Cardputer.Keyboard.keysState();
    Serial.print("keys:");
    if (status.ctrl)  Serial.print(" CTRL");
    if (status.shift) Serial.print(" SHIFT");
    if (status.alt)   Serial.print(" ALT");
    if (status.fn)    Serial.print(" FN");
    if (status.opt)   Serial.print(" OPT");
    for (auto c : status.word) Serial.printf(" '%c'(0x%02x)", c, c);
    for (auto k : status.hid_keys) Serial.printf(" hid=0x%02x", k);
    Serial.println();
  }

  // M2: drive the emulator and present each frame
  static uint32_t last_frame_ms = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) { // ~50 Hz (PAL). Not locked, coarse.
    last_frame_ms = now;
    Atari800_Frame();
    renderer::present(Screen_atari);
  }

  // heartbeat (from T5) — keep 10s
  static uint32_t last_hb = 0;
  if (now - last_hb >= 10000) {
    last_hb = now;
    Serial.printf("uptime %lu ms\n", now);
  }
}
```

Note: the old `delay(10)` is gone — frame pacing replaces it. The M3 TODO from M1 is satisfied here for this milestone.

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: HUMAN CHECKPOINT — first frame**

Copy bin to SD (`/Volumes/CARDPUTER/downloads/cardputer-atari800.m2-t12.bin`), flash via M5Launcher, observe the LCD.

Expected visual:
- Landscape 240×135 display.
- **Atari blue-ish background** (AltirraOS/BASIC's boot screen).
- **"READY"** prompt visible somewhere (may be small/stretched) in white-on-blue.
- **No garbage** (not random pixels — uniform blue with readable text).

Expected serial:
```
core: initialising atari800…
core: init_ok=1
uptime 10000 ms
...
```

If the LCD is random pixels or blank, something in the renderer pipeline is miswired — most likely:
- Screen_atari stride mismatch (expected 384, we might be skipping 320)
- Palette indexing wrong (wrong region or wrong byte extraction)
- LCD y offset wrong (try ATARI_VERT_OFF = 48 or 0)

If the LCD is uniform blue but no text, the vertical offset in `renderer.cpp` is clipping the text area — tune `ATARI_VERT_OFF`.

Do not proceed until human confirms the Atari BASIC "READY" prompt (or equivalent blue-with-white-text Atari boot screen) is visible.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "main: drive atari800 at 50 Hz, present frames to LCD

First frame milestone: atari800 boots into BASIC, Screen_atari buffer
pipes through renderer/projector (Stretch) onto the 240x135 LCD.
No input, no audio, no loader yet."
```

---

### Task 13: Hardcoded .xex loader

**Files:**
- Create: `src/storage/loader.h`
- Create: `src/storage/loader.cpp`
- Create: `test/test_loader.c`
- Modify: `test/CMakeLists.txt`
- Modify: `src/main.cpp` — call the loader at boot if a specific file is present on SD

`.xex` files are Atari executables. The format: a stream of `(start_addr, end_addr, bytes[end-start+1])` segments, with optional `0xFFFF` header and special `RUNAD`/`INITAD` segments. For M2 we support the common subset: load a simple `.xex` and jump to its RUN address.

- [ ] **Step 1: Host test first**

`test/test_loader.c`:

```c
/* test_loader.c — .xex parser. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* From loader.h */
typedef struct {
  uint16_t start_addr;
  uint16_t end_addr;
  const uint8_t* data;   /* points into the input buffer */
  size_t data_len;
} xex_segment_t;

#define XEX_MAX_SEGMENTS 32

typedef struct {
  xex_segment_t segs[XEX_MAX_SEGMENTS];
  int n_segs;
  uint16_t run_addr;     /* 0 if not specified */
  uint16_t init_addr;    /* 0 if not specified */
} xex_parsed_t;

extern int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out);

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Minimal .xex: header 0xFFFF, one segment 0x2000-0x2003 containing 4 bytes */
  uint8_t xex[] = {
    0xFF, 0xFF,                               /* header */
    0x00, 0x20,                               /* start 0x2000 */
    0x03, 0x20,                               /* end   0x2003 */
    0xAA, 0xBB, 0xCC, 0xDD,                   /* data */
    /* RUNAD segment: start=02E0, end=02E1, RUNAD=0x2000 */
    0xE0, 0x02, 0xE1, 0x02, 0x00, 0x20
  };

  xex_parsed_t p;
  int ok = xex_parse(xex, sizeof(xex), &p);
  CHECK(ok, "xex_parse should succeed");
  CHECK(p.n_segs == 1, "should parse exactly 1 data segment (RUNAD isn't data)");
  CHECK(p.segs[0].start_addr == 0x2000, "seg[0] start addr");
  CHECK(p.segs[0].end_addr   == 0x2003, "seg[0] end addr");
  CHECK(p.segs[0].data_len   == 4,       "seg[0] data len");
  CHECK(p.segs[0].data[0]    == 0xAA,    "seg[0] data[0]");
  CHECK(p.run_addr           == 0x2000,  "RUNAD should be 0x2000");

  if (fail) return EXIT_FAILURE;
  printf("PASS: loader\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Add to CMakeLists.txt**

```cmake
add_executable(test_loader test_loader.c ../src/storage/loader.cpp)
set_target_properties(test_loader PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME loader COMMAND test_loader)
```

- [ ] **Step 3: Create `src/storage/loader.h`**

```cpp
// loader.h — minimal .xex parser (M2 subset).
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XEX_MAX_SEGMENTS 32

typedef struct {
  uint16_t start_addr;
  uint16_t end_addr;
  const uint8_t* data;
  size_t data_len;
} xex_segment_t;

typedef struct {
  xex_segment_t segs[XEX_MAX_SEGMENTS];
  int n_segs;
  uint16_t run_addr;
  uint16_t init_addr;
} xex_parsed_t;

int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Create `src/storage/loader.cpp`**

```cpp
// loader.cpp — minimal .xex parser.

#include "loader.h"
#include <string.h>

extern "C" int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out) {
  if (!buf || !out || len < 6) return 0;
  memset(out, 0, sizeof(*out));

  size_t pos = 0;
  // Optional 0xFFFF header (must appear once at the start)
  if (buf[0] == 0xFF && buf[1] == 0xFF) pos = 2;

  while (pos + 4 <= len) {
    uint16_t start = buf[pos]     | ((uint16_t)buf[pos+1] << 8);
    uint16_t end   = buf[pos + 2] | ((uint16_t)buf[pos+3] << 8);
    pos += 4;

    // 0xFFFF 0xFFFF segment header = resync marker (skip)
    if (start == 0xFFFF && end == 0xFFFF) continue;

    if (end < start) return 0;             // malformed
    size_t data_len = (size_t)(end - start + 1);
    if (pos + data_len > len) return 0;    // truncated

    // Special segments:
    //   0x02E0/0x02E1 -> RUNAD (2 bytes, little-endian run address)
    //   0x02E2/0x02E3 -> INITAD (2 bytes)
    if (start == 0x02E0 && end == 0x02E1 && data_len == 2) {
      out->run_addr = buf[pos] | ((uint16_t)buf[pos+1] << 8);
    } else if (start == 0x02E2 && end == 0x02E3 && data_len == 2) {
      out->init_addr = buf[pos] | ((uint16_t)buf[pos+1] << 8);
    } else {
      if (out->n_segs >= XEX_MAX_SEGMENTS) return 0;
      xex_segment_t* s = &out->segs[out->n_segs++];
      s->start_addr = start;
      s->end_addr = end;
      s->data = &buf[pos];
      s->data_len = data_len;
    }
    pos += data_len;
  }

  return 1;
}
```

- [ ] **Step 5: Build and test**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R loader
```

Expected: `PASS: loader`.

- [ ] **Step 6: Commit**

```bash
git add src/storage/loader.h src/storage/loader.cpp test/test_loader.c test/CMakeLists.txt
git commit -m "loader: minimal .xex parser with host tests"
```

---

### Task 14: Load and run a .xex from SD in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Hook the loader into boot**

In `setup()`, after `Atari800_Initialise` but before the main loop starts, try to load `/atari800/test.xex` from the SD card (hardcoded path for this milestone; UI browser comes in M4).

Add helper at file scope in `src/main.cpp` (above `setup()`):

```cpp
// Exported by atari800 core (binload.c) to push a binary into memory + set PC
extern "C" int BINLOAD_Loader(const char* filename);
extern "C" void Atari800_Coldstart(void);
extern "C" uint8_t MEMORY_mem[65536];

// Load a .xex from SD into Atari memory using our own parser.
// filename is e.g. "/atari800/test.xex".
static bool try_load_xex(const char* path) {
  if (!sd_mounted) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.printf("xex: %s not found\n", path);
    return false;
  }
  size_t flen = f.size();
  if (flen == 0 || flen > 256 * 1024) {
    Serial.printf("xex: bad size %u\n", (unsigned)flen);
    f.close();
    return false;
  }
  static uint8_t xex_buf[64 * 1024];  // up to 64 KB xex files in RAM (most are far smaller)
  if (flen > sizeof(xex_buf)) {
    Serial.printf("xex: too large for buffer\n");
    f.close();
    return false;
  }
  f.read(xex_buf, flen);
  f.close();

  xex_parsed_t p;
  if (!xex_parse(xex_buf, flen, &p)) {
    Serial.println("xex: parse failed");
    return false;
  }

  // Write segments directly into the Atari core's memory map
  for (int i = 0; i < p.n_segs; i++) {
    const xex_segment_t& s = p.segs[i];
    for (size_t b = 0; b < s.data_len; b++) {
      MEMORY_mem[s.start_addr + b] = s.data[b];
    }
  }

  // Set RUN address: poke 0x02E0/0x02E1 to the parsed run_addr.
  if (p.run_addr) {
    MEMORY_mem[0x02E0] = p.run_addr & 0xFF;
    MEMORY_mem[0x02E1] = p.run_addr >> 8;
  }

  Serial.printf("xex: loaded %d segments, RUN=0x%04X\n", p.n_segs, p.run_addr);
  Atari800_Coldstart();
  return true;
}
```

Add `#include "storage/loader.h"` at the top of `src/main.cpp`. Also add:

```cpp
#include <SD.h>  // (already included for T6 but ensure)
extern "C" {
#include "../lib/atari800/src/memory.h"  // declares MEMORY_mem
}
```

In `setup()`, after the `renderer::set_region_ntsc(false);` line:

```cpp
  try_load_xex("/atari800/test.xex");
```

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: HUMAN CHECKPOINT — load a test .xex and see it run**

Prep: find any small XL/XE `.xex` file (public-domain demos work — look up "atari 8-bit demo xex download" or use a file from your own archive). File size should be ≤ 64 KB. Rename to `test.xex` and drop it on the SD card in `/atari800/test.xex` (create the folder if needed).

Flash the new firmware (`cardputer-atari800.m2-t14.bin`) the usual way.

Expected:
- Serial prints `xex: loaded N segments, RUN=0xXXXX`
- LCD shows whatever the `.xex` produces (a splash, a scene, etc.) instead of the BASIC prompt.

If the LCD shows BASIC's "READY" instead of the .xex, the parser didn't trigger `Atari800_Coldstart` or the RUN address wasn't set. Check the serial log.

If the screen garbles or hangs, that particular .xex may depend on OS ROM extensions not in AltirraOS. Try a different, simpler one.

Do not proceed until you see the .xex visibly running.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "main: load /atari800/test.xex from SD at boot (M2 hardcoded path)

M4 replaces this with a proper file browser."
```

---

### Task 15: Implement the remaining three display modes

**Files:**
- Modify: `src/display/projector.cpp`
- Modify: `test/test_projector.c`

- [ ] **Step 1: Extend host tests**

Add to `test/test_projector.c`:

```c
  /* --- Pixel-perfect (1:1 crop): output samples center 240 of the 320 source. */
  for (int i = 0; i < 320; i++) atari_line[i] = (uint8_t)i; // gradient
  palette[0x00] = 0;
  for (int i = 1; i < 256; i++) palette[i] = (uint16_t)i;

  projector_pixel_perfect_line(atari_line, out_line, palette);
  /* Output pixel 0 should correspond to source pixel 40 (= (320-240)/2) */
  CHECK(out_line[0]   == 40,  "1:1 crop starts at source pixel 40");
  CHECK(out_line[239] == 40 + 239, "1:1 crop ends at source pixel 279");

  /* --- Pillarbox: 225 center pixels + 7-8 px bar each side, aspect-preserving downscale */
  for (int i = 0; i < 320; i++) atari_line[i] = 0x10;
  palette[0x10] = 0xBEEF;
  /* Non-source columns should be 0x0000 (black bar), source columns 0xBEEF. */
  projector_pillarbox_line(atari_line, out_line, palette);
  /* Exact bar width is 7 or 8 px — accept either. Check pixel 0 is black, pixel 120 is BEEF. */
  CHECK(out_line[0]   == 0x0000, "pillarbox left bar is black");
  CHECK(out_line[120] == 0xBEEF, "pillarbox center is source color");
  CHECK(out_line[239] == 0x0000, "pillarbox right bar is black");

  /* --- Cover: 240 wide after x0.75 scale, 4-5 px source crop top/bottom
     (vertical crop is renderer's job; projector_cover_line at line level
     just does a stretch-by-x0.75 horizontally == Stretch horizontally.) */
  for (int i = 0; i < 320; i++) atari_line[i] = 0x10;
  projector_cover_line(atari_line, out_line, palette);
  for (int i = 0; i < 240; i++) CHECK(out_line[i] == 0xBEEF, "cover: solid source color");
```

- [ ] **Step 2: Implement pixel-perfect (1:1 crop)**

In `src/display/projector.cpp`, replace the `projector_pixel_perfect_line` stub:

```cpp
void projector_pixel_perfect_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Center crop: source column = 40 + x (maps out[0]..out[239] to src[40]..src[279])
  constexpr int CROP_OFFSET = (ATARI_W - LCD_W) / 2; // 40
  for (int x = 0; x < LCD_W; x++) {
    out_line[x] = palette[atari_line[ATARI_CENTER_OFFSET + CROP_OFFSET + x]];
  }
}
```

- [ ] **Step 3: Implement pillarbox**

Replace the `projector_pillarbox_line` stub:

```cpp
void projector_pillarbox_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Aspect-preserving scale 0.703 -> 225 wide, 7-8 px bars each side.
  constexpr int INNER_W = 225;                          // 320 * 0.703
  constexpr int BAR_W   = (LCD_W - INNER_W) / 2;        // 7 (with 1 leftover pixel)
  for (int x = 0; x < BAR_W; x++)            out_line[x] = 0x0000;
  for (int x = LCD_W - BAR_W; x < LCD_W; x++) out_line[x] = 0x0000;

  for (int x = 0; x < INNER_W; x++) {
    int src = (x * ATARI_W + INNER_W / 2) / INNER_W;
    if (src >= ATARI_W) src = ATARI_W - 1;
    out_line[BAR_W + x] = palette[atari_line[ATARI_CENTER_OFFSET + src]];
  }

  // Handle leftover pixel (if LCD_W - 2*BAR_W > INNER_W by 1 px, set it to black)
  if (2 * BAR_W + INNER_W < LCD_W) out_line[2 * BAR_W + INNER_W] = 0x0000;
}
```

- [ ] **Step 4: Implement cover**

Horizontally, Cover and Stretch produce the same line — both scale 320→240 by x0.75. The vertical crop (9 source rows) is the *renderer's* responsibility, not the projector's. So Cover's line implementation is the same as Stretch:

```cpp
void projector_cover_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  projector_stretch_line(atari_line, out_line, palette);
}
```

The vertical crop is handled in `renderer.cpp`'s `ATARI_VERT_OFF` computation, which will be mode-aware in T16.

- [ ] **Step 5: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R projector
```

Expected: all projector checks pass.

- [ ] **Step 6: Commit**

```bash
git add src/display/projector.cpp test/test_projector.c
git commit -m "projector: implement pixel-perfect, pillarbox, cover modes"
```

---

### Task 16: Renderer uses different vertical offsets per mode

**Files:**
- Modify: `src/display/renderer.cpp`

Currently the renderer uses one vertical offset regardless of mode. Each mode should have its own:

- **Pixel-perfect (1:1):** center-crop 135 rows of 240 source rows → source y offset = 48 (from top of active picture; varies by content)
- **Pillarbox:** source 192 rows scaled to 135 rows (x0.703) — fills the LCD vertically, no crop
- **Cover:** source 192 rows scaled to 144 rows (x0.75), then crop 4.5 rows top+bottom → 135 displayed rows, source y offset = 24 + 4 (approx)
- **Stretch:** source 192 rows scaled to 135 rows (x0.703), like Pillarbox vertically

- [ ] **Step 1: Revise renderer**

Replace the `present()` body in `src/display/renderer.cpp`:

```cpp
void present(const uint8_t* screen_atari) {
  const uint16_t* pal = use_ntsc ? palette_get_ntsc() : palette_get_pal();
  uint16_t line_buf[LCD_W];

  // Active picture top (Atari's top of visible area in Screen_atari space).
  constexpr int ATARI_ACTIVE_TOP = 24;  // skip over border/overscan
  constexpr int ATARI_ACTIVE_H   = 192;

  for (int y = 0; y < LCD_H; y++) {
    int src_y;
    switch (current_mode) {
      case Mode::PixelPerfect: {
        // 135 of 192 rows, centered
        int vcrop = (ATARI_ACTIVE_H - LCD_H) / 2;
        src_y = ATARI_ACTIVE_TOP + vcrop + y;
        break;
      }
      case Mode::Pillarbox: {
        // Scale 192 → 135 (x0.703), full height
        src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
        break;
      }
      case Mode::Cover: {
        // Scale 192 → 144 (x0.75), crop 4-5 rows top/bottom → 135 displayed
        constexpr int COVER_H = 144;
        int scaled_y = (y * ATARI_ACTIVE_H + LCD_H / 2) / COVER_H;
        // shift source up so the 135 displayed rows are centered in the 144
        int offset = (COVER_H - LCD_H) / 2;  // 4
        src_y = ATARI_ACTIVE_TOP + scaled_y + offset;
        break;
      }
      case Mode::Stretch: {
        // Scale 192 → 135 (x0.703), like Pillarbox vertically
        src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
        break;
      }
    }

    if (src_y < 0) src_y = 0;
    if (src_y >= ATARI_SRC_H) src_y = ATARI_SRC_H - 1;
    const uint8_t* atari_line = &screen_atari[src_y * ATARI_STRIDE];

    switch (current_mode) {
      case Mode::PixelPerfect: projector_pixel_perfect_line(atari_line, line_buf, pal); break;
      case Mode::Pillarbox:    projector_pillarbox_line    (atari_line, line_buf, pal); break;
      case Mode::Cover:        projector_cover_line        (atari_line, line_buf, pal); break;
      case Mode::Stretch:      projector_stretch_line      (atari_line, line_buf, pal); break;
    }
    lcd::push_line(y, line_buf, LCD_W);
  }
}
```

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/display/renderer.cpp
git commit -m "renderer: mode-aware vertical source offset for each display mode"
```

---

### Task 17: Fn+\ cycles display modes at runtime

**Files:**
- Modify: `src/main.cpp`

Per the design spec (4.3), Fn+\\ cycles through the four modes. M3 will handle the full Fn layer; here we just wire this one binding for M2 demo/testing.

- [ ] **Step 1: Detect Fn+\\ and cycle**

Add near the top of `src/main.cpp`:

```cpp
static renderer::Mode g_display_mode = renderer::Mode::Stretch;
static const char* mode_name(renderer::Mode m) {
  switch (m) {
    case renderer::Mode::PixelPerfect: return "Pixel-perfect";
    case renderer::Mode::Pillarbox:    return "Pillarbox";
    case renderer::Mode::Cover:        return "Cover";
    case renderer::Mode::Stretch:      return "Stretch";
  }
  return "?";
}
```

In the existing `loop()` keyboard handler, inside the `if (M5Cardputer.Keyboard.isChange())` branch, right before `Serial.println();`, add:

```cpp
    // Fn+\ cycles display mode (M2 shortcut; full Fn layer in M3)
    if (status.fn) {
      for (auto c : status.word) {
        if (c == '\\') {
          g_display_mode = static_cast<renderer::Mode>(
            (static_cast<int>(g_display_mode) + 1) % 4);
          renderer::set_mode(g_display_mode);
          Serial.printf("display: %s\n", mode_name(g_display_mode));
        }
      }
    }
```

- [ ] **Step 2: Build**

```bash
pio run -e cardputer-adv
```

Expected: SUCCESS.

- [ ] **Step 3: HUMAN CHECKPOINT — cycle modes**

Flash, then press Fn+\\ four times. Watch the LCD change each press:
1. → Pixel-perfect (smaller view, edges lost)
2. → Pillarbox (black bars left and right)
3. → Cover (fills screen, slight vertical crop)
4. → Stretch (back to default, full fill stretched)

Serial should print `display: <name>` each cycle.

Do not proceed until all four modes visually distinguishable on the LCD.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "main: Fn+\\ cycles display modes at runtime (M2 demo)"
```

---

### Task 18: Milestone acceptance + tag

**Files:** none

- [ ] **Step 1: Verify clean state**

```bash
git status
```

Expected: "nothing to commit, working tree clean".

- [ ] **Step 2: Full build + test**

```bash
pio run -e cardputer-adv
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: firmware builds; 3/3 tests pass (sanity, palette, projector, loader — actually 4).

- [ ] **Step 3: Tag**

```bash
git tag -a v0.2-m2 -m "Milestone 2: atari800 core integrated, first frame rendered

- Upstream atari800 5.2.0 vendored as a portable-C subset
- AltirraOS + AltirraBASIC embedded as 24 KB of code
- Port layer with hardware-blind hooks (input/sound stubs for M2)
- Palette, renderer, projector, LCD pipeline all wired
- All four display modes switchable at runtime via Fn+\\
- Hardcoded /atari800/test.xex loader from SD
- Host tests for palette, projector, .xex parser

M3 adds: real input routing, audio via POKEY, NTSC/PAL region toggle,
full Fn layer (cursor, Option/Select/Start, brightness/volume)."
```

---

## M2 acceptance checklist

- [ ] `pio run -e cardputer-adv` builds clean, flash under 80% of OTA slot
- [ ] `ctest` passes (sanity + palette + projector + loader = 4/4)
- [ ] Firmware flashed via M5Launcher boots without crashing
- [ ] Atari BASIC "READY" prompt visible on LCD (or a loaded .xex runs)
- [ ] All four display modes visibly different when cycled with Fn+\\
- [ ] Serial shows `core: init_ok=1` and per-mode names on cycle
- [ ] No regressions from M1 (keyboard serial events, SD mount status, etc.)
- [ ] Git tag `v0.2-m2` present

## What's NOT in M2 (deferred to M3+)

- Keyboard input routed to Atari core (M3)
- Joystick emulation from Cardputer keys (M3)
- POKEY audio output (M3)
- NTSC/PAL runtime toggle UX (M3)
- Full Fn layer (Option/Select/Start, brightness/volume, cursor keys) (M3)
- File browser (M4)
- User ROM override from SD `/atari800/roms/` (M4)
- Copy-on-write disk mounts (M4)
- Save states (M5)
- Machine selection (800XL / 65XE / 130XE / XEGS) UX (M3/M4)
- All error screens beyond basic crash prevention

## M3 readiness notes for future-you

- The `delay()` in `loop()` has been replaced by frame-timed `Atari800_Frame()` at ~50 Hz. Audio in M3 will need tighter pacing (48 kHz sample rate vs. 50 Hz frame rate = ~960 samples per frame). Plan audio as an I²S DMA ring fed from POKEY, consumed by ES8311, decoupled from the frame loop.
- `Atari800_Frame()` currently fires every 20 ms in wall time, not vblank-synced. A real scheduler in M3 should aim for 20 ms intervals ± drift compensation.
- `Screen_atari` pointer is fixed by the core; no concurrency hazard in a single-threaded main loop. If M3 moves audio to a FreeRTOS task, keep `Atari800_Frame()` on the main core (display + SD share that SPI bus) or route display pushes through a queue.
- The port layer's input stubs (`port_get_key`, `port_get_joy0`, `port_get_joy_fire0`) are called inside the core's `Atari800_Frame()`. When M3 replaces them with real implementations, ensure they don't block — they're called many times per frame.
