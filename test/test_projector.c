/* test_projector.c — Stretch mode sanity. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern void projector_stretch_line(const uint8_t* atari_line,
                                   uint16_t*       out_line,
                                   const uint16_t* palette);

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Build a 384-wide Atari line: all pixels = palette index 0x10 */
  uint8_t atari_line[384];
  for (int i = 0; i < 384; i++) atari_line[i] = 0x10;

  /* Palette where 0x10 maps to 0xABCD, all other entries to 0x0000. */
  uint16_t palette[256] = {0};
  palette[0x10] = 0xABCD;

  uint16_t out_line[240] = {0};
  projector_stretch_line(atari_line, out_line, palette);

  /* All 240 output pixels should be 0xABCD */
  for (int i = 0; i < 240; i++) {
    CHECK(out_line[i] == 0xABCD, "out_line should be uniform 0xABCD");
    if (out_line[i] != 0xABCD) break;
  }

  /* Alternating pattern: even columns → 0x10, odd → 0x20 */
  for (int i = 0; i < 384; i++) atari_line[i] = (i & 1) ? 0x20 : 0x10;
  palette[0x20] = 0x1234;

  projector_stretch_line(atari_line, out_line, palette);

  /* Both colors should appear in the output (proves we're not taking only even samples) */
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
