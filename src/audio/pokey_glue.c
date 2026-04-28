#include "pokey_glue.h"

/* Declared in atari800's pokeysnd.h; re-declared here so this module can be
   host-built with the mock test rig without pulling in all of pokeysnd.h. */
extern int          POKEYSND_enable_new_pokey;
extern int          POKEYSND_stereo_enabled;
extern unsigned char POKEYSND_num_pokeys;
extern int          POKEYSND_snd_flags;
extern long         POKEYSND_playback_freq;

extern int  POKEYSND_Init(unsigned long freq17, int playback_freq,
                          unsigned char num_pokeys, int flags);
extern void POKEYSND_Process(void* buf, int sndn);

/* POKEYSND_FREQ_17_APPROX (pokeysnd.h:63) — even-dividing 1.79 MHz clock. */
#define FREQ_17_APPROX 1787520UL

/* POKEYSND_BIT16 (pokeysnd.h). */
#define BIT16 0x01

int pokey_glue_init(int playback_freq, int stereo) {
  /* Step 1: mzpokeysnd is absent — force the classic Ron Fries path. */
  POKEYSND_enable_new_pokey = 0;
  POKEYSND_stereo_enabled   = stereo ? 1 : 0;

  int num_pokeys = stereo ? 2 : 1;
  int flags      = BIT16;
  /* POKEYSND_Init returns 0 on success, non-zero on error — see
     pokeysnd.c line 459 (the "return 0; OK" comment). Invert to the
     common "1 = success" convention for our caller. */
  int err = POKEYSND_Init(FREQ_17_APPROX, playback_freq,
                          (unsigned char)num_pokeys, flags);
  return (err == 0) ? 1 : 0;
}

void pokey_glue_fill(int16_t* buf, int frames, int stereo) {
  int samples = frames * (stereo ? 2 : 1);
  POKEYSND_Process(buf, samples);
}

void pokey_glue_set_stereo(int stereo) {
  POKEYSND_stereo_enabled = stereo ? 1 : 0;
  POKEYSND_num_pokeys     = stereo ? 2 : 1;
  /* NOTE: changing num_pokeys without re-Init leaves the internal filter
     state mid-compute. Use pokey_glue_init(freq, stereo) if you want a
     clean swap. This lightweight path is fine when the menu toggle is
     followed by a brief silence (audio::pause)+resume. */
}
