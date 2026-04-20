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

Compile:

```bash
pio run -e cardputer-adv
```

The build produces `.pio/build/cardputer-adv/firmware.bin`.

## Flash

The expected workflow is via [M5Launcher](https://github.com/bmorcelli/Launcher)
installed as the boot firmware. Copy `firmware.bin` to the Cardputer's SD card
(Launcher's SD browser picks it up) or upload via Launcher's WUI (WiFi web UI).
Launcher writes it to the inactive OTA app slot and reboots into it.

```bash
cp .pio/build/cardputer-adv/firmware.bin /Volumes/CARDPUTER/downloads/cardputer-atari800.bin
```

Then on the Cardputer: disable USB-MSC in Launcher, select the bin from the SD browser.

Advanced alternative — directly flash as the *primary* firmware, **overwriting M5Launcher**:

```bash
pio run -e cardputer-adv -t upload
```

## Serial monitor

```bash
pio device monitor -e cardputer-adv
```

## Host-side tests

Pure-C logic is covered by a CMake test harness:

```bash
cmake -B build -S test
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

GPLv2 (inherited from upstream `atari800`). AltirraOS / AltirraBASIC ROMs
included under their respective redistributable terms.
