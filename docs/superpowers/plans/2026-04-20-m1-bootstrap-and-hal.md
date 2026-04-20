# cardputer-atari800 — Milestone 1: Bootstrap + HAL Smoke Test

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a PlatformIO project for the M5Stack Cardputer-Adv that boots, shows a splash screen on the LCD, reads keyboard input, mounts the microSD card, and uses M5Launcher-compatible flash partitions. This is the foundation every subsequent milestone builds on; no Atari code in this milestone.

**Architecture:** PlatformIO + Arduino ESP32 framework + official `m5stack/M5Cardputer` library as the hardware abstraction layer. Source tree follows the spec (`/src/display/`, `/src/input/`, `/src/storage/`, etc.) but this milestone touches only `/src/main.cpp`, `/src/util/log.cpp`, and the PlatformIO configuration files. We also set up a parallel host-side CMake target for future pure-C testing.

**Tech Stack:** PlatformIO Core (CLI), Arduino ESP32 core 2.x, `m5stack/M5Cardputer` library, CMake (for host tests later).

**Prerequisites (must be installed by user before Task 1):**
- `brew install platformio cmake`
- Cardputer-Adv connected via USB-C and recognized by macOS
- Spare microSD card (FAT32, any size) for Task 6

**Reference docs for agent:**
- Spec: `docs/superpowers/specs/2026-04-20-cardputer-atari800-design.md`
- M5Cardputer library: https://github.com/m5stack/M5Cardputer
- PlatformIO ESP32 docs: https://docs.platformio.org/en/latest/platforms/espressif32.html

**Testing approach:** Embedded hardware bring-up is not strictly TDD-able — most tasks require a physical device. Tasks with pure logic (none in M1) get unit tests. Tasks requiring hardware have an explicit **HUMAN CHECKPOINT** step where the human flashes and observes. Do NOT mark a task complete until the human has confirmed the observation.

**Flashing workflow:** The user keeps [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) (M5Launcher) as the persistent boot firmware on the Cardputer-Adv and flashes `cardputer-atari800.bin` as an OTA app. This means:

- Build produces `.pio/build/cardputer-adv/firmware.bin` (raw app image, no bootloader/partition table — which is exactly what Launcher expects).
- To flash: either copy the bin to the SD card (M5Launcher's SD browser picks it up) or upload via M5Launcher's WUI (WiFi web UI). Launcher writes it to the inactive OTA app slot and reboots into it.
- `pio run -t upload` (direct esptool flash) is NOT used for M1 testing because it would overwrite M5Launcher. Keep it as a fallback for the day the user wants cardputer-atari800 to be the boot firmware.
- Serial monitoring works identically either way: `pio device monitor -e cardputer-adv` reads the USB CDC serial of whatever app is currently running.

**Implication for Task 7 (custom partitions):** Launcher's partition layout is what's actually in flash at runtime on the user's device. Our `partitions.csv` from Task 7 only applies if someone flashes this firmware directly (not via Launcher). We still create it so that path stays viable, but M1 verification happens under Launcher's partition layout.

---

### Task 1: Initialize PlatformIO project

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `src/main.cpp`

- [ ] **Step 1: Create `platformio.ini`**

```ini
; PlatformIO Project Configuration File — cardputer-atari800
; https://docs.platformio.org/page/projectconf.html

[env:cardputer-adv]
platform = espressif32
framework = arduino
board = m5stack-cardputer

; serial / USB
monitor_speed = 115200
upload_speed = 921600

; C / C++ standards
build_unflags = -std=gnu++11
build_flags =
  -std=gnu++17
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DCORE_DEBUG_LEVEL=3

; libraries — only bring in M5Cardputer for now; atari800 core added in M2
lib_deps =
  m5stack/M5Cardputer@^1.0.3

; flash / filesystem
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
```

**Note on `board = m5stack-cardputer`:** at the time of writing, the Espressif platform package does not distinguish OG vs ADV. The `M5Cardputer` library auto-detects ADV at runtime and uses the correct keyboard scanner / audio codec. If a dedicated `m5stack-cardputer-adv` board ID is added to espressif32 later, switch to it.

- [ ] **Step 2: Create `.gitignore`**

```gitignore
# PlatformIO
.pio/
.pioenvs/
.piolibdeps/
.vscode/

# CMake (host tests)
build/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# macOS
.DS_Store

# brainstorming artifacts (already present, confirming)
.superpowers/
```

- [ ] **Step 3: Create empty `src/main.cpp`**

```cpp
// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

#include <Arduino.h>

void setup() {
  // populated in Task 2
}

void loop() {
  // populated in Task 2
}
```

- [ ] **Step 4: Verify PlatformIO accepts the config**

Run: `pio run -e cardputer-adv --target checkprogsize`

Expected: PlatformIO downloads the ESP32 toolchain on first run (~500 MB, one-time), compiles `main.cpp`, and reports firmware size. First run takes 1–5 min; subsequent runs are fast.

If it errors on `m5stack/M5Cardputer`, check the exact published version on https://registry.platformio.org/libraries/m5stack/M5Cardputer and update the `@^1.0.3` constraint.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini .gitignore src/main.cpp
git commit -m "initialize PlatformIO project for Cardputer-Adv"
```

---

### Task 2: Minimal boot with serial hello

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Populate `src/main.cpp`**

```cpp
// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  // small delay so the USB CDC has time to enumerate before first print
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");
  Serial.println("milestone 1: bootstrap + HAL smoke test");
}

