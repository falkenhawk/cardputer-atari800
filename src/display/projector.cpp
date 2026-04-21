// projector.cpp — M2: Stretch mode implemented; others fall through to Stretch.
#include "projector.h"

namespace {
// Atari active picture is 320 pixels wide; the Screen_atari buffer is 384
// pixels wide (32-pixel border on each side). We sample from the center 320.
constexpr int ATARI_W              = 320;
constexpr int ATARI_CENTER_OFFSET  = 32;   // (384 - 320) / 2
constexpr int LCD_W                = 240;
}

extern "C" {

void projector_stretch_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette) {
  // Non-integer scale 240/320 = 0.75. Nearest-neighbor. Fixed-point math.
  //   src = (out_x * 320 + LCD_W/2) / LCD_W
  for (int x = 0; x < LCD_W; x++) {
    int src = (x * ATARI_W + LCD_W / 2) / LCD_W;
    if (src >= ATARI_W) src = ATARI_W - 1;
    out_line[x] = palette[atari_line[ATARI_CENTER_OFFSET + src]];
  }
}

// Stubs — T15 replaces these with real implementations.
void projector_pixel_perfect_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}
void projector_pillarbox_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}
void projector_cover_line(const uint8_t* a, uint16_t* o, const uint16_t* p) {
  projector_stretch_line(a, o, p);
}

} // extern "C"
