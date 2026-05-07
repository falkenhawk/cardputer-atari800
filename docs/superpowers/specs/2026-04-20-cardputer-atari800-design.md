# cardputer-atari800 — design spec

- **Date:** 2026-04-20
- **Target hardware:** M5Stack Cardputer-Adv (ESP32-S3FN8, 512 KB SRAM, 8 MB flash, no PSRAM)
- **Goal:** standalone Atari 8-bit (XL/XE family) emulator firmware, flashable via M5Launcher
- **Scene alignment:** name and conventions follow upstream `atari800` emulator

## 1. Scope

### 1.1 Supported machines

`atari800` upstream is a family emulator. We expose a subset at runtime:

| Model | RAM | Notes |
|---|---|---|
| 800XL | 64 KB | |
| 65XE  | 64 KB | default profile at first boot |
| 130XE | 128 KB | adds bank switching |
| XEGS  | 64 KB | game system variant |

400/800 and 5200 are out of scope for v1. The atari800 core supports them; we can expose later without rework.

### 1.2 Supported file formats

Dispatched by extension:

- `.atr` — floppy disk image, copy-on-write at D1:
- `.xex` / `.exe` — direct-load executable (no disk)
- `.car` — cartridge with 16-byte header, auto-detects mapper
- `.rom` — raw cartridge, size auto-detected (8K / 16K / 32K); exotic mappers via pre-load picker
- `.cas` — cassette, mounted at C:
- `.bas` — BASIC source text, piped through ENTER

### 1.3 Region

