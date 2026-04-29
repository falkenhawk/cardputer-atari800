/* audio_console_speaker.h - fast-path Atari console speaker mixer. */

#ifndef CARDPUTER_AUDIO_CONSOLE_SPEAKER_H
#define CARDPUTER_AUDIO_CONSOLE_SPEAKER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_console_speaker_reset(void);
void audio_console_speaker_init(int16_t* ring, int ring_frames);
int audio_console_speaker_static_bytes(void);
void audio_console_speaker_write(int level, uint32_t cpu_clock);
int audio_console_speaker_frame_end(uint32_t frame_end_clock,
                                    int ticks_per_frame,
                                    int sample_rate,
                                    int video_hz);
int audio_console_speaker_mix_stereo(int16_t* samples, int frames);
int audio_console_speaker_discard(int frames);

#ifdef __cplusplus
}
#endif

#endif