void loop() {
  // idle heartbeat — proves the main loop is alive
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 5000) {
    last = now;
    Serial.printf("uptime %lu ms\n", now);
  }
  delay(10);
}
```

- [ ] **Step 2: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds, firmware binary produced under `.pio/build/cardputer-adv/firmware.bin`. Report shows RAM/flash usage for the empty program (should be tiny — a few % of flash).

- [ ] **Step 3: HUMAN CHECKPOINT — first flash**

Plug the Cardputer-Adv into USB-C. Run:

```bash
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

Expected serial output:
```
cardputer-atari800 — boot
milestone 1: bootstrap + HAL smoke test
uptime 5000 ms
uptime 10000 ms
...
```

If upload fails with "port not found", list ports: `pio device list`. On the first upload you may need to hold the Cardputer's BOOT button while plugging in (rare on ADV but possible). If the serial monitor shows nothing, verify `ARDUINO_USB_CDC_ON_BOOT=1` is in `build_flags`.

Do not proceed until human confirms the serial output is visible.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "boot with serial hello + idle heartbeat"
```

---

### Task 3: Initialize M5Cardputer library (build-only check)

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add M5Cardputer include and init**

Replace `src/main.cpp` entirely:

```cpp
// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

#include <Arduino.h>
#include <M5Cardputer.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enableKeyboard (default)

  Serial.println("M5Cardputer library initialized");
}

void loop() {
  M5Cardputer.update();  // required each loop for keyboard/button state

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 5000) {
    last = now;
    Serial.printf("uptime %lu ms\n", now);
  }
  delay(10);
}
```

- [ ] **Step 2: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds. Flash/RAM usage jumps (the library pulls in LGFX, SD, etc.) but still well under limits.

If the build fails with a missing header, confirm `M5Cardputer` actually installed into `.pio/libdeps/cardputer-adv/`. If needed, force a re-fetch:

```bash
pio pkg install -e cardputer-adv
```

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "integrate M5Cardputer library (begin() in setup)"
```

---

### Task 4: Splash screen on LCD

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Render splash**

Replace `setup()` in `src/main.cpp`:

```cpp
// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enableKeyboard (default)

  // splash screen
  auto& d = M5Cardputer.Display;
  d.setRotation(1);                        // landscape, text-friendly
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.setCursor(8, 16);
  d.print("cardputer-atari800");
  d.setCursor(8, 32);
  d.print("v0.1-m1");
  d.setCursor(8, 56);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.print("bootstrap + HAL smoke");

  Serial.println("splash rendered");
}
```

- [ ] **Step 2: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds.

- [ ] **Step 3: HUMAN CHECKPOINT — verify LCD**

```bash
pio run -e cardputer-adv -t upload
```

Expected on the Cardputer-Adv LCD: three lines of white/grey text on a black background in landscape orientation:
```
cardputer-atari800
v0.1-m1
bootstrap + HAL smoke
```

If the display is upside down or in portrait, try `setRotation(3)` or `setRotation(0)` respectively. If the display stays black, confirm `M5Cardputer.begin()` is called before `Display.` calls.

