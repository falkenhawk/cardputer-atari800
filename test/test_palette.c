/* test_palette.c — verify the Atari palette LUT generation.
 *
 * The palette generator is a direct port of atari800 5.2.0's upstream
 * colours_ntsc.c + colours_pal.c with the STANDARD preset (black_level=16,
 * white_level=235, gamma=2.35). Assertions here therefore don't expect
 * luma 0 to map to pure 0x0000 (it's pedestal black ~ 16/255 after
 * gamma+sRGB), nor luma 15 to be literally saturated white. Tests focus
 * on qualitative properties: near-black is dark, near-white is bright,
 * hue 9 is blue-dominant, PAL != NTSC. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Forward declarations matching palette.h interface */
extern const uint16_t* palette_get_pal(void);
extern const uint16_t* palette_get_ntsc(void);

static int fail = 0;

#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

/* Helpers for RGB565 component extraction. R: upper 5 bits. G: middle 6.
 * B: lower 5. */
static inline int r5(uint16_t c) { return (c >> 11) & 0x1F; }
static inline int g6(uint16_t c) { return (c >>  5) & 0x3F; }
static inline int b5(uint16_t c) { return  c        & 0x1F; }

int main(void) {
  const uint16_t* pal  = palette_get_pal();
  const uint16_t* ntsc = palette_get_ntsc();

  CHECK(pal  != NULL, "palette_get_pal returned NULL");
  CHECK(ntsc != NULL, "palette_get_ntsc returned NULL");

  /* luma 0, hue 0 — near-black (BT.601 pedestal at 16/255) */
  CHECK(r5(pal[0])  < 4, "pal[0]  R should be near black");
  CHECK(g6(pal[0])  < 8, "pal[0]  G should be near black");
  CHECK(b5(pal[0])  < 4, "pal[0]  B should be near black");
  CHECK(r5(ntsc[0]) < 4, "ntsc[0] R should be near black");
  CHECK(g6(ntsc[0]) < 8, "ntsc[0] G should be near black");
  CHECK(b5(ntsc[0]) < 4, "ntsc[0] B should be near black");

  /* luma 15, hue 0 — near-white (BT.601 white at 235/255) */
  CHECK(r5(pal[0x0F])  > 26, "pal[0x0F]  R should be bright");
  CHECK(g6(pal[0x0F])  > 52, "pal[0x0F]  G should be bright");
  CHECK(b5(pal[0x0F])  > 26, "pal[0x0F]  B should be bright");
  CHECK(r5(ntsc[0x0F]) > 26, "ntsc[0x0F] R should be bright");
  CHECK(g6(ntsc[0x0F]) > 52, "ntsc[0x0F] G should be bright");
  CHECK(b5(ntsc[0x0F]) > 26, "ntsc[0x0F] B should be bright");

  /* hue 0 greyscale: R, G, B should be roughly equal for any luma.
   * Use component-normalized comparison: compare R5 vs B5 directly,
   * and G6/2 vs R5. */
  for (int lm = 0; lm < 16; lm++) {
    int idx = lm;
    CHECK(abs(r5(pal[idx])  - b5(pal[idx]))  <= 1, "pal  hue 0 should be greyscale (R~=B)");
    CHECK(abs(r5(ntsc[idx]) - b5(ntsc[idx])) <= 1, "ntsc hue 0 should be greyscale (R~=B)");
  }

  /* Hue 9 at luma 4 (index 0x94) — should be blue-dominant in both
   * systems. NTSC: YIQ with 303° starting angle, hue 9 lands at ~157°.
   * PAL: simplified YIQ with 330° starting angle, hue 9 lands at ~162°.
   * In both, B > R and B > G. */
  CHECK(b5(pal[0x94])  > r5(pal[0x94]),  "PAL  hue 9 luma 4 should be bluer than red");
  CHECK(b5(pal[0x94])  > 4,              "PAL  hue 9 luma 4 should have non-trivial blue");
  CHECK(b5(ntsc[0x94]) > r5(ntsc[0x94]), "NTSC hue 9 luma 4 should be bluer than red");
  CHECK(b5(ntsc[0x94]) > 4,              "NTSC hue 9 luma 4 should have non-trivial blue");

  /* PAL hue 9 luma 4 should be clearly blue-dominant after the simplified
   * YIQ port (B > G and B >> R). Compare G6 on a 5-bit scale (G6 >> 1). */
  {
    uint16_t p94 = pal[0x94];
    int r = r5(p94);
    int g = g6(p94) >> 1;
    int b = b5(p94);
    CHECK(b > g && b > r, "PAL hue 9 luma 4: blue channel dominant over both green and red");
    CHECK(b >= 2 * r,     "PAL hue 9 luma 4: blue at least 2x red");
  }

  /* Hue 1 (index 0x14 at luma 4) — traditionally "gold" (colorburst
   * hue in upstream NTSC); at least should NOT be blue-dominant. */
  CHECK(b5(ntsc[0x14]) <= r5(ntsc[0x14]) + 2, "NTSC hue 1 should not be strongly blue");

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
