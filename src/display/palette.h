// palette.h — Atari 256-color palette lookup tables (PAL + NTSC) in RGB565.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lazy-compute on first call, then return a cached pointer to 256 uint16_t. */
const uint16_t* palette_get_pal(void);
const uint16_t* palette_get_ntsc(void);

#ifdef __cplusplus
}
#endif
