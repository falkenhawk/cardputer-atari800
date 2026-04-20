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
