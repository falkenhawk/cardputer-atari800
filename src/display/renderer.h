// renderer.h — drives per-line conversion of Screen_atari → LCD via palette+projector.
#pragma once
#include <stdint.h>

namespace renderer {

enum class Mode { PixelPerfect, Pillarbox, Cover, Stretch };

void set_mode(Mode m);
Mode get_mode();
void set_region_ntsc(bool ntsc);
void present(const uint8_t* screen_atari);  // buffer is 384×240 bytes

}
