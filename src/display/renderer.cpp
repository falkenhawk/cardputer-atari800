#include "renderer.h"
#include "projector.h"
#include "palette.h"
#include "lcd.h"

namespace renderer {

namespace {
Mode current_mode = Mode::Stretch;
bool use_ntsc = false;

constexpr int ATARI_SRC_H     = 240;
constexpr int LCD_H           = 135;
constexpr int ATARI_STRIDE    = 384;
// Vertical offset into Screen_atari — approximately centered on the active
// picture area. T16 will replace this with per-mode offsets.
constexpr int ATARI_VERT_OFF  = (ATARI_SRC_H - LCD_H) / 2 + 24;
constexpr int LCD_W           = 240;
}

void set_mode(Mode m) { current_mode = m; }
void set_region_ntsc(bool ntsc) { use_ntsc = ntsc; }

void present(const uint8_t* screen_atari) {
  const uint16_t* pal = use_ntsc ? palette_get_ntsc() : palette_get_pal();
  uint16_t line_buf[LCD_W];

  for (int y = 0; y < LCD_H; y++) {
    int src_y = ATARI_VERT_OFF + y;
    if (src_y < 0) src_y = 0;
    if (src_y >= ATARI_SRC_H) src_y = ATARI_SRC_H - 1;
    const uint8_t* atari_line = &screen_atari[src_y * ATARI_STRIDE];

    switch (current_mode) {
      case Mode::PixelPerfect: projector_pixel_perfect_line(atari_line, line_buf, pal); break;
      case Mode::Pillarbox:    projector_pillarbox_line    (atari_line, line_buf, pal); break;
      case Mode::Cover:        projector_cover_line        (atari_line, line_buf, pal); break;
      case Mode::Stretch:      projector_stretch_line      (atari_line, line_buf, pal); break;
    }

    lcd::push_line(y, line_buf, LCD_W);
  }
}

} // namespace renderer
