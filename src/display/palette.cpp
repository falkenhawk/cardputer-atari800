// palette.cpp — Generate Atari 256-color palette LUTs in RGB565.
// The Atari palette is 16 hues × 16 luminance levels. PAL and NTSC use
// different chroma generation (YIQ vs YUV w/ even/odd-line averaging).
//
// This is a minimal port of atari800 5.2.0 upstream generators:
//   src/colours_ntsc.c      (UpdateYIQTableFromGenerated + YIQ2RGB)
//   src/colours_pal.c       (GetYUVFromGenerated + YUV2RGB w/ del_coeffs)
//   src/colours.c           (Gamma2Linear, Linear2sRGB, YUV2RGB_matrix,
//                            STANDARD preset defaults)
//
// Parameters are hardcoded to the STANDARD preset (hue=0, sat=0, contrast=0,
// brightness=0, gamma=2.35, black_level=16, white_level=235) plus the
// per-system default color_delay (NTSC 26.8°, PAL 23.2°). The cardputer
// LCD has no menu for these yet; if/when M4 exposes them, lift these into
// a runtime Colours_setup_t.

#include "palette.h"
#include <math.h>

namespace {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -- Upstream STANDARD preset (colours.c: presets[COLOURS_PRESET_STANDARD])
constexpr double kHue        = 0.0;
constexpr double kSaturation = 0.0;
constexpr double kContrast   = 0.0;
constexpr double kBrightness = 0.0;
constexpr double kGamma      = 2.35;
constexpr int    kBlackLevel = 16;
constexpr int    kWhiteLevel = 235;

// -- Per-system default color_delay (degrees)
constexpr double kNtscColorDelay = 26.8; // colours_ntsc.c: default_setup
constexpr double kPalColorDelay  = 23.2; // colours_pal.c:  default_setup

// -- NTSC colorburst angle in YIQ (upstream: 303°)
constexpr double kNtscColorburstDeg = 303.0;

// -- NTSC luma multipliers from CGIA.PDF (shared between NTSC & PAL)
static constexpr double kLumaMult[16] = {
  0.6941, 0.7091, 0.7241, 0.7401,
  0.7560, 0.7741, 0.7931, 0.8121,
  0.8260, 0.8470, 0.8700, 0.8930,
  0.9160, 0.9420, 0.9690, 1.0000
};

// Matches upstream Colours_Gamma2Linear: pow(c, gamma) with negative-c branch
double gamma_to_linear(double c, double gamma_adj) {
  if (c >= 0.0) return pow(c, gamma_adj);
  return c / 12.92;
}

// Standard sRGB OETF (Colours_Linear2sRGB in colours.c)
double linear_to_srgb(double c) {
  if (c <= 0.0031308) return c * 12.92;
  return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

uint8_t clamp_u8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

uint16_t rgb888_to_565(int r, int g, int b) {
  uint8_t rr = clamp_u8(r);
  uint8_t gg = clamp_u8(g);
  uint8_t bb = clamp_u8(b);
  return ((uint16_t)(rr & 0xF8) << 8) | ((uint16_t)(gg & 0xFC) << 3) | ((uint16_t)(bb & 0xF8) >> 3);
}

// Compute luma for a given lm (0..15) matching upstream's formula
double compute_y(int lm) {
  double y = (kLumaMult[lm] - kLumaMult[0]) / (kLumaMult[15] - kLumaMult[0]);
  y *= kContrast * 0.5 + 1.0;
  y += kBrightness * 0.5;
  const double black = (double)kBlackLevel / 255.0;
  const double white = (double)kWhiteLevel / 255.0;
  return y * (white - black) + black;
}

// Final gamma+sRGB+clamp+encode step shared between NTSC and PAL paths
uint16_t finalize_rgb(double r, double g, double b) {
  r = linear_to_srgb(gamma_to_linear(r, kGamma));
  g = linear_to_srgb(gamma_to_linear(g, kGamma));
  b = linear_to_srgb(gamma_to_linear(b, kGamma));
  return rgb888_to_565((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

// ---------------------------------------------------------------------------
// NTSC generator — port of UpdateYIQTableFromGenerated + YIQ2RGB
// ---------------------------------------------------------------------------
void compute_ntsc(uint16_t out[256]) {
  const double start_angle = (kNtscColorburstDeg * M_PI / 180.0) + kHue * M_PI;
  const double color_diff  = kNtscColorDelay * M_PI / 180.0;

  for (int cr = 0; cr < 16; cr++) {
    double i_chroma = 0.0, q_chroma = 0.0;
    if (cr) {
      double angle = start_angle + (cr - 1) * color_diff;
      double sat   = (kSaturation + 1.0) * 0.175;
      i_chroma = cos(angle) * sat;
      q_chroma = sin(angle) * sat;
    }
    for (int lm = 0; lm < 16; lm++) {
      double y = compute_y(lm);
      // YIQ -> RGB (BT.601 coefficients, from upstream YIQ2RGB)
      double r = y + 0.9563 * i_chroma + 0.6210 * q_chroma;
      double g = y - 0.2721 * i_chroma - 0.6474 * q_chroma;
      double b = y - 1.1070 * i_chroma + 1.7046 * q_chroma;
      out[(cr << 4) | lm] = finalize_rgb(r, g, b);
    }
  }
}

// ---------------------------------------------------------------------------
// PAL generator — port of GetYUVFromGenerated + YUV2RGB
//
// Faithful reproduction including the del_coeffs[] even/odd per-hue delay
// table. The resulting UV is averaged across even & odd lines (upstream
// does the same; true per-line differences aren't visible in our single
// 256-entry LUT).
// ---------------------------------------------------------------------------
struct DelCoeff { int add; int mult; };
static constexpr DelCoeff kEvenDel[15] = {
  { 1, 5 }, { 1, 6 }, { 1, 7 },
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 },
  { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }
};
static constexpr DelCoeff kOddDel[15] = {
  { 1, 1 }, { 1, 0 },
  { 0, 7 }, { 0, 6 }, { 0, 5 }, { 0, 4 }, { 0, 2 }, { 0, 1 }, { 0, 0 },
  { 1, 7 }, { 1, 5 }, { 1, 4 }, { 1, 3 }, { 1, 2 }, { 1, 1 }
};

void compute_pal(uint16_t out[256]) {
  // Constants from colours_pal.c (verbatim)
  constexpr double kColorDisableThreshold = 0.05;
  constexpr double kBaseDel = 0.421894970414201; // ~95.2ns / (1/4.43MHz)
  constexpr double kAddDel  = 0.446563064859117; // ~100.7ns
  const double del_adj = kPalColorDelay / 360.0;

  const double even_burst_del = kBaseDel + kAddDel * kEvenDel[0].add + del_adj * kEvenDel[0].mult;
  const double odd_burst_del  = kBaseDel + kAddDel * kOddDel[0].add  + del_adj * kOddDel[0].mult;

  double burst_diff = even_burst_del - odd_burst_del;
  burst_diff -= floor(burst_diff); // normalize to 0..1

  double saturation_mult;
  if (burst_diff > 0.5 - kColorDisableThreshold && burst_diff < 0.5 + kColorDisableThreshold) {
    saturation_mult = 0.0;
  } else {
    double amp = sqrt(2.0 * cos(burst_diff * 2.0 * M_PI) + 2.0);
    saturation_mult = sqrt(2.0) / amp;
  }

  const double subcarrier_del = (even_burst_del + odd_burst_del + kHue) / 2.0;

  // YUV-to-RGB matrix (from colours.c: YUV2RGB_matrix)
  // R = Y + 1.13983 * V
  // G = Y - 0.39465 * U - 0.58060 * V
  // B = Y + 2.03211 * U

  for (int cr = 0; cr < 16; cr++) {
    double even_u = 0.0, odd_u = 0.0, even_v = 0.0, odd_v = 0.0;
    if (cr) {
      const DelCoeff& ev = kEvenDel[cr - 1];
      const DelCoeff& od = kOddDel [cr - 1];
      double even_del   = kBaseDel + kAddDel * ev.add + del_adj * ev.mult;
      double odd_del    = kBaseDel + kAddDel * od.add + del_adj * od.mult;
      double even_angle = (0.5 - (even_del - subcarrier_del)) * 2.0 * M_PI;
      double odd_angle  = (0.5 + (odd_del  - subcarrier_del)) * 2.0 * M_PI;
      double sat        = (kSaturation + 1.0) * 0.175 * saturation_mult;
      even_u = cos(even_angle) * sat;
      even_v = sin(even_angle) * sat;
      odd_u  = cos(odd_angle)  * sat;
      odd_v  = sin(odd_angle)  * sat;
    }
    // Average even & odd UV (upstream does this in YUV2RGB)
    double u = (even_u + odd_u) * 0.5;
    double v = (even_v + odd_v) * 0.5;

    for (int lm = 0; lm < 16; lm++) {
      double y = compute_y(lm);
      double r = y + 1.13983 * v;
      double g = y - 0.39465 * u - 0.58060 * v;
      double b = y + 2.03211 * u;
      out[(cr << 4) | lm] = finalize_rgb(r, g, b);
    }
  }
}

uint16_t pal_lut[256];
uint16_t ntsc_lut[256];
bool pal_computed  = false;
bool ntsc_computed = false;

} // anonymous namespace

extern "C" const uint16_t* palette_get_pal(void) {
  if (!pal_computed) {
    compute_pal(pal_lut);
    pal_computed = true;
  }
  return pal_lut;
}

extern "C" const uint16_t* palette_get_ntsc(void) {
  if (!ntsc_computed) {
    compute_ntsc(ntsc_lut);
    ntsc_computed = true;
  }
  return ntsc_lut;
}
