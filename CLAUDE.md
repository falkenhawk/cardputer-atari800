# cardputer-atari800 — session handoff

Atari 8-bit (XL/XE family) emulator firmware for the M5Stack Cardputer-Adv.
Built via PlatformIO + Arduino ESP32 + the official `m5stack/M5Cardputer`
library, with upstream `atari800` 5.2.0 vendored in `lib/atari800/`.

## Current state

- **Tags:** `v0.1-m1` (bootstrap + HAL), `v0.2-m2` (atari800 core + first frame).
  NOTE: the existing `v0.2-m2` tag points at a premature commit — the T14 xex-loading
  + SD-mount work that followed is on master but not yet re-tagged. When you're happy
  with the state, move the tag forward with `git tag -f v0.2-m2 HEAD`.
- Master branch, clean tree. HEAD splash: `v0.2-m2-t14q`.
- **Hardware-verified:** SD mounts, `/sd/atari800/test.xex` loads via `BINLOAD_Loader`,
  the 6502 executes it (horizontal colored bars render on LCD — static but correct —
  the xex is a `LDA $D40B / STA $D018 / JMP $2000` loop), Fn+`\` cycles 4 display
  modes, ~16 KB free heap at steady state, no crashes.
- Build: RAM 45.9%, Flash 18.8%. Host tests 4/4 (sanity, palette, projector, loader).

## Hardware (important quirks)

- **M5Stack Cardputer-Adv** — ESP32-S3FN8, 512 KB SRAM, 8 MB flash.
- **No PSRAM.** `ESP.getFreePsram()` returns 0. All big allocations must go to the ~320 KB internal DRAM heap.
- TCA8418 I²C keyboard scanner (different from OG Cardputer's direct-GPIO). ES8311 audio codec. BMI270 IMU (unused in v1).
- LCD: 240x135 ST7789V2 SPI. Separate SPI bus from SD card.

## How to build, flash, monitor

```bash
pio run -e cardputer-adv              # compile → .pio/build/cardputer-adv/firmware.bin
```

Flashing uses M5Launcher as persistent boot firmware, our app in the OTA app1 slot. Two options:

- **Esptool over USB (download mode):** unplug USB, hold `G0`, plug back in, release, then:
  ```bash
  pio pkg exec -- esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 \
    --baud 921600 write_flash 0x170000 .pio/build/cardputer-adv/firmware.bin
  ```
  This writes ONLY app1, keeps Launcher intact in app0.
- **SD + Launcher:** copy `firmware.bin` to Cardputer's SD `/downloads/`, flash via Launcher's SD browser.

Serial monitor on macOS is flaky with `pio device monitor` on USB-CDC — Cardputer USB re-enumerates between ports (`/dev/cu.usbmodem101` vs `1101`). Use a reconnecting `cat` loop instead:

```bash
for i in $(seq 1 30); do
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
  [ -n "$PORT" ] && cat "$PORT"
  sleep 1
done
```

## Source tree orientation

```
src/
  main.cpp              # boot, frame loop, keyboard diag, Fn+\ mode cycle, xex loader call
  display/
    palette.{h,cpp}     # PAL + NTSC 256-color RGB565 LUTs
    lcd.{h,cpp}         # M5Cardputer.Display wrapper
    renderer.{h,cpp}    # Screen_atari → per-line project → lcd::push_line
    projector.{h,cpp}   # 4 display modes (Stretch, Pillarbox, Cover, Pixel-perfect)
  storage/
    loader.{h,cpp}      # .xex parser (M2 subset)
  port_impl.cpp         # Atari core port hooks (timing, ROM loaders, Screen_Initialise,
                        # input/sound stubs, PLATFORM_* stubs, weak display fallback)
  port_display.cpp      # strong port_present_frame → renderer::present

lib/
  atari800/             # upstream atari800 5.2.0 core (GPLv2), minimally patched
  atari800_port/        # unused since LDF needed port code in /src/; retained as docs

test/
  test_palette.c  test_projector.c  test_loader.c  test_sanity.c
  CMakeLists.txt        # host-side tests via ctest, -Wall -Wextra -Wpedantic

docs/superpowers/
  specs/2026-04-20-cardputer-atari800-design.md   # FULL design spec, read this first
  plans/2026-04-20-m1-bootstrap-and-hal.md        # M1 plan (done)
  plans/2026-04-20-m2-core-and-first-frame.md     # M2 plan (done)