Do not proceed until human confirms splash is visible.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "render boot splash on LCD"
```

---

### Task 5: Keyboard polling loop

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add keyboard polling + serial print**

Replace `loop()` in `src/main.cpp`:

```cpp
void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange()) {
    auto status = M5Cardputer.Keyboard.keysState();

    // print modifier state
    Serial.print("keys:");
    if (status.ctrl)  Serial.print(" CTRL");
    if (status.shift) Serial.print(" SHIFT");
    if (status.alt)   Serial.print(" ALT");
    if (status.fn)    Serial.print(" FN");
    if (status.opt)   Serial.print(" OPT");  // dedicated Opt key (2nd from left on bottom row)

    // print printable characters
    for (auto c : status.word) {
      Serial.printf(" '%c'(0x%02x)", c, c);
    }
    // print HID key codes (for non-printable keys like Enter, Esc, Backspace)
    for (auto k : status.hid_keys) {
      Serial.printf(" hid=0x%02x", k);
    }
    Serial.println();
  }

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 10000) {
    last = now;
    Serial.printf("uptime %lu ms\n", now);
  }
  delay(10);
}
```

- [ ] **Step 2: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds.

- [ ] **Step 3: HUMAN CHECKPOINT — type some keys**

```bash
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

Press these on the Cardputer keyboard and confirm the following serial output patterns:

| Press | Expected output |
|---|---|
| `a` | `keys: 'a'(0x61)` |
| `A` (shift+a) | `keys: SHIFT 'A'(0x41)` |
| `Enter` | `keys: hid=0x28` (or similar) |
| `Fn` alone | `keys: FN` |
| `Fn` + `1` | `keys: FN '1'(0x31)` |
| `Ctrl` + `c` | `keys: CTRL` followed by another event with `'c'` |

The exact HID codes for non-printable keys may differ — goal is just to confirm the library is delivering events. Any output at all confirms the keyboard scanner (TCA8418 on ADV) is working.

Do not proceed until human confirms keypress events appear on serial.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "poll Cardputer keyboard and print events to serial"
```

---

### Task 6: SD card mount + root listing

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add SD mount and graceful fail**

Add this helper above `setup()` in `src/main.cpp`:

```cpp
#include <SD.h>
#include <SPI.h>

// Cardputer-Adv SD card pins (from M5Cardputer hardware reference)
// Confirm these against the schematic if the mount fails.
static constexpr int SD_PIN_SCK  = 40;
static constexpr int SD_PIN_MISO = 39;
static constexpr int SD_PIN_MOSI = 14;
static constexpr int SD_PIN_CS   = 12;

static bool sd_mounted = false;

static bool mount_sd() {
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, SPI, 25000000)) {
    Serial.println("SD: mount failed (no card? wrong format? wrong pins?)");
    return false;
  }
  uint64_t size_mb = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD: mounted, %llu MB\n", size_mb);
  return true;
}

static void list_sd_root() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("SD: could not open root");
    return;
  }
  Serial.println("SD: root listing:");
  while (File f = root.openNextFile()) {
    Serial.printf("  %s%s  %u bytes\n",
                  f.isDirectory() ? "DIR " : "    ",
                  f.name(),
                  (unsigned)f.size());
    f.close();
  }
  root.close();
}
```

Append to `setup()` after the splash code:

```cpp
  sd_mounted = mount_sd();
  if (sd_mounted) {
    list_sd_root();

    // also show mount status on LCD
    auto& d = M5Cardputer.Display;
    d.setCursor(8, 80);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.print("SD: mounted");
  } else {
    auto& d = M5Cardputer.Display;
    d.setCursor(8, 80);
    d.setTextColor(TFT_RED, TFT_BLACK);
    d.print("SD: not mounted");
  }
```

- [ ] **Step 2: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds.

- [ ] **Step 3: HUMAN CHECKPOINT — SD card cases**

Prep: format a spare microSD card to FAT32, create a folder `/atari800/` on it, drop a text file `/atari800/hello.txt` containing "hi".

**Case A: card inserted**

Insert card. Flash:
```bash
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

Expected:
- Serial prints `SD: mounted, <N> MB` then the root listing (should include `atari800/`).
- LCD shows "SD: mounted" in green below the splash.

**Case B: no card**

Power off (unplug USB), remove SD card, power back on:
```bash
pio device monitor -e cardputer-adv
```

Expected:
- Serial prints `SD: mount failed (...)`.
- LCD shows "SD: not mounted" in red.
- Firmware does not crash — heartbeat still prints.

