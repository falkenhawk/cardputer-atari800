/* pokey_fast.h - lightweight register-driven POKEY approximation.

   This is intentionally small and deterministic for the no-PSRAM Cardputer.
   It reads the core's POKEY_AUDF/AUDC/AUDCTL registers directly and emits
   signed 16-bit PCM centered at zero. */

#ifndef CARDPUTER_POKEY_FAST_H
#define CARDPUTER_POKEY_FAST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pokey_fast_reset(void);
void pokey_fast_fill(int16_t* buf, int frames, int stereo, int sample_rate);

#ifdef __cplusplus
}
#endif

#endif
