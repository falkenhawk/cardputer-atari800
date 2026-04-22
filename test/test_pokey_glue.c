/* test_pokey_glue.c — verify our glue correctly parameterises POKEYSND_Init
   and safely handles the mono/stereo toggle. Uses a mock of the atari800
   POKEYSND_* symbols. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/audio/pokey_glue.h"

/* ---- Mock POKEYSND globals + functions ---- */
int  POKEYSND_enable_new_pokey = 1;   /* default-TRUE per upstream */
int  POKEYSND_stereo_enabled   = 0;
unsigned char POKEYSND_num_pokeys = 1;
int  POKEYSND_snd_flags        = 0;
long POKEYSND_playback_freq    = 44100;

static int mock_init_called_with_freq = 0;
static int mock_init_called_with_num_pokeys = 0;
static int mock_init_called_with_flags = 0;
static int mock_init_return_val = 1;

int POKEYSND_Init(unsigned long freq17, int playback_freq,
                  unsigned char num_pokeys, int flags) {
  (void)freq17;
  mock_init_called_with_freq = playback_freq;
  mock_init_called_with_num_pokeys = num_pokeys;
  mock_init_called_with_flags = flags;
  return mock_init_return_val;
}

static int process_total_samples = 0;
void POKEYSND_Process(void* buf, int sndn) {
  (void)buf;
  process_total_samples += sndn;
}

/* ---- Tests ---- */
static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Mono init */
  int ok = pokey_glue_init(44100, 0 /* stereo=false */);
  CHECK(ok == 1, "init mono success");
  CHECK(POKEYSND_enable_new_pokey == 0, "enable_new_pokey forced to 0");
  CHECK(mock_init_called_with_freq == 44100, "freq passed through");
  CHECK(mock_init_called_with_num_pokeys == 1, "mono -> num_pokeys=1");
  CHECK((mock_init_called_with_flags & 0x01) != 0, "flags has BIT16");

  /* Stereo init */
  ok = pokey_glue_init(44100, 1 /* stereo=true */);
  CHECK(ok == 1, "init stereo success");
  CHECK(mock_init_called_with_num_pokeys == 2, "stereo -> num_pokeys=2");
  CHECK(POKEYSND_stereo_enabled == 1, "stereo flag set");

  /* Init failure propagates */
  mock_init_return_val = 0;
  ok = pokey_glue_init(44100, 0);
  CHECK(ok == 0, "init failure propagates");
  mock_init_return_val = 1;

  /* Process: 441 frames stereo = 882 samples */
  int16_t buf[882];
  pokey_glue_fill(buf, 441, 1);
  CHECK(process_total_samples == 882, "stereo 441 frames -> 882 samples");

  process_total_samples = 0;
  pokey_glue_fill(buf, 441, 0);
  CHECK(process_total_samples == 441, "mono 441 frames -> 441 samples");

  if (fail) return EXIT_FAILURE;
  printf("PASS: pokey_glue\n");
  return EXIT_SUCCESS;
}
