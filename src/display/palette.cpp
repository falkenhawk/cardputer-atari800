// palette.cpp — Generate Atari 256-color palette LUTs in RGB565.
// The Atari palette is 16 hues × 16 luminance levels. PAL and NTSC use
// different hue phases; we compute both.
//
// Reference: atari800 upstream colours_pal.c / colours_ntsc.c. This is
// a simplified YUV → RGB → RGB565 derivation.

#include "palette.h"
#include <math.h>

namespace {

uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | ((uint16_t)(b & 0xF8) >> 3);
}

uint8_t clamp_u8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

double hue_angle(int hue_idx, bool is_pal) {
  const double base = is_pal ? 14.0 : 27.0;
  const double step = 360.0 / 15.0;
  if (hue_idx == 0) return 0.0;
  return base + step * (hue_idx - 1);
}

void compute_palette(uint16_t out[256], bool is_pal) {
  for (int i = 0; i < 256; i++) {
    int hue_idx  = (i >> 4) & 0x0F;
    int luma_idx =  i       & 0x0F;

    double y = luma_idx / 15.0;

    if (hue_idx == 0) {
      uint8_t g = clamp_u8((int)(y * 255.0));
      out[i] = rgb888_to_565(g, g, g);
      continue;
    }

    double angle = hue_angle(hue_idx, is_pal) * M_PI / 180.0;
    double chroma = is_pal ? 0.50 : 0.55;

    double u = chroma * cos(angle);
    double v = chroma * sin(angle);

    double r = y + 1.140 * v;
    double g = y - 0.395 * u - 0.581 * v;
    double b = y + 2.032 * u;

    out[i] = rgb888_to_565(
      clamp_u8((int)(r * 255.0)),
      clamp_u8((int)(g * 255.0)),
      clamp_u8((int)(b * 255.0))
    );
  }
}

uint16_t pal_lut[256];
uint16_t ntsc_lut[256];
bool pal_computed  = false;
bool ntsc_computed = false;

} // anonymous namespace

extern "C" const uint16_t* palette_get_pal(void) {
  if (!pal_computed) {
    compute_palette(pal_lut, true);
    pal_computed = true;
  }
  return pal_lut;
}

extern "C" const uint16_t* palette_get_ntsc(void) {
  if (!ntsc_computed) {
    compute_palette(ntsc_lut, false);
    ntsc_computed = true;
  }
  return ntsc_lut;
}
