#include "renderer.h"
#include "projector.h"
#include "palette.h"
#include "lcd.h"
#include "screenshot.h"

namespace renderer {

namespace {
Mode current_mode = Mode::Stretch;
bool use_ntsc = false;

constexpr int ATARI_SRC_H     = 240;
constexpr int LCD_H           = 135;
constexpr int ATARI_STRIDE    = 384;
constexpr int LCD_W           = 240;
}

void set_mode(Mode m) { current_mode = m; }
Mode get_mode() { return current_mode; }
void set_region_ntsc(bool ntsc) { use_ntsc = ntsc; }

void present(const uint8_t* screen_atari) {
  const uint16_t* pal = use_ntsc ? palette_get_ntsc() : palette_get_pal();
  uint16_t line_buf[LCD_W];

  /* Screenshot capture: if armed, begin() opens the file + writes BMP
     header. write_line() called after each lcd::push_line to persist
     exactly the bytes we sent. end() closes + flushes after the last row. */
  bool capturing = screenshot::is_armed() && screenshot::begin(LCD_W, LCD_H);

  // Active picture area in Screen_atari (384×240 buffer).
  // Atari's visible active picture is approximately rows 24..215 (192 rows).
  constexpr int ATARI_ACTIVE_TOP = 24;
  constexpr int ATARI_ACTIVE_H   = 192;

  for (int y = 0; y < LCD_H; y++) {
    int src_y;
    switch (current_mode) {
      case Mode::PixelPerfect: {
        // 135 of 192 active rows, vertically centered
        constexpr int VCROP = (ATARI_ACTIVE_H - LCD_H) / 2; // 28
        src_y = ATARI_ACTIVE_TOP + VCROP + y;
        break;
      }
      case Mode::Pillarbox: {
        // aspect-preserving vertical: 192 → 135 rows (×0.703)
        src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
        break;
      }
      case Mode::Cover: {
        // scale active 192 → 144 rows (×0.75), then crop center 135 out of 144
        constexpr int COVER_H  = 144;
        constexpr int VMARGIN  = (COVER_H - LCD_H) / 2; // 4 or 5
        src_y = ATARI_ACTIVE_TOP + ((y + VMARGIN) * ATARI_ACTIVE_H + COVER_H / 2) / COVER_H;
        break;
      }
      case Mode::Stretch:
      default: {
        // scale 192 → 135 (×0.703) — same vertical math as Pillarbox
        src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
        break;
      }
    }

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
    if (capturing) screenshot::write_line(line_buf, LCD_W);
  }

  if (capturing) screenshot::end();
}

} // namespace renderer
