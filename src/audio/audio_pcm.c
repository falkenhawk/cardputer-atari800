#include "audio_pcm.h"

int audio_pcm_frames_per_video_frame(int sample_rate, int video_hz) {
  if (sample_rate <= 0 || video_hz <= 0) return 0;
  return sample_rate / video_hz;
}

void audio_pcm_recenter_pokey16(int16_t* samples, int count) {
  if (!samples || count <= 0) return;
  for (int i = 0; i < count; i++) {
    samples[i] = (int16_t)((uint16_t)samples[i] ^ 0x8000u);
  }
}

void audio_pcm_expand_mono_to_stereo(int16_t* samples, int frames) {
  if (!samples || frames <= 0) return;
  for (int i = frames - 1; i >= 0; i--) {
    int16_t s = samples[i];
    samples[2 * i] = s;
    samples[2 * i + 1] = s;
  }
}
