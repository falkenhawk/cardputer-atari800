#pragma once
#include <stdint.h>

/* LCD framebuffer capture. renderer::present() calls into this on the frame
   right after screenshot::arm() was invoked; it writes exactly what was
   pushed to the LCD (post-projector, post-palette, post-byte-swap-from-our-
   perspective) as a 16-bit BMP to SD at /sd/atari800/screenshots/shot_<ms>.bmp. */
namespace screenshot {

/* Request a capture on the next frame. Noop if one is already pending. */
void arm();

/* True if a capture is armed. Renderer checks this each frame. */
bool is_armed();

/* Start a new capture. Opens the file, writes BMP header. Returns true on
   success. Called by renderer at the top of present(). */
bool begin(int width, int height);

/* Write one horizontal line (`rgb565`, native-endian uint16_t). Called by
   renderer in row order 0..height-1. */
void write_line(const uint16_t* rgb565, int width);

/* Close the file. Clears armed flag. */
void end();

} /* namespace screenshot */
