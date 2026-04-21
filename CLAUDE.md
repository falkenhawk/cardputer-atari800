# cardputer-atari800 — session handoff

Atari 8-bit (XL/XE family) emulator firmware for the M5Stack Cardputer-Adv.
Built via PlatformIO + Arduino ESP32 + the official `m5stack/M5Cardputer`
library, with upstream `atari800` 5.2.0 vendored in `lib/atari800/`.

## Current state

- **Tags:** `v0.1-m1` (bootstrap + HAL), `v0.2-m2` (atari800 core + first frame).
- Master branch, clean tree.
- **Hardware-verified:** AltirraOS boot screen renders on the LCD, Fn+`\` cycles 4 display modes, heap stable, no crashes.
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

Everything big is malloc'd in `setup()` BEFORE `M5Cardputer.begin()` eats the heap:

- `Screen_atari` (96 KB) — very first allocation in setup(), before Serial.begin
- `MEMORY_mem` (65 KB) — via `ensure_memory_mem_allocated()` in memory.c
- `under_atarixl_os` / `under_cart809F` / `under_cartA0BF` (32 KB total shadow buffers) — via `ensure_under_buffers_allocated()` in memory.c
- `MEMORY_attrib` is NOT allocated as a flat array — `PAGED_ATTRIB` is defined in `lib/atari800/src/config.h`, which switches upstream atari800 to a 1 KB page-pointer table.

Without these four fixes the firmware boot-loops with StoreProhibited panics at various NULL pointer memcpy's. Do not move allocations later in setup() or revert PAGED_ATTRIB without profiling first.

## Core invariants to preserve

- `M5Cardputer` library auto-detects OG vs ADV at runtime via M5GFX. Don't hardcode board ID; let the library do it.
- `Atari800_Frame()` runs every 20 ms (50 Hz PAL). No `delay()` in loop(); frame pacing replaces it.
- Keyboard polling still prints to Serial but does NOT route to Atari core yet (that's M3's job). Only Fn+`\` is wired to something (mode cycle).
- `port_impl.cpp` stubs for sound, joystick, platform_* return neutral values. M3 replaces them with real implementations.

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
- Splash version string is bumped on each HUMAN CHECKPOINT so it's visually obvious which build was flashed. Current: `v0.2-m2-t14`.

## Starting M3

Invoke `superpowers:writing-plans` (or via `/plan` slash command) with M3 scope above. Then `superpowers:subagent-driven-development` to execute.

Before kicking off, skim the M2 plan's "memory-management arc" commits (see `git log v0.1-m1..v0.2-m2 --oneline`) — especially `9648de4`, `b5bc49d`, `d7fb77e`, `9331748`, `dc7bc0d` — they codify the memory pattern M3 must keep intact.

— Handoff written 2026-04-21, immediately after tagging `v0.2-m2`.
