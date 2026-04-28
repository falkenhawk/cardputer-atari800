#include "pokey_fast.h"

#include <stdint.h>

extern unsigned char POKEY_AUDF[8];
extern unsigned char POKEY_AUDC[8];
extern unsigned char POKEY_AUDCTL[2];

#define AUDC_NOTPOLY5    0x80
#define AUDC_PURETONE    0x20
#define AUDC_VOL_ONLY    0x10
#define AUDC_VOLUME_MASK 0x0f

#define AUDCTL_CH1_179   0x40
#define AUDCTL_CH3_179   0x20
#define AUDCTL_CLOCK_15  0x01

#define POKEY_CLOCK_HZ   1787520u
#define POKEY_DIV_64     28u
#define POKEY_DIV_15     114u
#define MAX_CHANNELS     8
#define VOLUME_SCALE     480
#define SMOOTH_SHIFT     2

static uint32_t g_phase[MAX_CHANNELS];
static uint32_t g_noise[MAX_CHANNELS];
static int32_t  g_smooth_q8[MAX_CHANNELS];

static int16_t clamp16(int sample) {
  if (sample > 32767) return 32767;
  if (sample < -32768) return -32768;
  return (int16_t)sample;
}

void pokey_fast_reset(void) {
  for (int i = 0; i < MAX_CHANNELS; i++) {
    g_phase[i] = 0;
    g_noise[i] = 0x1ffffu ^ (uint32_t)(i * 0x1234u);
    g_smooth_q8[i] = 0;
  }
}

static uint32_t channel_step(int ch, int sample_rate) {
  int chip = ch >> 2;
  int local = ch & 3;
  unsigned char audctl = POKEY_AUDCTL[chip];
  unsigned int divisor;

  if ((local == 0 && (audctl & AUDCTL_CH1_179)) ||
      (local == 2 && (audctl & AUDCTL_CH3_179))) {
    divisor = (unsigned int)POKEY_AUDF[ch] + 4u;
  } else {
    unsigned int base_div = (audctl & AUDCTL_CLOCK_15) ? POKEY_DIV_15
                                                       : POKEY_DIV_64;
    divisor = ((unsigned int)POKEY_AUDF[ch] + 1u) * base_div;
  }

  if (divisor == 0 || sample_rate <= 0) return 0;
  uint32_t freq = POKEY_CLOCK_HZ / (2u * divisor);
  return (uint32_t)(((uint64_t)freq << 32) / (uint32_t)sample_rate);
}

static int channel_sample(int ch, int sample_rate) {
  unsigned char audc = POKEY_AUDC[ch];
  int volume = audc & AUDC_VOLUME_MASK;
  if (volume == 0) {
    g_smooth_q8[ch] = 0;
    return 0;
  }

  int amp = volume * VOLUME_SCALE;
  int target;
  if (audc & AUDC_VOL_ONLY) {
    target = amp;
    goto smooth;
  }

  uint32_t prev = g_phase[ch];
  uint32_t step = channel_step(ch, sample_rate);
  g_phase[ch] += step;

  int pure = ((audc & (AUDC_NOTPOLY5 | AUDC_PURETONE)) ==
              (AUDC_NOTPOLY5 | AUDC_PURETONE));
  if (!pure && g_phase[ch] < prev) {
    uint32_t bit = ((g_noise[ch] >> 0) ^ (g_noise[ch] >> 5)) & 1u;
    g_noise[ch] = (g_noise[ch] >> 1) | (bit << 16);
  }

  int high = pure ? ((g_phase[ch] & 0x80000000u) != 0)
                  : ((g_noise[ch] & 1u) != 0);
  target = high ? amp : -amp;

smooth:
  {
    int32_t target_q8 = (int32_t)target << 8;
    g_smooth_q8[ch] += (target_q8 - g_smooth_q8[ch]) >> SMOOTH_SHIFT;
    return (int)(g_smooth_q8[ch] >> 8);
  }
}

void pokey_fast_fill(int16_t* buf, int frames, int stereo, int sample_rate) {
  if (!buf || frames <= 0) return;

  if (stereo) {
    for (int i = 0; i < frames; i++) {
      int left = 0;
      int right = 0;
      for (int ch = 0; ch < 4; ch++) left += channel_sample(ch, sample_rate);
      for (int ch = 4; ch < 8; ch++) right += channel_sample(ch, sample_rate);
      buf[2 * i] = clamp16(left);
      buf[2 * i + 1] = clamp16(right);
    }
    return;
  }

  for (int i = 0; i < frames; i++) {
    int mono = 0;
    for (int ch = 0; ch < 4; ch++) mono += channel_sample(ch, sample_rate);
    buf[i] = clamp16(mono);
  }
}
