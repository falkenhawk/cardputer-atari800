#include "audio_console_speaker.h"

#include <stddef.h>

#define CONSOLE_GAIN             8192
#define CONSOLE_MAX_EVENTS       64
#define OUTPUT_CHANNELS          2

typedef struct console_event_t {
  uint32_t clock;
  uint8_t level;
} console_event_t;

static console_event_t g_events[CONSOLE_MAX_EVENTS];
static int g_event_count;
static int g_current_level;
static int g_frame_level;
static uint32_t g_frame_start_clock;
static int g_have_frame_start;

static int16_t* g_ring;
static int g_ring_frames;
static int g_ring_read;
static int g_ring_queued;

static int16_t clip16(int32_t sample) {
  if (sample > 32767) return 32767;
  if (sample < -32768) return -32768;
  return (int16_t)sample;
}

static int frame_offset(uint32_t clock, uint32_t start, int ticks, int frames) {
  uint32_t rel;
  if (ticks <= 0 || frames <= 0) return 0;
  rel = clock - start;
  if (rel >= (uint32_t)ticks) return frames;
  return (int)(((uint64_t)rel * (uint64_t)frames) / (uint32_t)ticks);
}

static void ring_write(int16_t sample) {
  int write_frame;
  if (!g_ring || g_ring_frames <= 0) return;
  if (g_ring_queued >= g_ring_frames) {
    g_ring_read++;
    if (g_ring_read >= g_ring_frames) g_ring_read = 0;
    g_ring_queued--;
  }
  write_frame = g_ring_read + g_ring_queued;
  if (write_frame >= g_ring_frames) write_frame -= g_ring_frames;
  g_ring[write_frame] = sample;
  g_ring_queued++;
}

static void render_run(int level, int frames) {
  int16_t sample;
  if (frames <= 0) return;
  sample = level ? CONSOLE_GAIN : 0;
  for (int i = 0; i < frames; i++) ring_write(sample);
}

void audio_console_speaker_reset(void) {
  g_event_count = 0;
  g_current_level = 0;
  g_frame_level = 0;
  g_frame_start_clock = 0;
  g_have_frame_start = 0;
  g_ring_read = 0;
  g_ring_queued = 0;
}

void audio_console_speaker_init(int16_t* ring, int ring_frames) {
  g_ring = ring;
  g_ring_frames = ring_frames > 0 ? ring_frames : 0;
  audio_console_speaker_reset();
}

int audio_console_speaker_static_bytes(void) {
  return (int)(sizeof(g_events) +
               sizeof(g_event_count) +
               sizeof(g_current_level) +
               sizeof(g_frame_level) +
               sizeof(g_frame_start_clock) +
               sizeof(g_have_frame_start) +
               sizeof(g_ring) +
               sizeof(g_ring_frames) +
               sizeof(g_ring_read) +
               sizeof(g_ring_queued));
}

void audio_console_speaker_write(int level, uint32_t cpu_clock) {
  level = level ? 1 : 0;
  if (level == g_current_level) return;
  g_current_level = level;
  if (g_event_count < CONSOLE_MAX_EVENTS) {
    g_events[g_event_count].clock = cpu_clock;
    g_events[g_event_count].level = (uint8_t)level;
    g_event_count++;
  } else {
    g_events[CONSOLE_MAX_EVENTS - 1].clock = cpu_clock;
    g_events[CONSOLE_MAX_EVENTS - 1].level = (uint8_t)level;
  }
}

int audio_console_speaker_frame_end(uint32_t frame_end_clock,
                                    int ticks_per_frame,
                                    int sample_rate,
                                    int video_hz) {
  int frames;
  int pos = 0;
  int level = g_frame_level;

  if (ticks_per_frame <= 0 || sample_rate <= 0 || video_hz <= 0) {
    g_event_count = 0;
    return 0;
  }

  frames = sample_rate / video_hz;
  if (frames <= 0) {
    g_event_count = 0;
    return 0;
  }

  if (!g_have_frame_start) {
    g_frame_start_clock = frame_end_clock >= (uint32_t)ticks_per_frame
                            ? frame_end_clock - (uint32_t)ticks_per_frame
                            : 0;
    g_have_frame_start = 1;
  }

  for (int i = 0; i < g_event_count; i++) {
    int off = frame_offset(g_events[i].clock, g_frame_start_clock,
                           ticks_per_frame, frames);
    if (off < pos) off = pos;
    render_run(level, off - pos);
    pos = off;
    level = g_events[i].level ? 1 : 0;
  }
  render_run(level, frames - pos);

  g_frame_start_clock = frame_end_clock;
  g_frame_level = g_current_level;
  g_event_count = 0;
  return frames;
}

int audio_console_speaker_mix_stereo(int16_t* samples, int frames) {
  int mixed = 0;
  if (!samples || frames <= 0 || !g_ring || g_ring_frames <= 0) return 0;
  while (mixed < frames && g_ring_queued > 0) {
    int16_t s = g_ring[g_ring_read];
    int idx = mixed * OUTPUT_CHANNELS;
    samples[idx] = clip16((int32_t)samples[idx] + s);
    samples[idx + 1] = clip16((int32_t)samples[idx + 1] + s);
    g_ring_read++;
    if (g_ring_read >= g_ring_frames) g_ring_read = 0;
    g_ring_queued--;
    mixed++;
  }
  return mixed;
}

int audio_console_speaker_discard(int frames) {
  int dropped;
  if (frames <= 0 || g_ring_queued <= 0 || !g_ring || g_ring_frames <= 0) return 0;
  dropped = frames < g_ring_queued ? frames : g_ring_queued;
  g_ring_read += dropped;
  while (g_ring_read >= g_ring_frames) {
    g_ring_read -= g_ring_frames;
  }
  g_ring_queued -= dropped;
  return dropped;
}
