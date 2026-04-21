// projector.h — maps Atari scanline → LCD line per display mode.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each function writes exactly 240 pixels into out_line.
   atari_line is at least 384 bytes of 8-bit palette indices
   (we sample from the center 320 — the active picture).
   palette is 256 entries of RGB565. */
void projector_stretch_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);

/* M2 only implements stretch. The other three fall through to Stretch;
   T15 gives them real implementations. */
void projector_pixel_perfect_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);
void projector_pillarbox_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);
void projector_cover_line(const uint8_t* atari_line, uint16_t* out_line, const uint16_t* palette);

#ifdef __cplusplus
}
#endif
