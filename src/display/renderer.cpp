#include "renderer.h"
#include "projector.h"
#include "palette.h"
#include "lcd.h"
#include "screenshot.h"
#include "dirty_lines.h"
#include <esp_heap_caps.h>

namespace renderer {

namespace {
Mode current_mode = Mode::Stretch;
bool use_ntsc = false;

constexpr int ATARI_SRC_H     = 240;
constexpr int LCD_H           = 135;
constexpr int ATARI_STRIDE    = 384;
constexpr int ATARI_W         = 320;
constexpr int ATARI_CENTER_X  = 32;
constexpr int LCD_W           = 240;
constexpr int STRIP_H         = 8;
constexpr int STRIP_PIXELS    = LCD_W * STRIP_H;
uint16_t* strip_buf = nullptr;
dirty_lines_t dirty_lines = {};
int last_dirty_row_count = 0;
int last_dirty_run_count = 0;

bool ensure_strip_buf() {
  if (strip_buf) return true;
  strip_buf = static_cast<uint16_t*>(
      heap_caps_malloc(STRIP_PIXELS * sizeof(uint16_t),
                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  return strip_buf != nullptr;
}

int source_y_for_lcd_y(int y) {
  // Active picture area in Screen_atari (384x240 buffer).
  // Atari's visible active picture is approximately rows 24..215 (192 rows).
  constexpr int ATARI_ACTIVE_TOP = 24;
  constexpr int ATARI_ACTIVE_H   = 192;

  int src_y;
  switch (current_mode) {
    case Mode::PixelPerfect: {
      // 135 of 192 active rows, vertically centered
      constexpr int VCROP = (ATARI_ACTIVE_H - LCD_H) / 2; // 28
      src_y = ATARI_ACTIVE_TOP + VCROP + y;
      break;
    }
    case Mode::Pillarbox: {
      // aspect-preserving vertical: 192 -> 135 rows (x0.703)
      src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
      break;
    }
    case Mode::Cover: {
      // scale active 192 -> 144 rows (x0.75), then crop center 135 out of 144
      constexpr int COVER_H  = 144;
      constexpr int VMARGIN  = (COVER_H - LCD_H) / 2; // 4 or 5
      src_y = ATARI_ACTIVE_TOP + ((y + VMARGIN) * ATARI_ACTIVE_H + COVER_H / 2) / COVER_H;
      break;
    }
    case Mode::Stretch:
    default: {
      // scale 192 -> 135 (x0.703), same vertical math as Pillarbox
      src_y = ATARI_ACTIVE_TOP + (y * ATARI_ACTIVE_H + LCD_H / 2) / LCD_H;
      break;
    }
  }

  if (src_y < 0) src_y = 0;
  if (src_y >= ATARI_SRC_H) src_y = ATARI_SRC_H - 1;
  return src_y;
}

void project_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* pal) {
  switch (current_mode) {
    case Mode::PixelPerfect: projector_pixel_perfect_line(atari_line, out_line, pal); break;
    case Mode::Pillarbox:    projector_pillarbox_line    (atari_line, out_line, pal); break;
    case Mode::Cover:        projector_cover_line        (atari_line, out_line, pal); break;
    case Mode::Stretch:      projector_stretch_line      (atari_line, out_line, pal); break;
  }
}

}

void invalidate() { dirty_lines_reset(&dirty_lines); }
int last_dirty_rows() { return last_dirty_row_count; }
int last_dirty_runs() { return last_dirty_run_count; }

void set_mode(Mode m) {
  if (current_mode != m) {
    current_mode = m;
    invalidate();
  }
}
Mode get_mode() { return current_mode; }
void set_region_ntsc(bool ntsc) {
  if (use_ntsc != ntsc) {
    use_ntsc = ntsc;
    invalidate();
  }
}

void present(const uint8_t* screen_atari) {
  const uint16_t* pal = use_ntsc ? palette_get_ntsc() : palette_get_pal();
  last_dirty_row_count = 0;
  last_dirty_run_count = 0;

  /* Screenshot capture: if armed, begin() opens the file + writes BMP
     header. write_line() receives each projected row even when the LCD row
     is skipped because it is unchanged. */
  bool capturing = screenshot::is_armed() && screenshot::begin(LCD_W, LCD_H);

  if (!ensure_strip_buf()) {
    uint16_t line_buf[LCD_W];
    for (int y = 0; y < LCD_H; y++) {
      const int src_y = source_y_for_lcd_y(y);
      const uint8_t* atari_line = &screen_atari[src_y * ATARI_STRIDE];
      const uint32_t hash = dirty_lines_hash_bytes(&atari_line[ATARI_CENTER_X], ATARI_W);
      const bool dirty = dirty_lines_check_and_store(&dirty_lines, y, hash) != 0;
      if (dirty || capturing) {
        project_line(atari_line, line_buf, pal);
      }
      if (dirty) {
        last_dirty_row_count++;
        last_dirty_run_count++;
        lcd::push_line(y, line_buf, LCD_W);
      }
      if (capturing) screenshot::write_line(line_buf, LCD_W);
    }
    dirty_lines_end_frame(&dirty_lines);
    if (capturing) screenshot::end();
    return;
  }

  for (int base_y = 0; base_y < LCD_H; base_y += STRIP_H) {
    int rows = LCD_H - base_y;
    if (rows > STRIP_H) rows = STRIP_H;
    bool dirty_row[STRIP_H] = {false};

    for (int row = 0; row < rows; row++) {
      const int y = base_y + row;
      const int src_y = source_y_for_lcd_y(y);
      uint16_t* out_line = &strip_buf[row * LCD_W];
      const uint8_t* atari_line = &screen_atari[src_y * ATARI_STRIDE];
      const uint32_t hash = dirty_lines_hash_bytes(&atari_line[ATARI_CENTER_X], ATARI_W);
      dirty_row[row] = dirty_lines_check_and_store(&dirty_lines, y, hash) != 0;
      if (dirty_row[row] || capturing) {
        project_line(atari_line, out_line, pal);
      }
      if (dirty_row[row]) last_dirty_row_count++;
      if (capturing) screenshot::write_line(out_line, LCD_W);
    }

    int run_start = -1;
    for (int row = 0; row < rows; row++) {
      if (dirty_row[row]) {
        if (run_start < 0) run_start = row;
      } else if (run_start >= 0) {
        last_dirty_run_count++;
        lcd::push_rect(0, base_y + run_start, LCD_W, row - run_start,
                       &strip_buf[run_start * LCD_W]);
        run_start = -1;
      }
    }
    if (run_start >= 0) {
      last_dirty_run_count++;
      lcd::push_rect(0, base_y + run_start, LCD_W, rows - run_start,
                     &strip_buf[run_start * LCD_W]);
    }
  }

  dirty_lines_end_frame(&dirty_lines);
  if (capturing) screenshot::end();
}

} // namespace renderer
