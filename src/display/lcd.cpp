#include "lcd.h"
#include <M5Cardputer.h>

namespace lcd {

void init() {
  auto& d = M5Cardputer.Display;
  d.setRotation(1);
  d.fillScreen(0x0000); // black
}

void push_line(int y, const uint16_t* rgb565, int width) {
  M5Cardputer.Display.pushImage(0, y, width, 1, rgb565);
}

void fill_rect(int x, int y, int w, int h, uint16_t color) {
  M5Cardputer.Display.fillRect(x, y, w, h, color);
}

}
