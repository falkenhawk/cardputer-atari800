// projector.cpp — M2: Stretch mode implemented; others fall through to Stretch.
#include "projector.h"

namespace {
// Atari active picture is 320 pixels wide; the Screen_atari buffer is 384
// pixels wide (32-pixel border on each side). We sample from the center 320.
constexpr int ATARI_W              = 320;
constexpr int ATARI_CENTER_OFFSET  = 32;   // (384 - 320) / 2
constexpr int LCD_W                = 240;

static inline int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline int stretch_src_x_unchecked(int out_x) {
  // Equivalent to (out_x * 320 + 120) / 240, reduced to avoid division.
  return (out_x * 4 + 1) / 3;
}

static inline int pillarbox_src_x_unchecked(int inner_x) {
  constexpr int INNER_W = 225;
  return (inner_x * ATARI_W + INNER_W / 2) / INNER_W;
}
}

extern "C" {

int projector_stretch_src_x(int out_x) {
  out_x = clamp_int(out_x, 0, LCD_W - 1);
  return stretch_src_x_unchecked(out_x);
}

int projector_pillarbox_src_x(int inner_x) {
  constexpr int INNER_W = 225;
  inner_x = clamp_int(inner_x, 0, INNER_W - 1);
  int src = pillarbox_src_x_unchecked(inner_x);
  if (src >= ATARI_W) src = ATARI_W - 1;
  return src;
}

void projector_stretch_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Non-integer scale 240/320 = 0.75. Nearest-neighbor. Fixed-point math.
  //   src = (out_x * 320 + LCD_W/2) / LCD_W
  for (int x = 0; x < LCD_W; x++) {
    out_line[x] = palette[atari_line[ATARI_CENTER_OFFSET + stretch_src_x_unchecked(x)]];
  }
}

void projector_pixel_perfect_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Center crop: source column = ATARI_CENTER_OFFSET + ((ATARI_W - LCD_W) / 2) + x
  //             = 32 + 40 + x   (for 384-wide buffer, 320 active, 240 LCD)
  constexpr int CROP_OFFSET = (ATARI_W - LCD_W) / 2; // 40
  for (int x = 0; x < LCD_W; x++) {
    out_line[x] = palette[atari_line[ATARI_CENTER_OFFSET + CROP_OFFSET + x]];
  }
}

void projector_pillarbox_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Aspect-preserving horizontal scale 320 → 225 (×0.703125), 7-8 px black bars each side.
  constexpr int INNER_W = 225;                    // 320 * 0.703125
  constexpr int BAR_W   = (LCD_W - INNER_W) / 2; // 7
  // Paint bars
  for (int x = 0; x < BAR_W; x++)             out_line[x] = 0x0000;
  for (int x = LCD_W - BAR_W; x < LCD_W; x++) out_line[x] = 0x0000;
  // Inner area
  for (int x = 0; x < INNER_W; x++) {
    int src = pillarbox_src_x_unchecked(x);
    if (src >= ATARI_W) src = ATARI_W - 1;
    out_line[BAR_W + x] = palette[atari_line[ATARI_CENTER_OFFSET + src]];
  }
  // Handle leftover pixel if (LCD_W - 2*BAR_W) > INNER_W by 1
  if (2 * BAR_W + INNER_W < LCD_W) out_line[2 * BAR_W + INNER_W] = 0x0000;
}

void projector_cover_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Horizontal is same as Stretch (×0.75, fills LCD). Vertical crop is renderer's job (T16).
  projector_stretch_line(atari_line, out_line, palette);
}

} // extern "C"