```

## The hard-won memory-management pattern

Allocation order in `setup()` is load-bearing — the total contiguous need is
~173 KB (Screen 92 KB + MEMORY_mem 65 KB + under_xlos 16 KB) and the largest
contiguous block at `heap@entry` is 196 KB. After SD mount it's only 172 KB,
just 1 KB short — so SD must mount LAST:

1. `Screen_atari` (92 KB = 384 × 240) via `malloc` at top of setup(). Exactly
   `Screen_WIDTH * Screen_HEIGHT` bytes — do NOT add padding, we don't have the
   heap for it. atari800 core's antic.c never writes past Screen_HEIGHT rows.
2. `MEMORY_mem` (65 538) via `ensure_memory_mem_allocated()`.
3. `under_atarixl_os` (16 KB) + `under_cart809F` (8 KB) + `under_cartA0BF` (8 KB)
   via `ensure_under_buffers_allocated()`. The 16 KB one slots into the big
   block's leftover; the two 8 KB ones find DRAM1 small-block gaps.
4. `mount_sd()` — FATFS state (~27 KB) fits into post-alloc fragments.
5. `M5Cardputer.begin()`, renderer, etc.

`MEMORY_attrib` is NOT a flat 64 KB array — `PAGED_ATTRIB` in
`lib/atari800/src/config.h` switches atari800 to a 1 KB page-pointer table.

Without this ordering the firmware boot-loops with `StoreProhibited` panics at
NULL-deref'd memcpy's in `MEMORY_HandlePORTB`. Do not move allocations or
revert `PAGED_ATTRIB` without profiling first.

### Tight-heap debugging toolkit (keep — cheap and invaluable)

- `heap_caps_get_info(…, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)` at entry shows
  per-region free/largest/min_free/blocks. Critical when one alloc "silently"
  fails — the difference between 8192-byte request and 8180-byte largest is
  invisible in `ESP.getFreeHeap()` alone.
- `debug_get_under_*` getters in `memory.c` expose static pointers for
  `dump_shadow_ptrs()` in main.cpp — verifies the three shadow buffers stay
  valid across frames. Catches both "alloc silently failed" and "pointer got
  corrupted later" cases.
- Per-step heap prints (`heap@post-screen`, `heap@post-mem-alloc`, …) let
  you see exactly which allocation ran out of contiguous space.

### Latent bug class watch: silent partial-failure allocs

`ensure_under_buffers_allocated` allocates three pointers; if any `malloc`
returns NULL the pointer stays NULL and the function returns normally.
`MEMORY_HandlePORTB` then derefs NULL+0x1000 on the first bank switch that
exercises that buffer. This was masked for a long time because only the
xlos buffer was exercised during basic XL/XE boot — cartA0BF's NULL
wouldn't manifest until the 0xA000-0xBFFF cart region toggled.

## Core invariants to preserve

- `M5Cardputer` library auto-detects OG vs ADV at runtime via M5GFX. Don't hardcode board ID; let the library do it.
- `Atari800_Frame()` runs every 20 ms (50 Hz PAL). No `delay()` in loop(); frame pacing replaces it.
- Keyboard polling still prints to Serial but does NOT route to Atari core yet (that's M3's job). Only Fn+`\` is wired to something (mode cycle).
- `port_impl.cpp` stubs for sound, joystick, platform_* return neutral values. M3 replaces them with real implementations.

## xex loading

Use `BINLOAD_Loader(const char* vfs_path)` from atari800 core — NOT hand-rolled
poke-into-MEMORY_mem + Atari800_Coldstart(). Coldstart zeroes RAM *after* your
pokes, so hand-rolled loading appears to work in logs but the 6502 never sees
the code. `BINLOAD_Loader` uses `fopen()` which Arduino's `SD.begin()` routes
through ESP-IDF VFS at `/sd/…`, then sets `BINLOAD_start_binloading=TRUE` and
lets the core's SIO layer inject a fake boot sector that drives proper segment
loading + RUNAD/INITAD handling.

Path format from main.cpp: translate `/atari800/test.xex` to `/sd/atari800/test.xex`.

## What M3 needs to deliver (per the design spec)

Keyboard → Atari input wiring per the Fn layer defined in spec section 4.3:
- Default keys → Atari keyboard matrix (KBCODE)
- `Fn+1..8` → console buttons (Option/Select/Start/Reset/Help/Break + menu/save state)
- `Fn + ; . , /` → Atari cursor (maps to Ctrl+`-`/`=`/`+`/`*` at matrix level)
- Dual-cluster joystick: `ESAD+K/L` and `; , . / + Z/X` (simultaneous, OR'd into Joystick-1)
- Auto-detect keyboard vs joystick mode per loaded file (`.atr/.bas` → keyboard, `.xex/.car` → joystick), `Fn+J` to override

POKEY audio via the ES8311 codec at 44.1 kHz stereo. Single-POKEY default with a menu toggle for dual-POKEY stereo. Mute on menu open. Heap is TIGHT (~36 KB free at steady state) — the I²S ring buffer must be allocated at setup(), not runtime.

NTSC/PAL runtime toggle. Machine-model picker (800XL / 65XE / 130XE / XEGS). Probably minimum menu UI to flip these — full menu is M4.

## Session conventions

- Auto mode (`/loop` or the session `auto` toggle) has been fine for this project. User prefers terse, direct exchanges.
- Subagent-driven-development worked well: fresh subagent per task → spec review → code review. See the M1/M2 commit history for the cadence.
- User prefers M5Launcher-flash-to-app1 over `pio run -t upload`. The Launcher layout is `app0 (test, 1.4 MB) + app1 (ota_0, 5 MB)` at offsets `0x10000` and `0x170000` (confirmed by reading the partition table in download mode).
- Splash version string is bumped on each HUMAN CHECKPOINT so it's visually obvious which build was flashed. Current: `v0.2-m2-t14q`. Also `FW_VER=…` is printed on Serial at boot — the LCD splash is easy to miss, Serial isn't.
- When flashing via Launcher → SD: name the file with a version suffix
  (`cardputer-atari800.m2-t14q.bin`) so Launcher's browser unambiguously shows
  which build is selected. We once burned an hour chasing a "fix not working"
  that was actually just Launcher picking an older file with the same name.
- Always verify the firmware on SD matches the one you just built:
  `shasum -a 256 /Volumes/CARDPUTER/downloads/<file>.bin .pio/build/cardputer-adv/firmware.bin`
  must match before ejecting.
- `diskutil unmount force` does NOT flush macOS write-back cache to SD. Use
  `sync && sleep 2 && diskutil unmount force` or (better) `diskutil eject`.

## Gotchas observed this session (don't repeat)

- **Don't misread crash-dump field names.** `EXCVADDR` and `LCOUNT` appear on
  the same line of ESP-IDF's panic output; a truncated serial capture made me
  read `LCOUNT: 0x7F` as `EXCVADDR: 0x7F` and hypothesize a new crash site
  when it was the same old NULL-deref. Always grab the FULL panic block.
- **ESP_ERR codes: 0x101 is `ESP_ERR_NO_MEM`, NOT `ESP_ERR_INVALID_STATE`
  (which is 0x103).** A heap-fragmentation failure in `esp_vfs_fat_register`
  ate hours of investigation because I'd memorized the wrong mapping.
- **`ESP.getFreeHeap()` lies by omission.** Total free can be 46 KB while
  largest contiguous is 17 KB — totally different bug profile. Use
  `heap_caps_get_info()` or at minimum `ESP.getMaxAllocHeap()` when chasing
  alloc failures.
- **Don't blame M5Launcher.** Spent hours hypothesizing that Launcher's OTA
  boot-handoff was leaving peripherals / VFS / FatFs slot in a weird state
  that broke SD mount. Proved that wrong by flashing as primary firmware
  (overwriting Launcher) — same failure reproduced. The real cause was
  internal heap fragmentation from our own pre-allocations. When Launcher is
  the "different thing" in a reproducing failure, verify its involvement with
  a primary-firmware flash FIRST before building fixes around it.

## Starting M3

Invoke `superpowers:writing-plans` (or via `/plan` slash command) with M3 scope above. Then `superpowers:subagent-driven-development` to execute.

Before kicking off, skim the M2 plan's "memory-management arc" commits (see `git log v0.1-m1..v0.2-m2 --oneline`) — especially `9648de4`, `b5bc49d`, `d7fb77e`, `9331748`, `dc7bc0d` — they codify the memory pattern M3 must keep intact.

— Handoff written 2026-04-21, updated 2026-04-22 after the T14 xex-loading
  + heap-packing work. M2 is functionally complete pending a tag move.