If the mount fails even with a valid card, the SD pin constants may be wrong for ADV — check the Cardputer-Adv schematic (linked from m5-docs) and update `SD_PIN_SCK/MISO/MOSI/CS`. On OG Cardputer these are SCK=40, MISO=39, MOSI=14, CS=12; ADV should be the same but verify.

Do not proceed until both cases are verified.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "mount microSD and list root; graceful fallback on no-card"
```

---

### Task 7: Custom partitions for M5Launcher compatibility

**Files:**
- Create: `partitions.csv`
- Modify: `platformio.ini`

- [ ] **Step 1: Create `partitions.csv`**

This layout matches M5Launcher's expectations: two OTA app slots (so Launcher can flash a new firmware to the inactive slot and swap), plus NVS for settings fallback. Total = 8 MB.

```csv
# Name,   Type, SubType,  Offset,   Size,     Flags
nvs,      data, nvs,      0x9000,   0x5000,
otadata,  data, ota,      0xe000,   0x2000,
app0,     app,  ota_0,    0x10000,  0x3E0000,
app1,     app,  ota_1,    0x3F0000, 0x3E0000,
spiffs,   data, spiffs,   0x7D0000, 0x30000,
```

Sizes: app0 and app1 each = 0x3E0000 = 3,997,696 bytes (~3.8 MB) each. This gives us comfortable headroom for the full v1 firmware without blocking the OTA swap. SPIFFS reserved 192 KB for future use; not required in v1 (SD is primary storage).

- [ ] **Step 2: Reference from `platformio.ini`**

Add inside the `[env:cardputer-adv]` section:

```ini
board_build.partitions = partitions.csv
```

- [ ] **Step 3: Build**

Run: `pio run -e cardputer-adv`

Expected: build succeeds. Output should mention the custom partition table was used. Flash usage should report against the ~3.8 MB app0 slot size.

- [ ] **Step 4: HUMAN CHECKPOINT — reflash and verify**

```bash
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

Expected: firmware still boots and runs normally (splash + SD mount). Switching partition schemes wipes NVS once; this is expected on first boot with the new layout.

Optional: if you have M5Launcher already flashed on another device or in M5Burner, verify it can see the firmware as a valid OTA target. Not required for milestone completion.

- [ ] **Step 5: Commit**

```bash
git add partitions.csv platformio.ini
git commit -m "add M5Launcher-compatible partition layout"
```

---

### Task 8: Host-side CMake test harness (for future pure-C tests)

This task sets up the infrastructure M2+ will use for TDD on pure-C code (port glue, loaders, keymap logic). It isolates host-testable code from firmware-only code so tests run fast on macOS without touching hardware.

**Files:**
- Create: `test/CMakeLists.txt`
- Create: `test/test_sanity.c`
- Create: `test/README.md`

- [ ] **Step 1: Create `test/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(cardputer_atari800_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# All host tests live here. Each test is a standalone executable for easy
# isolation — ctest runs them and reports failures individually.
enable_testing()

add_executable(test_sanity test_sanity.c)
add_test(NAME sanity COMMAND test_sanity)
```

- [ ] **Step 2: Create `test/test_sanity.c`**

```c
// Smoke test — proves the CMake host toolchain is wired up. Replaced in M2
// by actual tests of the port layer / loaders / keymap.

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  // trivial invariant; a real test would assert against a known expected value
  int two_plus_two = 2 + 2;
  if (two_plus_two != 4) {
    fprintf(stderr, "FAIL: 2+2 = %d, expected 4\n", two_plus_two);
    return EXIT_FAILURE;
  }
  printf("PASS: sanity\n");
  return EXIT_SUCCESS;
}
```

- [ ] **Step 3: Create `test/README.md`**

```markdown
# Host-side tests

Pure-C code in the cardputer-atari800 codebase that has no hardware
dependencies is tested here on the host machine. This is much faster than
flashing to the device and catches logic bugs before they ever touch hardware.

## Running

```bash
cd test
cmake -B ../build .
cmake --build ../build
ctest --test-dir ../build --output-on-failure
```

## Adding tests

For each new pure-C module under `src/` that has a testable interface:

1. Write the test as `test/test_<module>.c`
2. Add `add_executable(test_<module> test_<module>.c <sources>)` to CMakeLists.txt
3. Add `add_test(NAME <module> COMMAND test_<module>)`
4. Run locally first — iteration is seconds, not minutes

Tests in this directory run on macOS (or any POSIX host). They never run on
the ESP32; they test the pure logic that happens to also run on the ESP32.
```

