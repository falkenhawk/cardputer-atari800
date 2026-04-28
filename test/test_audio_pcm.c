/* test_audio_pcm.c - pure audio sizing/format helpers used by audio_out. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/audio/audio_pcm.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  CHECK(audio_pcm_frames_per_video_frame(48000, 50) == 960,
        "48 kHz / 50 Hz -> 960 frames per pump");
  CHECK(audio_pcm_frames_per_video_frame(44100, 50) == 882,
        "44.1 kHz / 50 Hz -> 882 frames per pump");

  int16_t pokey[4] = {-32768, -30000, -1, 0};
  audio_pcm_recenter_pokey16(pokey, 4);
  CHECK(pokey[0] == 0, "POKEY silence -32768 recenters to I2S zero");
  CHECK(pokey[1] == 2768, "negative POKEY sample recenters upward");
  CHECK(pokey[2] == 32767, "near-mid POKEY sample recenters to positive peak");
  CHECK(pokey[3] == -32768, "wrapped POKEY midpoint remains representable");

  int16_t samples[8] = {0, 1234, -1234, 32767, 0, 0, 0, 0};
  audio_pcm_expand_mono_to_stereo(samples, 4);
  CHECK(samples[0] == 0 && samples[1] == 0, "silence stays signed zero");
  CHECK(samples[2] == 1234 && samples[3] == 1234, "positive sample duplicated");
  CHECK(samples[4] == -1234 && samples[5] == -1234, "negative sample duplicated");
  CHECK(samples[6] == 32767 && samples[7] == 32767, "peak sample duplicated");

  if (fail) return EXIT_FAILURE;
  printf("PASS: audio_pcm\n");
  return EXIT_SUCCESS;
}
