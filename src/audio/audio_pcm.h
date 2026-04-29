/* audio_pcm.h - pure helpers for sizing/formatting PCM buffers. */

#ifndef CARDPUTER_AUDIO_PCM_H
#define CARDPUTER_AUDIO_PCM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_PCM_SAMPLE_RATE       48000
#define AUDIO_PCM_VIDEO_HZ_PAL      50
#define AUDIO_PCM_OUTPUT_CHANNELS   2
#define AUDIO_PCM_FRAMES_PER_PUMP   (AUDIO_PCM_SAMPLE_RATE / AUDIO_PCM_VIDEO_HZ_PAL)
#define AUDIO_PCM_STREAM_CHUNK_FRAMES 256
#define AUDIO_PCM_RING_FRAMES       1536

typedef struct audio_pcm_ring_t {
  int16_t samples[AUDIO_PCM_RING_FRAMES * AUDIO_PCM_OUTPUT_CHANNELS];
  int read_frame;
  int queued_frames;
} audio_pcm_ring_t;

typedef struct audio_pcm_dc_blocker_t {
  int32_t prev_x[AUDIO_PCM_OUTPUT_CHANNELS];
  int32_t prev_y[AUDIO_PCM_OUTPUT_CHANNELS];
} audio_pcm_dc_blocker_t;

typedef struct audio_pcm_suppressor_t {
  int remaining_frames;
} audio_pcm_suppressor_t;

int audio_pcm_frames_per_video_frame(int sample_rate, int video_hz);
void audio_pcm_recenter_pokey16(int16_t* samples, int count);
void audio_pcm_expand_mono_to_stereo(int16_t* samples, int frames);
void audio_pcm_ring_init(audio_pcm_ring_t* ring);
int audio_pcm_ring_available(const audio_pcm_ring_t* ring);
int audio_pcm_ring_write_stereo(audio_pcm_ring_t* ring, const int16_t* samples, int frames);
int audio_pcm_ring_write_mono_as_stereo(audio_pcm_ring_t* ring, const int16_t* samples, int frames);
int audio_pcm_ring_read_stereo(audio_pcm_ring_t* ring, int16_t* samples, int frames);
void audio_pcm_dc_blocker_init(audio_pcm_dc_blocker_t* dc);
void audio_pcm_dc_block_stereo(audio_pcm_dc_blocker_t* dc, int16_t* samples, int frames);
void audio_pcm_suppressor_init(audio_pcm_suppressor_t* suppressor);
void audio_pcm_suppressor_start(audio_pcm_suppressor_t* suppressor, int frames);
int audio_pcm_suppressor_active(const audio_pcm_suppressor_t* suppressor);
int audio_pcm_suppressor_consume(audio_pcm_suppressor_t* suppressor, int frames);

#ifdef __cplusplus
}
#endif

#endif
