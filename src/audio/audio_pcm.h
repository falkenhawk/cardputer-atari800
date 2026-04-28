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

int audio_pcm_frames_per_video_frame(int sample_rate, int video_hz);
void audio_pcm_recenter_pokey16(int16_t* samples, int count);
void audio_pcm_expand_mono_to_stereo(int16_t* samples, int frames);

#ifdef __cplusplus
}
#endif

#endif
