// lcd.h — minimal LCD wrapper for line-buffer blitting.
#pragma once
#include <stdint.h>

namespace lcd {

void init();                                              // clear screen, set rotation
void push_line(int y, const uint16_t* rgb565, int width); // blit one horizontal line
void push_rect(int x, int y, int w, int h, const uint16_t* rgb565);
void fill_rect(int x, int y, int w, int h, uint16_t color);

}
