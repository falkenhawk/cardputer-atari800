# Host-side tests

Pure-C code in the cardputer-atari800 codebase that has no hardware
dependencies is tested here on the host machine. This is much faster than
flashing to the device and catches logic bugs before they ever touch hardware.

## Running

```bash
cmake -B build -S test
cmake --build build
ctest --test-dir build --output-on-failure
```

## Adding tests

For each new pure-C module under `src/` that has a testable interface:

1. Write the test as `test/test_<module>.c`
2. Add `add_executable(test_<module> test_<module>.c <sources>)` to CMakeLists.txt
3. Add `add_test(NAME <module> COMMAND test_<module>)`
4. Run locally first — iteration is seconds, not minutes

Tests in this directory run on macOS (or any POSIX host). They never run on
the ESP32; they test the pure logic that happens to also run on the ESP32.
