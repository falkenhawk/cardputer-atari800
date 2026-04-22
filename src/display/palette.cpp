// palette.cpp — Generate Atari 256-color palette LUTs in RGB565.
// The Atari palette is 16 hues × 16 luminance levels. NTSC uses YIQ math
// that ports cleanly from atari800 upstream. PAL upstream uses a del_coeffs
// per-line chroma averaging model that is physically accurate on a CRT
// (the eye integrates successive lines) but produces very low effective
// saturation on a 240x135 LCD with no chroma subcarrier — muddy green-cyan
// instead of a clear blue for AltirraBASIC's background. So we use the same
// YIQ math for PAL with a PAL-shifted starting angle; the result matches a
// good PAL composite output on a digital display without the extra cost.
//
// This is a minimal port of atari800 5.2.0 upstream:
//   src/colours_ntsc.c      (UpdateYIQTableFromGenerated + YIQ2RGB)
//   src/colours.c           (Gamma2Linear, Linear2sRGB, STANDARD preset)
//
// Parameters are hardcoded to the STANDARD preset (hue=0, sat=0, contrast=0,
// brightness=0, gamma=2.35, black_level=16, white_level=235). The cardputer
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
constexpr double kPalColorDelay  = 23.2; // colours_pal.c:  default_setup (kept for PAL angle step)

// -- NTSC colorburst angle in YIQ (upstream: 303°)
constexpr double kNtscColorburstDeg = 303.0;

// -- PAL starting angle for the simplified YIQ path. Chosen so hue 9 lands
//    in the blue region of YIQ → RGB. See top-of-file comment on why we use
//    YIQ math for PAL instead of upstream's per-line del_coeffs approach.
constexpr double kPalColorburstDeg = 330.0;

// -- Saturation amplitudes. PAL broadcast nominal is 0.17, but digital
//    reproduction on a small LCD benefits from a modest boost to roughly
//    match NTSC's perceived chroma strength after gamma.
constexpr double kNtscSatAmp = 0.175;
constexpr double kPalSatAmp  = 0.22;

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
      double sat   = (kSaturation + 1.0) * kNtscSatAmp;
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
// PAL generator — simplified YIQ with a PAL-shifted starting angle.
//
// Upstream atari800 uses a del_coeffs per-line chroma-averaging model that is
// faithful to how a real PAL decoder reconstructs color across successive
// lines. On a CRT the eye integrates; the resulting saturation looks right.
// On our 240x135 LCD with no chroma subcarrier, the averaged UV collapses
// into very dim chroma — AltirraBASIC's hue-9-luma-4 background rendered as
// muddy green-cyan. So we reuse the NTSC YIQ math with a PAL-shifted
// starting angle and a slightly boosted saturation amplitude.
// ---------------------------------------------------------------------------
void compute_pal(uint16_t out[256]) {
  const double start_angle = (kPalColorburstDeg * M_PI / 180.0) + kHue * M_PI;
  const double color_diff  = kPalColorDelay * M_PI / 180.0;

  for (int cr = 0; cr < 16; cr++) {
    double i_chroma = 0.0, q_chroma = 0.0;
    if (cr) {
      double angle = start_angle + (cr - 1) * color_diff;
      double sat   = (kSaturation + 1.0) * kPalSatAmp;
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
