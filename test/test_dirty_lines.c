#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/display/dirty_lines.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  dirty_lines_t dirty = {0};
  uint16_t line_a[4] = {0x0000, 0x1111, 0x2222, 0x3333};
  uint16_t line_b[4] = {0x0000, 0x1111, 0x2222, 0x3334};

  uint32_t hash_a = dirty_lines_hash_rgb565(line_a, 4);
  uint32_t hash_b = dirty_lines_hash_rgb565(line_b, 4);
  CHECK(hash_a != hash_b, "hash changes when a pixel changes");

  uint8_t bytes_a[4] = {1, 2, 3, 4};
  uint8_t bytes_b[4] = {1, 2, 3, 5};
  CHECK(dirty_lines_hash_bytes(bytes_a, 4) != dirty_lines_hash_bytes(bytes_b, 4),
        "byte hash changes when a source byte changes");
  uint8_t bytes_tail_a[5] = {1, 2, 3, 4, 5};
  uint8_t bytes_tail_b[5] = {1, 2, 3, 4, 6};
  CHECK(dirty_lines_hash_bytes(bytes_tail_a, 5) != dirty_lines_hash_bytes(bytes_tail_b, 5),
        "byte hash includes tail bytes");

  dirty_lines_reset(&dirty);
  CHECK(dirty_lines_check_and_store(&dirty, 7, hash_a) == 1, "first frame marks line dirty");
  dirty_lines_end_frame(&dirty);
  CHECK(dirty_lines_check_and_store(&dirty, 7, hash_a) == 0, "same line hash stays clean");
  CHECK(dirty_lines_check_and_store(&dirty, 7, hash_b) == 1, "changed line hash is dirty");

  dirty_lines_end_frame(&dirty);
  dirty_lines_reset(&dirty);
  CHECK(dirty_lines_check_and_store(&dirty, 7, hash_b) == 1, "reset forces repaint");

  CHECK(dirty_lines_check_and_store(&dirty, -1, hash_b) == 0, "negative line is ignored");
  CHECK(dirty_lines_check_and_store(&dirty, DIRTY_LINES_MAX, hash_b) == 0, "out-of-range line is ignored");

  if (fail) return EXIT_FAILURE;
  puts("PASS: dirty_lines");
  return EXIT_SUCCESS;
}
