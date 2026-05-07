# cardputer-atari800

Atari 8-bit (800XL / 65XE / 130XE / XEGS) emulator firmware for the
M5Stack Cardputer-Adv.

Based on the upstream [atari800](https://atari800.github.io/) emulator core,
ported to the ESP32-S3 running Arduino framework via PlatformIO.

## Status

Current working reference firmware: `v0.3-m3-t11a-renderperf`.

- [x] M1 - bootstrap + HAL smoke test
- [x] M2 - atari800 core integration + first rendered frame
- [x] M3 - keyboard/joystick routing, fast POKEY audio, usable ROM browser
- [ ] M4 - browser polish, full in-emulator menu, settings persistence
- [ ] M5 - save states + final polish

Implemented beyond the original M2 baseline: AltirraBASIC boot, SD ROM/ATR/XEX
loading via Fn+L, fast ROM browser enumeration, Cardputer keyboard + joystick
mapping, raw ES8311/I2S audio, Atari console/key-click sounds, and dirty-row LCD
redraw optimization.

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

The normal workflow keeps [M5Launcher](https://github.com/bmorcelli/Launcher) as
the boot firmware and writes this app into Launcher's OTA app1 slot.

Direct app1 flash over USB download mode:

```bash
pio pkg exec -- esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 \
  --baud 921600 write_flash 0x170000 .pio/build/cardputer-adv/firmware.bin
```

SD/Launcher workflow:

```bash
COPYFILE_DISABLE=1 cp .pio/build/cardputer-adv/firmware.bin \
  /Volumes/CARDPUTER/downloads/cardputer-atari800.bin
sync
```

Then on the Cardputer, select the bin from Launcher's SD browser.

## Serial monitor

`pio device monitor` is unreliable with the Cardputer USB-CDC re-enumeration on
macOS. Use the active `/dev/cu.usbmodem*` port directly:

```bash
stty -f /dev/cu.usbmodem101 115200 raw -clocal -echo
cat /dev/cu.usbmodem101
```

## Host-side tests

Pure-C logic is covered by a CMake test harness:

```bash
cmake -B build -S test
cmake --build build
ctest --test-dir build --output-on-failure
```

The current baseline has 17 host tests.

## License

GPLv2 (inherited from upstream `atari800`). AltirraOS / AltirraBASIC ROMs
included under their respective redistributable terms.
