/* pokey_glue.h — thin wrapper over atari800's POKEYSND_* audio engine.

   Handles the two non-obvious gotchas:
   1. POKEYSND_enable_new_pokey must be FALSE because mzpokeysnd.c is NOT
      vendored. Setting this implicitly selects the classic Ron Fries code
      path inside pokeysnd.c.
   2. POKEYSND_Process's 'sndn' parameter is SAMPLES, not frames. For stereo,
      that's 2x the frame count. This wrapper converts for you. */

#ifndef CARDPUTER_POKEY_GLUE_H
#define CARDPUTER_POKEY_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize POKEY audio.
   playback_freq: Hz (typical 44100).
   stereo: 0 = single POKEY (mono output), 1 = dual POKEY (stereo L/R).
   Returns 1 on success, 0 on failure. */
int pokey_glue_init(int playback_freq, int stereo);

/* Fill `buf` with `frames` audio frames. `stereo` MUST match the value
   passed to pokey_glue_init. Output is 16-bit signed PCM interleaved
   (L R L R ... for stereo). */
void pokey_glue_fill(int16_t* buf, int frames, int stereo);

/* Reset the stereo bit WITHOUT re-initialising — cheap path for the
   menu toggle. Re-Init is the safe one if you need a full reset. */
void pokey_glue_set_stereo(int stereo);

#ifdef __cplusplus
}
#endif

#endif