PAL is the first-boot default (user's hardware was 65XE PAL). NTSC is a runtime toggle in the menu and per-game override.

## 2. Architecture

### 2.1 Approach: harvest + M5Cardputer library

We do NOT full-fork Game Station. We create a fresh PlatformIO project that uses the official `m5stack/M5Cardputer` library for hardware abstraction (covers OG and ADV transparently) and harvests specific proven pieces from `geo-tp/Cardputer-Game-Station-Emulators`:

- File-browser UI widget pattern
- Scheduler loop shape
- Audio ring buffer strategy

Everything else is written fresh against M5Cardputer's API. Rationale: ADV-specific hardware differences (TCA8418 keyboard scanner, ES8311 audio codec) mean Game Station's direct-GPIO keyboard code and NS4168 audio init
would have to be rewritten anyway. Using the official library for those eliminates the rewrite and gets ADV support for free.

### 2.2 Three-layer separation

```
┌───────────────────────────────────────────────────────┐
│  Application logic (UI, menus, settings, loaders)     │
├───────────────────────────────────────────────────────┤
│  Port layer (port.h hooks: timing, kbd, joy, snd, ...)│
│  ↕                                                     │
│  Atari800 core (upstream, pure C, hardware-blind)     │
├───────────────────────────────────────────────────────┤
│  M5Cardputer library (handles OG vs ADV transparently)│
├───────────────────────────────────────────────────────┤
│  Hardware: ESP32-S3, LCD, keyboard, speaker, SD       │
└───────────────────────────────────────────────────────┘
```

The core speaks to nothing except `port.h`. The port layer speaks to M5Cardputer.
M5Cardputer speaks to the chips. Upstream `atari800` updates can be merged without rebase conflicts.

### 2.3 Source tree

```
cardputer-atari800/
├── platformio.ini                # board target for Cardputer-Adv (exact ID verified at build time), deps, partitions
├── lib/
│   ├── atari800/                 # vendored upstream core, lightly patched for ESP32-S3
│   │   ├── src/                  #   antic.c gtia.c cpu.c pokey.c sio.c memory.c pia.c ...
│   │   ├── roms/                 #   AltirraOS + AltirraBASIC as .h byte arrays (embedded defaults)
│   │   └── port.h                #   core-facing hook declarations
│   └── atari800_port/            # thin glue between core hooks and our subsystems
├── src/
│   ├── main.cpp                  # boot, main scheduler
│   ├── display/
│   │   ├── lcd.cpp               # M5Cardputer.Display wrapper + DMA push
│   │   ├── renderer.cpp          # ANTIC/GTIA scanline → RGB565 line buffer
│   │   ├── projector.cpp         # 4 display modes
│   │   └── palette.cpp           # Atari 256-color palette → RGB565 LUTs (PAL + NTSC)
│   ├── input/
│   │   ├── keyboard.cpp          # M5Cardputer.Keyboard polling → event stream
│   │   ├── keymap.cpp            # Default / Fn / Joystick layers
│   │   └── mode.cpp              # keyboard ↔ joystick mode auto-detect
│   ├── storage/
│   │   ├── sd.cpp                # mount, enumeration, safe R/W
│   │   ├── loader.cpp            # extension-dispatched file loading
│   │   └── cow_disk.cpp          # copy-on-write .atr implementation
│   ├── audio/
│   │   ├── audio_out.cpp         # M5Cardputer.Speaker init + I²S ring
│   │   └── pokey_glue.cpp        # pokeysnd → 44.1 kHz stereo
│   ├── ui/
│   │   ├── splash.cpp            # boot splash
│   │   ├── browser.cpp           # file browser with resume-last
│   │   ├── menu.cpp              # in-emulator overlay menu (Fn+8)
│   │   └── osd.cpp               # corner status text
│   ├── state/
│   │   ├── snapshot.cpp          # save/load state
│   │   └── settings.cpp          # atari800.cfg parse + last.json
│   └── util/
│       └── log.cpp               # ring-buffer log → USB serial
└── docs/
    └── superpowers/specs/2026-04-20-cardputer-atari800-design.md
```

Note: M1 uses the stock 8 MB Arduino OTA partition layout from the board JSON. A custom partitions.csv is only needed if flashing as the primary firmware (displacing M5Launcher).

## 3. Display pipeline

### 3.1 Scanline-granular rendering

Atari games change palette and scroll registers mid-frame via Display List Interrupts. Buffering a full frame and rendering once at vblank would lose these effects.
We render one scanline at a time, in the same order the emulator produces them.

```
Atari CPU       ANTIC      GTIA          renderer.cpp      palette.cpp      projector.cpp      lcd.cpp
   │              │          │                 │                 │                 │              │
   │ write regs   │          │                 │                 │                 │              │
   │─────────────▶│          │                 │                 │                 │              │
   │              │ read DL  │                 │                 │                 │              │
   │              │────────▶ │ pixel stream    │                 │                 │              │
   │              │          │───────────────▶ │ RGB565 line buf │                 │              │
   │              │          │                 │ ──────────────▶ │ mode A/B/C/D    │              │
   │              │          │                 │                 │ ──────────────▶ │ DMA push     │
   │              │          │                 │                 │                 │ ───────────▶ LCD
```

RAM cost: one 240 × 2 byte = 480 byte line buffer (vs 62 KB full frame).

### 3.2 Four display modes

Atari native is 320 × 192. Cardputer LCD is 240 × 135. There is no clean integer scale between them, so we ship four modes; `\` (Fn+`\`) cycles at runtime.

| ID | Name | What it does | Content lost | Aspect | Default? |
|---|---|---|---|---|---|
| A | **1:1 (Pixel-perfect)** | center 240×135 of 320×192, no scaling | 80 px horiz + 57 px vert | correct | no |
| B | **Pillarbox** | scale to 225×135 (×0.703), 7–8 px bars on sides | nothing | correct | no |
| C | **Cover** | scale to 240×144 (×0.75), 4.5 px crop top and bottom | ~9 Atari rows | correct | no |
| D | **Stretch** | fill 240×135 (×0.75 h, ×0.703 v) | nothing | ~6 % off | **yes** |

### 3.3 Palette

PAL and NTSC have different GTIA hue phases. Two 256-entry RGB565 LUTs computed at boot; the region toggle points palette.cpp at the right one.

## 4. Input pipeline

### 4.1 Layers

1. **Default** — typewriter keys → Atari keyboard matrix passthrough
2. **Fn held** — console buttons, cursor, system UX (brightness/volume/display mode)
3. **Joystick mode** — dedicated clusters map to Joystick-1 instead of keyboard

Mode at emulator start:

- `.xex` / `.car` → **joystick** mode
- `.atr` / `.bas` / boot-to-BASIC → **keyboard** mode
- `Fn+J` — manual toggle, any time
- Per-game last-used mode stored in `last.json`

### 4.2 Joystick (dual cluster, simultaneous)

Both clusters continuously polled and OR'd into one logical Joystick-1.

| Direction/Action | Cluster 1 (left hand) | Cluster 2 (right hand) |
|---|---|---|
| Up | E | ; |
| Down | S | . |
| Left | A | , |
| Right | D | / |
| Fire (primary) | K | Z |
| Fire (secondary) | L | X |

### 4.3 Fn layer

| Fn + | Function | Rationale |
|---|---|---|
| 1 | Option | atari800 F1 |
| 2 | Select | atari800 F2 |
| 3 | Start | atari800 F3 |
| 4 | Warm Reset | atari800 F4 |
| 5 | Cold Reset | atari800 F5 |
| 6 | Help | atari800 F6 |
| 7 | Break | atari800 F7 |
| 8 | Firmware menu | atari800 F8 |
| 9 | Save state | quick-save slot |
| 0 | Load state | quick-load slot |
| ; | Atari cursor ↑ | printed arrow on key |
| . | Atari cursor ↓ | printed arrow on key |
| , | Atari cursor ← | printed arrow on key |
| / | Atari cursor → | printed arrow on key |
| i | Inverse video toggle | mnemonic |
| j | Toggle keyboard ↔ joystick mode | manual override |
| [ | Brightness − | Game Station convention |
| ] | Brightness + | Game Station convention |
| − | Volume − | Game Station convention |
| = | Volume + | Game Station convention |
| \ | Display mode cycle | Game Station convention |
| Esc | Back in firmware menu | — |
| (hold GO 1 s) | Exit emulator | Game Station convention |

## 5. Storage

### 5.1 SD card layout

```
/atari800/
├── roms/
│   ├── altirra-xl.rom        # embedded default; user may replace
│   ├── altirra-basic.rom     # embedded default; user may replace
│   ├── atarixl.rom           # optional user dump (preferred over default)
│   └── ataribas.rom          # optional user dump (preferred over default)
├── disks/                    # .atr files
├── cartridges/               # .car / .rom files
├── executables/              # .xex / .exe
├── basic/                    # .bas source
├── saves/                    # copy-on-write disk mirrors
├── states/                   # save-state slots
└── config/
    ├── atari800.cfg          # user-editable settings
    └── last.json             # firmware-managed last-run state
```

Folders are conventions, not filters. The file browser surfaces any valid file type found anywhere under `/atari800/`.

### 5.2 ROM override priority

At boot, for each ROM slot (OS and BASIC), check in order:

1. User dump in `/atari800/roms/` — use this if present
2. Embedded default (Altirra) — always available as fallback

Applies independently to OS ROM and BASIC ROM.

### 5.3 Disk write-back: copy-on-write

- First write to `D1:game.atr` triggers a copy of the original to `/atari800/saves/game.atr`
- Subsequent reads and writes go to the copy; the original in `/atari800/disks/` stays untouched
- User restores by deleting the save copy
- Per-session menu option to override: read-only (ignore writes) or direct-write (modify original)

### 5.4 Scratch disk

Booting into BASIC without an explicit disk, then typing `SAVE "D:MYPROG.BAS"`, auto-creates `/atari800/saves/scratch.atr` and mounts it at D1:.

### 5.5 Drives

D1: mounted by default. D2:–D4: attachable via menu (`Swap disk →`). D5:–D8: not exposed.

## 6. Audio

- **Output:** 44100 Hz, stereo, 16-bit via ES8311 codec → NS4150B amp → 1 W speaker + 3.5 mm jack
- **Stereo routing:** mono POKEY output duplicated to L + R. Stock Atari is mono.
- **Dual POKEY toggle** (menu): emulate a second POKEY; left ear = POKEY 1, right ear = POKEY 2. Authentic to the "Stereo POKEY" hardware mod scene.
- **Ring buffer:** 4096 samples stereo (~23 ms latency), DMA-fed
- **Mute-on-menu:** audio suspends while the Fn+8 overlay menu is open

## 7. UI and menus

### 7.1 Boot sequence

```
power on / reset
  ├─ 1 s splash ("cardputer-atari800 vX.Y — 65XE / 128KB / PAL")
  ├─ mount SD
  │    ├─ if fails → "NO SD CARD" screen with retry
  │    └─ else → read atari800.cfg + last.json
  ├─ if last.json says auto_resume AND state snapshot exists:
  │    load state directly into emulation
  └─ else show file browser with "▶ Resume <lastgame>" highlighted first
```

### 7.2 File browser

Milestone split: a usable full-screen ROM browser is part of the M3 baseline.
M4 improves the browser experience and adds the broader in-emulator menu and
settings persistence around it. Save-state work remains a later polish item.

- Rooted at `/atari800/`
- Shows folders and recognized file types
- Top entry = "Resume <last>" when a last-run file exists
- Cursor or dual-cluster joystick navigates, Enter/Fire loads
- Fn+8 from browser opens settings menu (region, default machine, audio, etc.)

### 7.3 In-emulator overlay menu (Fn+8)

Emulator pauses, screen dims, text menu overlays. Navigated with cursor keys
or dual-cluster joystick.

```
─── MENU ───────────────────────
  ▶ Resume
    Save state (Fn+9)
    Load state (Fn+0)
    Swap disk              →
    Cold reset (Fn+5)
    Machine: 65XE          →
    Region:  PAL / NTSC
    Display: Stretch       →
    Input:   Joystick      →
    Dual POKEY: off / on
    ─────────
    Console buttons:         ← visual grouping in the "Console buttons"
      Help   (Fn+6)             submenu mirrors the physical top-right
      Start  (Fn+3)             order of the real 65XE keyboard
      Select (Fn+2)             (Help / Start / Select / Option / Reset),
      Option (Fn+1)             even though the Fn+N bindings follow the
      Reset  (Fn+4)             atari800 F-key software convention.
    ─────────
    Exit to browser
```

### 7.4 On-screen overlays (OSD)

Small transient text in the top-right corner, auto-dismisses after 1–3 seconds:

- "SAVED" / "LOADED" / "NO SAVE STATE"
- "DISK SAVED" when cow_disk first writes
- "DISK WRITE FAILED" on write error
- Volume bar (on Fn+−/=)
- Brightness bar (on Fn+[/])
- Display mode name (on Fn+\\)

## 8. Save states

### 8.1 Format

- Raw snapshot of atari800 core state struct (CPU regs, ANTIC/GTIA/POKEY regs, full RAM, cart/disk attachment), gzipped
- Typical size: 30–60 KB
- Atomic: partial reads or corrupt snapshots do not perturb the running emulator

### 8.2 Slots

- v1: single slot per game, at `/atari800/states/<basename>.state`
- Fn+9: immediate save, overwrites previous
- Fn+0: immediate load, if file exists
- Menu exposes explicit "Save slot N / Load slot N" later (post-v1)

## 9. Settings and persistence

### 9.1 `/atari800/config/atari800.cfg`

User-editable INI. Example:

```ini
[machine]
model = 65XE          # 800XL | 65XE | 130XE | XEGS
region = PAL          # PAL | NTSC
basic = on            # boot with BASIC by default

[display]
mode = stretch        # pixel-perfect | pillarbox | cover | stretch
brightness = 70

[audio]
volume = 80
dual_pokey = off

[input]
default_input_mode = auto   # auto | keyboard | joystick
```

### 9.2 `/atari800/config/last.json`

Firmware-managed, not user-edited:

- Last loaded file path
- Per-game overrides (region, BASIC on/off, input mode, display mode)
- Auto-resume flag

### 9.3 NVS fallback

If SD missing at boot, a tiny subset (volume, brightness, last display mode)
is read from ESP32 NVS flash so the "NO SD" screen respects the user's preferences.

## 10. Boot behavior

- **Default:** BASIC on at cold boot, matches real 65XE
- **Global override:** menu setting "Boot with BASIC" on/off
- **Per-game override:** stored in last.json; games that historically need "no BASIC" (most disk games) get it auto-disabled for that run, without changing the global setting
- **Warm vs cold reset:** Fn+4 = warm (keeps RAM contents), Fn+5 = cold (power cycle). Cold reset also re-applies machine/region config.

## 11. IMU (BMI270)

Not used in v1. Reserved hook in firmware for "optional tilt-as-joystick" as a future setting. Hardware is present on ADV, absent on OG.

## 12. Error handling

### 12.1 At boot

Unrecoverable errors show a full-screen description with cause and remediation,
not a stack trace:

- `NO SD CARD — insert card and press A to retry`
- `OS ROM MISSING — expected /atari800/roms/atarixl.rom or embedded AltirraOS. Reflash firmware or add ROM.`
- `CONFIG CORRUPT — atari800.cfg is malformed (line 14). Continue with defaults? (Enter) Edit? (Esc)`

### 12.2 At runtime

- File-load errors are soft: emulator keeps running, menu shows `LOAD FAILED: <reason>`, user returns to browser
- Corrupt save states are treated as missing: delete, warn via OSD
- SD write failures during copy-on-write: disk goes read-only for the session, OSD shows `DISK WRITE FAILED`
- ESP32 panic: reboot; on next boot, firmware detects "previous run crashed" via RTC memory flag and offers `Don't auto-resume last game` to avoid crash loops

## 13. Testing

This is a hobby firmware on custom hardware. Test strategy is modest and honest.

- **Host-side compile** of pure-C parts (port glue, keymap, loader dispatch) as a CMake target on macOS. Fast iteration with asserts.
- **Golden-frame tests** for the renderer: feed known Atari ANTIC register sequences, compare RGB565 line buffer against reference PNGs.
- **Format loader tests** for `.atr` / `.xex` / `.car` parsers, including malformed and truncated files.
- **Manual hardware sanity checklist** on each release: boot, load a known game, verify all four display modes, joystick dual-cluster, Fn+8 menu, save/load state, PAL/NTSC toggle.
- **Automated hardware testing out of scope for v1.**

## 14. Build target and flash budget

- **platformio.ini** sets `board = m5stack-cardputer` (ADV support via M5Cardputer library; specific ADV board ID used if/when the ESP32 platform package distinguishes it)
- **Custom partitions** (`partitions.csv`):
  - app0 / app1 OTA slots for M5Launcher compatibility
  - nvs for config fallback
  - spiffs / littlefs reserved for future use (not required in v1; SD is primary)
- **Target sizes:**
  - Firmware: ≤ 4 MB
  - Heap at steady state: ≤ 300 KB
  - Embedded ROM payload: ≤ 32 KB (AltirraOS 16K + AltirraBASIC 8K + headroom)

## 15. Licensing

- Upstream `atari800` is GPLv2. Our firmware inherits that.
- Source code distributed under GPLv2, matching upstream.
- AltirraOS and AltirraBASIC are redistributable under their own permissive terms; packaged as embedded byte arrays.

## 16. What's explicitly out of scope for v1

- 400 / 800 / 5200 machine models
- D5:–D8: drives
- Multi-slot save states with named slots
- IMU tilt-as-joystick
- Settings Sync / cloud save
- WiFi features (TNFS / FujiNet / cloud disks)
- Stereo POKEY authoring tools
- Composite video output (no hardware path)
