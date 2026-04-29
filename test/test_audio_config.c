/* test_audio_config.c - lock the firmware audio path to the non-blocking
   register-driven POKEY synth. */

#include "config.h"

#ifndef SOUND
#error "SOUND stays enabled so atari800 maintains POKEY state"
#endif

#ifdef SERIO_SOUND
#error "SERIO_SOUND must stay disabled in the fast path"
#endif

#ifdef CONSOLE_SOUND
#error "CONSOLE_SOUND must stay disabled in the fast path"
#endif

#ifdef VOL_ONLY_SOUND
#error "VOL_ONLY_SOUND must stay disabled: it adds large static sample buffers before MEMORY_mem allocation"
#endif

#ifdef SYNCHRONIZED_SOUND
#error "SYNCHRONIZED_SOUND must stay disabled: it blocks the emulator on POKEY-heavy games"
#endif

int main(void) {
  return 0;
}