- [ ] **Step 4: Verify the test harness runs**

```bash
cd test
cmake -B ../build .
cmake --build ../build
ctest --test-dir ../build --output-on-failure
cd ..
```

Expected output (final ctest summary):
```
Test project build
    Start 1: sanity
1/1 Test #1: sanity ...........................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 1
```

If cmake fails with "CMake not found", confirm `brew install cmake` was done (prerequisite).

- [ ] **Step 5: Commit**

```bash
git add test/
git commit -m "add CMake host-test harness with sanity check"
```

---

### Task 9: Project README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README**

```markdown
# cardputer-atari800

Atari 8-bit (800XL / 65XE / 130XE / XEGS) emulator firmware for the
M5Stack Cardputer-Adv.

Based on the upstream [atari800](https://atari800.github.io/) emulator core,
ported to the ESP32-S3 running Arduino framework via PlatformIO.

## Status

Early development. See `docs/superpowers/specs/` for the design spec and
`docs/superpowers/plans/` for milestone plans.

- [x] M1 — bootstrap + HAL smoke test
- [ ] M2 — atari800 core integration + first rendered frame
- [ ] M3 — input routing + audio
- [ ] M4 — UI: file browser + in-emulator menu
- [ ] M5 — save states + polish

## Build

Requires PlatformIO Core:

```bash
brew install platformio
```

Then:

```bash
pio run -e cardputer-adv             # compile
pio run -e cardputer-adv -t upload   # flash
pio device monitor -e cardputer-adv  # serial
```

## Host-side tests

Pure-C logic is covered by a CMake test harness:

```bash
cd test && cmake -B ../build . && cmake --build ../build && ctest --test-dir ../build
```

## License

GPLv2 (inherited from upstream `atari800`). AltirraOS / AltirraBASIC ROMs
included under their respective redistributable terms.
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "add project README"
```

---

### Task 10: Milestone 1 tag

**Files:** (none, just a tag)

- [ ] **Step 1: Verify clean state**

Run: `git status`

Expected: "nothing to commit, working tree clean"

- [ ] **Step 2: Tag the milestone**

```bash
git tag -a v0.1-m1 -m "Milestone 1: bootstrap + HAL smoke test

Firmware boots on Cardputer-Adv, shows splash on LCD, polls the
TCA8418 keyboard, mounts microSD, uses M5Launcher-compatible
partitions, has a working host-side CMake test harness."
```

- [ ] **Step 3: Verify**

Run: `git tag -l -n1`

Expected: shows `v0.1-m1` with the message.

---

## M1 acceptance checklist

Before declaring M1 complete, walk through this list on real hardware:

- [ ] `pio run -e cardputer-adv` compiles cleanly (no warnings about M5Cardputer or Arduino APIs)
- [ ] `pio run -e cardputer-adv -t upload` flashes successfully
- [ ] Serial monitor shows `cardputer-atari800 — boot` immediately on reset
- [ ] LCD shows 3-line splash in landscape orientation
- [ ] Keyboard events print on serial (letter keys, Fn, Shift)
- [ ] With SD card inserted (FAT32, `/atari800/` folder present), serial shows root listing AND LCD shows "SD: mounted" in green
- [ ] Without SD card, serial shows mount-failed message AND LCD shows "SD: not mounted" in red; no crash
- [ ] `ctest --test-dir build --output-on-failure` reports 1 passed / 0 failed
- [ ] `git log --oneline` shows ~9 commits (one per task except the tag)
- [ ] `git tag -l` shows `v0.1-m1`

When every box is checked, hand off to M2 planning.

---

## What's NOT in M1 (deferred to M2+)

- Any `atari800` core code — the `lib/atari800/` tree does not exist yet
- Display scanline rendering — splash is static
- Input-to-Atari translation — keyboard events only go to serial
- Audio output — `src/audio/` doesn't exist yet
- Settings / config parsing — hard-coded behavior
- File browser UI — SD is read but not navigated
- AltirraOS / AltirraBASIC embedding — no emulator to feed them to
- Save states — no emulator state to snapshot
- Error screens beyond "SD: not mounted" — basic cases only
- `.superpowers/brainstorm/` contents — already git-ignored
