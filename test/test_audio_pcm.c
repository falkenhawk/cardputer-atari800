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
  CHECK(AUDIO_PCM_STREAM_CHUNK_FRAMES > 0,
        "stream chunk has a positive frame count");
  CHECK(AUDIO_PCM_STREAM_CHUNK_FRAMES < AUDIO_PCM_FRAMES_PER_PUMP,
        "stream chunk is shorter than a video-frame pump");
  CHECK((AUDIO_PCM_STREAM_CHUNK_FRAMES % 2) == 0,
        "stream chunk keeps stereo DMA writes frame-aligned");

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

  audio_pcm_ring_t ring;
  int16_t ring_out[10] = {0};
  int16_t stereo_in[6] = {10, 11, 20, 21, 30, 31};
  audio_pcm_ring_init(&ring);
  CHECK(audio_pcm_ring_write_stereo(&ring, stereo_in, 3) == 3,
        "ring accepts stereo frames");
  CHECK(audio_pcm_ring_available(&ring) == 3,
        "ring reports queued frame count");
  CHECK(audio_pcm_ring_read_stereo(&ring, ring_out, 2) == 2,
        "ring reads requested stereo frames");
  CHECK(ring_out[0] == 10 && ring_out[1] == 11 &&
        ring_out[2] == 20 && ring_out[3] == 21,
        "ring preserves stereo interleaving");
  CHECK(audio_pcm_ring_available(&ring) == 1,
        "ring leaves unread frames queued");

  int16_t mono_in[3] = {-7, 0, 7};
  CHECK(audio_pcm_ring_write_mono_as_stereo(&ring, mono_in, 3) == 3,
        "ring duplicates mono frames into stereo");
  CHECK(audio_pcm_ring_read_stereo(&ring, ring_out, 4) == 4,
        "ring reads remaining plus mono-expanded frames");
  CHECK(ring_out[0] == 30 && ring_out[1] == 31,
        "ring returns older stereo frame first");
  CHECK(ring_out[2] == -7 && ring_out[3] == -7 &&
        ring_out[4] == 0 && ring_out[5] == 0 &&
        ring_out[6] == 7 && ring_out[7] == 7,
        "mono input is expanded to left and right");

  CHECK(audio_pcm_ring_read_stereo(&ring, ring_out, 1) == 0,
        "empty ring returns no frames");

  audio_pcm_dc_blocker_t dc;
  int16_t dc_buf[4096];
  audio_pcm_dc_blocker_init(&dc);
  for (int i = 0; i < 2048; i++) {
    dc_buf[2 * i] = 8192;
    dc_buf[2 * i + 1] = 8192;
  }
  audio_pcm_dc_block_stereo(&dc, dc_buf, 2048);
  CHECK(dc_buf[0] > 8000 && dc_buf[1] > 8000,
        "dc blocker preserves the leading edge of a speaker step");
  CHECK(dc_buf[4094] < 16 && dc_buf[4095] < 16,
        "dc blocker removes sustained console speaker offset");

  audio_pcm_suppressor_t suppressor;
  audio_pcm_suppressor_init(&suppressor);
  CHECK(audio_pcm_suppressor_consume(&suppressor, 960) == 0,
        "inactive suppressor does not drop audio");
  audio_pcm_suppressor_start(&suppressor, 1920);
  CHECK(audio_pcm_suppressor_active(&suppressor) == 1,
        "started suppressor reports active");
  CHECK(audio_pcm_suppressor_consume(&suppressor, 960) == 1,
        "suppressor drops first reset audio batch");
  CHECK(audio_pcm_suppressor_active(&suppressor) == 1,
        "partially consumed suppressor is still active");
  CHECK(audio_pcm_suppressor_consume(&suppressor, 960) == 1,
        "suppressor drops second reset audio batch");
  CHECK(audio_pcm_suppressor_active(&suppressor) == 0,
        "fully consumed suppressor reports inactive");
  CHECK(audio_pcm_suppressor_consume(&suppressor, 960) == 0,
        "suppressor lets later keyclick audio through");

  if (fail) return EXIT_FAILURE;
  printf("PASS: audio_pcm\n");
  return EXIT_SUCCESS;
}
