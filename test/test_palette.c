/* test_palette.c — verify the Atari palette LUT generation. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Forward declarations matching palette.h interface */
extern const uint16_t* palette_get_pal(void);
extern const uint16_t* palette_get_ntsc(void);

static int fail = 0;

#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  const uint16_t* pal  = palette_get_pal();
  const uint16_t* ntsc = palette_get_ntsc();

  CHECK(pal  != NULL, "palette_get_pal returned NULL");
  CHECK(ntsc != NULL, "palette_get_ntsc returned NULL");

  /* Color index 0 (luma=0) is always black on Atari */
  CHECK(pal[0] == 0x0000,  "pal[0] should be black");
  CHECK(ntsc[0] == 0x0000, "ntsc[0] should be black");

  /* Index 0x0E (hue=0, luma=E = near-max) is near-white on Atari */
  CHECK((pal[0x0E]  & 0xF800) > 0xD000, "pal[0x0E] R channel should be bright");
  CHECK((ntsc[0x0E] & 0xF800) > 0xD000, "ntsc[0x0E] R channel should be bright");

  /* PAL and NTSC differ — tables must not be identical */
  int any_diff = 0;
  for (int i = 0; i < 256; i++) {
    if (pal[i] != ntsc[i]) { any_diff = 1; break; }
  }
  CHECK(any_diff, "PAL and NTSC tables should differ");

  if (fail) return EXIT_FAILURE;
  printf("PASS: palette\n");
  return EXIT_SUCCESS;
}
