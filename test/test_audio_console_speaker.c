/* test_audio_console_speaker.c - hybrid console speaker mixer behavior. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/audio/audio_console_speaker.h"

void audio_console_speaker_init(int16_t* ring, int ring_frames);
int audio_console_speaker_static_bytes(void);

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  int16_t stereo[40] = {0};
  int16_t ring[1024] = {0};

  CHECK(audio_console_speaker_static_bytes() <= 768,
        "console mixer keeps static BSS below SD-mount-sensitive budget");
  audio_console_speaker_init(ring, 1024);
  audio_console_speaker_reset();
  audio_console_speaker_write(1, 10);
  audio_console_speaker_write(0, 30);
  CHECK(audio_console_speaker_frame_end(100, 100, 100, 10) == 10,
        "100 Hz / 10 Hz video frame renders 10 console frames");
  CHECK(audio_console_speaker_mix_stereo(stereo, 10) == 10,
        "mixer drains the rendered console frame");
  CHECK(stereo[0] == 0 && stereo[1] == 0,
        "speaker remains silent before the first event");
  CHECK(stereo[2] == 8192 && stereo[3] == 8192 &&
        stereo[4] == 8192 && stereo[5] == 8192,
        "speaker high interval is mixed into both stereo channels");
  CHECK(stereo[6] == 0 && stereo[7] == 0,
        "speaker turns off at the second event");

  audio_console_speaker_reset();
  audio_console_speaker_write(1, 110);
  CHECK(audio_console_speaker_frame_end(200, 100, 100, 10) == 10,
        "first frame renders after a non-zero starting clock");
  CHECK(audio_console_speaker_frame_end(300, 100, 100, 10) == 10,
        "held speaker level carries into the next frame");
  for (int i = 0; i < 40; i++) stereo[i] = 0;
  CHECK(audio_console_speaker_mix_stereo(stereo, 20) == 20,
        "mixer drains consecutive rendered frames");
  CHECK(stereo[0] == 0 && stereo[1] == 0,
        "first sample before held-high edge stays silent");
  CHECK(stereo[2] == 8192 && stereo[3] == 8192,
        "held-high edge starts at the expected frame offset");
  CHECK(stereo[38] == 8192 && stereo[39] == 8192,
        "held-high level persists until a low edge arrives");

  audio_console_speaker_reset();
  audio_console_speaker_write(1, 0);
  CHECK(audio_console_speaker_frame_end(100, 100, 100, 10) == 10,
        "rendered frame can be queued for discard");
  CHECK(audio_console_speaker_discard(4) == 4,
        "discard removes queued speaker frames during reset suppression");
  for (int i = 0; i < 20; i++) stereo[i] = 0;
  CHECK(audio_console_speaker_mix_stereo(stereo, 10) == 6,
        "discarded frames are not mixed later");
  CHECK(stereo[0] == 8192 && stereo[1] == 8192,
        "remaining queued speaker frames keep their order");

  if (fail) return EXIT_FAILURE;
  printf("PASS: audio_console_speaker\n");
  return EXIT_SUCCESS;
}
