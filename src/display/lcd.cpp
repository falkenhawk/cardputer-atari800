#include "lcd.h"
#include <M5Cardputer.h>

namespace lcd {

void init() {
  auto& d = M5Cardputer.Display;
  d.setRotation(1);
  d.fillScreen(0x0000); // black
  /* LGFX pushImage writes the raw uint16_t bytes little-endian; ST7789 reads
     them big-endian. For scalar setTextColor()/fillRect() (those take a single
     uint32_t) LGFX handles the byte-order internally, but pushImage ships the
     array verbatim. Without this flag, our palette's 0x0993 (blue) shows as
     0x9309 (warm brown) — confirmed at v0.3-m3-t7c via serial palette dump. */
  d.setSwapBytes(true);
}

void push_line(int y, const uint16_t* rgb565, int width) {
  M5Cardputer.Display.pushImage(0, y, width, 1, rgb565);
}

void fill_rect(int x, int y, int w, int h, uint16_t color) {
  M5Cardputer.Display.fillRect(x, y, w, h, color);
}

}
