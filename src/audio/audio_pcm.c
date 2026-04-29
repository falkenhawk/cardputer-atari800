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

void audio_pcm_ring_init(audio_pcm_ring_t* ring) {
  if (!ring) return;
  ring->read_frame = 0;
  ring->queued_frames = 0;
}

int audio_pcm_ring_available(const audio_pcm_ring_t* ring) {
  if (!ring) return 0;
  return ring->queued_frames;
}

static int audio_pcm_ring_write_frame(audio_pcm_ring_t* ring, int16_t left, int16_t right) {
  if (!ring || ring->queued_frames >= AUDIO_PCM_RING_FRAMES) return 0;
  int write_frame = ring->read_frame + ring->queued_frames;
  if (write_frame >= AUDIO_PCM_RING_FRAMES) write_frame -= AUDIO_PCM_RING_FRAMES;
  ring->samples[2 * write_frame] = left;
  ring->samples[2 * write_frame + 1] = right;
  ring->queued_frames++;
  return 1;
}

int audio_pcm_ring_write_stereo(audio_pcm_ring_t* ring, const int16_t* samples, int frames) {
  if (!ring || !samples || frames <= 0) return 0;
  int written = 0;
  for (int i = 0; i < frames; i++) {
    if (!audio_pcm_ring_write_frame(ring, samples[2 * i], samples[2 * i + 1])) break;
    written++;
  }
  return written;
}

int audio_pcm_ring_write_mono_as_stereo(audio_pcm_ring_t* ring, const int16_t* samples, int frames) {
  if (!ring || !samples || frames <= 0) return 0;
  int written = 0;
  for (int i = 0; i < frames; i++) {
    if (!audio_pcm_ring_write_frame(ring, samples[i], samples[i])) break;
    written++;
  }
  return written;
}

int audio_pcm_ring_read_stereo(audio_pcm_ring_t* ring, int16_t* samples, int frames) {
  if (!ring || !samples || frames <= 0) return 0;
  int read = 0;
  for (int i = 0; i < frames && ring->queued_frames > 0; i++) {
    samples[2 * i] = ring->samples[2 * ring->read_frame];
    samples[2 * i + 1] = ring->samples[2 * ring->read_frame + 1];
    ring->read_frame++;
    if (ring->read_frame >= AUDIO_PCM_RING_FRAMES) ring->read_frame = 0;
    ring->queued_frames--;
    read++;
  }
  return read;
}

void audio_pcm_dc_blocker_init(audio_pcm_dc_blocker_t* dc) {
  if (!dc) return;
  for (int i = 0; i < AUDIO_PCM_OUTPUT_CHANNELS; i++) {
    dc->prev_x[i] = 0;
    dc->prev_y[i] = 0;
  }
}

static int16_t audio_pcm_clip16(int32_t sample) {
  if (sample > 32767) return 32767;
  if (sample < -32768) return -32768;
  return (int16_t)sample;
}

void audio_pcm_dc_block_stereo(audio_pcm_dc_blocker_t* dc, int16_t* samples, int frames) {
  if (!dc || !samples || frames <= 0) return;
  for (int i = 0; i < frames; i++) {
    for (int ch = 0; ch < AUDIO_PCM_OUTPUT_CHANNELS; ch++) {
      int idx = i * AUDIO_PCM_OUTPUT_CHANNELS + ch;
      int32_t x = samples[idx];
      int32_t y = x - dc->prev_x[ch] + ((dc->prev_y[ch] * 255) >> 8);
      dc->prev_x[ch] = x;
      dc->prev_y[ch] = y;
      samples[idx] = audio_pcm_clip16(y);
    }
  }
}

void audio_pcm_suppressor_init(audio_pcm_suppressor_t* suppressor) {
  if (!suppressor) return;
  suppressor->remaining_frames = 0;
}

void audio_pcm_suppressor_start(audio_pcm_suppressor_t* suppressor, int frames) {
  if (!suppressor) return;
  suppressor->remaining_frames = frames > 0 ? frames : 0;
}

int audio_pcm_suppressor_active(const audio_pcm_suppressor_t* suppressor) {
  return suppressor && suppressor->remaining_frames > 0;
}

int audio_pcm_suppressor_consume(audio_pcm_suppressor_t* suppressor, int frames) {
  if (!suppressor || frames <= 0 || suppressor->remaining_frames <= 0) return 0;
  if (frames >= suppressor->remaining_frames) {
    suppressor->remaining_frames = 0;
  } else {
    suppressor->remaining_frames -= frames;
  }
  return 1;
}
