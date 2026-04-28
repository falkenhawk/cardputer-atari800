/* test_pokey_fast.c - fast register-driven POKEY synth smoke tests. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/audio/pokey_fast.h"

unsigned char POKEY_AUDF[8];
unsigned char POKEY_AUDC[8];
unsigned char POKEY_AUDCTL[2];

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

static int has_nonzero(const int16_t* samples, int count) {
  for (int i = 0; i < count; i++) {
    if (samples[i] != 0) return 1;
  }
  return 0;
}

static int max_abs_delta(const int16_t* samples, int count) {
  int max_delta = 0;
  for (int i = 1; i < count; i++) {
    int delta = (int)samples[i] - (int)samples[i - 1];
    if (delta < 0) delta = -delta;
    if (delta > max_delta) max_delta = delta;
  }
  return max_delta;
}

static int has_full_scale_clip(const int16_t* samples, int count) {
  for (int i = 0; i < count; i++) {
    if (samples[i] == 32767 || samples[i] == -32768) return 1;
  }
  return 0;
}

int main(void) {
  int16_t buf[512];

  pokey_fast_reset();
  pokey_fast_fill(buf, 64, 0, 48000);
  CHECK(!has_nonzero(buf, 64), "silent registers produce zero PCM");

  /* Atari BASIC SOUND 0,121,10,8 writes AUDF1=121, AUDC1=0xA8. */
  POKEY_AUDF[0] = 121;
  POKEY_AUDC[0] = 0xA8;
  pokey_fast_fill(buf, 512, 0, 48000);
  CHECK(has_nonzero(buf, 512), "BASIC SOUND 0,121,10,8 produces tone PCM");
  CHECK(max_abs_delta(buf, 512) <= 3072,
        "pure tone edges are smoothed for the small speaker");

  pokey_fast_reset();
  for (int ch = 0; ch < 4; ch++) {
    POKEY_AUDF[ch] = (unsigned char)(31 + ch * 17);
    POKEY_AUDC[ch] = 0xAF;
  }
  pokey_fast_fill(buf, 512, 0, 48000);
  CHECK(!has_full_scale_clip(buf, 512), "stacked channels avoid full-scale clipping");

  for (int ch = 0; ch < 4; ch++) {
    POKEY_AUDC[ch] = 0;
  }
  pokey_fast_fill(buf, 64, 0, 48000);
  CHECK(!has_nonzero(buf, 64), "SOUND 0,0,0,0 silences channel");

  if (fail) return EXIT_FAILURE;
  printf("PASS: pokey_fast\n");
  return EXIT_SUCCESS;
}
