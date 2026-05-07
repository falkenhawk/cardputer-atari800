/* test_projector.c — Stretch mode sanity. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern void projector_stretch_line(const uint8_t* atari_line,
                                   uint16_t*       out_line,
                                   const uint16_t* palette);
extern void projector_pixel_perfect_line(const uint8_t* atari_line,
                                         uint16_t*       out_line,
                                         const uint16_t* palette);
extern void projector_pillarbox_line(const uint8_t* atari_line,
                                     uint16_t*       out_line,
                                     const uint16_t* palette);
extern void projector_cover_line(const uint8_t* atari_line,
                                 uint16_t*       out_line,
                                 const uint16_t* palette);
extern int projector_stretch_src_x(int out_x);
extern int projector_pillarbox_src_x(int inner_x);

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

  CHECK(projector_stretch_src_x(0) == 0, "stretch x-map first pixel");
  CHECK(projector_stretch_src_x(1) == 1, "stretch x-map keeps rounded nearest source");
  CHECK(projector_stretch_src_x(2) == 3, "stretch x-map skips every fourth source pixel");
  CHECK(projector_stretch_src_x(239) == 319, "stretch x-map last pixel");

  /* --- Pixel-perfect (1:1 crop): output maps to center 240 of source. */
  for (int i = 0; i < 384; i++) atari_line[i] = (uint8_t)(i & 0xFF); /* gradient */
  for (int i = 0; i < 256; i++) palette[i] = (uint16_t)i;
  projector_pixel_perfect_line(atari_line, out_line, palette);
  /* Active picture is at columns 32..(32+320)=352.
     Pixel-perfect crop = center 240 of the 320 active → columns 32+40=72..(72+240)=312 in source.
     out_line[0] should equal palette[atari_line[72]] = palette[72] = 72. */
  CHECK(out_line[0] == 72,                        "1:1 out[0] samples src col 72 (active start + 40)");
  CHECK(out_line[239] == (uint16_t)((72 + 239) & 0xFF), "1:1 out[239] samples src col 311 (wraps mod 256)");

  /* --- Pillarbox: 225 inner pixels, ~7 px bars each side. */
  for (int i = 0; i < 384; i++) atari_line[i] = 0x10;
  for (int i = 0; i < 256; i++) palette[i] = 0;
  palette[0x10] = 0xBEEF;
  projector_pillarbox_line(atari_line, out_line, palette);
  /* Pixel 0 and 239 should be in the black bar (color 0). */
  CHECK(out_line[0] == 0x0000,   "pillarbox left bar is black");
  CHECK(out_line[239] == 0x0000, "pillarbox right bar is black");
  /* Pixel 120 (center) should be the source color. */
  CHECK(out_line[120] == 0xBEEF, "pillarbox center is source color");
  CHECK(projector_pillarbox_src_x(0) == 0, "pillarbox x-map first inner pixel");
  CHECK(projector_pillarbox_src_x(224) == 319, "pillarbox x-map last inner pixel");

  /* --- Cover: horizontally equivalent to Stretch for a solid color. */
  for (int i = 0; i < 384; i++) atari_line[i] = 0x10;
  projector_cover_line(atari_line, out_line, palette);
  int cover_all_beef = 1;
  for (int i = 0; i < 240; i++) if (out_line[i] != 0xBEEF) { cover_all_beef = 0; break; }
  CHECK(cover_all_beef, "cover: solid source color fills output");

  if (fail) return EXIT_FAILURE;
  printf("PASS: projector stretch\n");
  return EXIT_SUCCESS;
}
