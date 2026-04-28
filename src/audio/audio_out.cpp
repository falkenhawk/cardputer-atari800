/* audio_out.cpp — legacy ESP-IDF i2s driver + M5.In_I2C for ES8311.

   Bypass M5Unified's Speaker_Class to try to shrink link-time BSS that
   breaks SD mount. The legacy `driver/i2s.h` API is ESP-IDF 4.x-style
   (i2s_driver_install / i2s_set_pin / i2s_write). Our arduino-esp32
   toolchain doesn't have the newer i2s_std.h driver. */

#include "audio_out.h"

#include <M5Cardputer.h>
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern "C" {
#include "audio_pcm.h"
#include "pokey_fast.h"
#include "pokey_glue.h"
}

namespace {

/* Cardputer-Adv I2S pins (M5Unified.cpp:2089-2102). */
constexpr gpio_num_t PIN_BCK  = GPIO_NUM_41;
constexpr gpio_num_t PIN_WS   = GPIO_NUM_43;
constexpr gpio_num_t PIN_DOUT = GPIO_NUM_42;
constexpr i2s_port_t I2S_PORT = I2S_NUM_1;

constexpr uint8_t ES8311_ADDR = 0x18;

/* Match M5Unified's Cardputer-Adv codec default. One pump happens at the end
   of each PAL Atari frame, so the buffer must contain 20 ms of PCM. */
constexpr int SAMPLE_RATE       = AUDIO_PCM_SAMPLE_RATE;
constexpr int FRAMES_PER_BUFFER = AUDIO_PCM_FRAMES_PER_PUMP;
constexpr int OUTPUT_CHANNELS   = AUDIO_PCM_OUTPUT_CHANNELS;

bool     g_muted    = false;
uint8_t  g_vol_reg  = 0xBF;

bool     g_i2s_ok   = false;
bool     g_pokey_ok = false;
bool     g_stereo   = false;

int16_t* g_buf      = nullptr;

bool es8311_write(uint8_t reg, uint8_t val) {
  return M5.In_I2C.writeRegister8(ES8311_ADDR, reg, val, 400000);
}

bool es8311_init() {
  static const uint8_t regs[][2] = {
    {0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18},
    {0x0D, 0x01}, {0x12, 0x00}, {0x13, 0x10},
    {0x32, 0xBF}, {0x37, 0x08},
  };
  for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    if (!es8311_write(regs[i][0], regs[i][1])) {
      Serial.printf("audio(raw): ES8311 reg 0x%02X write failed\n", regs[i][0]);
      return false;
    }
  }
  return true;
}

bool i2s_init_legacy() {
  i2s_config_t cfg = {};
  cfg.mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate       = SAMPLE_RATE;
  cfg.bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format    = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags  = 0;
  /* DMA size is bounded by DMA-capable contiguous heap at init time. Stereo
     6 x 128 frames uses the same ~3 KB DMA-cap memory as the earlier mono
     6 x 256 setup, while giving enough buffering to avoid short gaps. */
  cfg.dma_buf_count     = 6;
  cfg.dma_buf_len       = 128;
  cfg.use_apll          = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk        = 0;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("audio(raw): i2s_driver_install err=0x%x\n", (unsigned)err);
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.bck_io_num   = PIN_BCK;
  pins.ws_io_num    = PIN_WS;
  pins.data_out_num = PIN_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;
  pins.mck_io_num   = I2S_PIN_NO_CHANGE;

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    Serial.printf("audio(raw): i2s_set_pin err=0x%x\n", (unsigned)err);
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

} /* anonymous namespace */

namespace audio_out {

bool preallocate_buffers() {
  const size_t bytes = FRAMES_PER_BUFFER * OUTPUT_CHANNELS * sizeof(int16_t);
  g_buf = (int16_t*)heap_caps_malloc(bytes,
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_buf) return false;
  memset(g_buf, 0, bytes);
  return true;
}

bool start(int initial_stereo) {
  multi_heap_info_t hi;
  heap_caps_get_info(&hi, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf("audio(raw): pre-init heap DMA|INT|8: free=%u largest=%u\n",
                (unsigned)hi.total_free_bytes, (unsigned)hi.largest_free_block);

  if (!es8311_init()) {
    Serial.println("audio(raw): ES8311 init failed");
    return false;
  }
  Serial.println("audio(raw): ES8311 configured");

  if (!i2s_init_legacy()) {
    Serial.println("audio(raw): i2s_init_legacy failed");
    return false;
  }
  g_i2s_ok = true;
  Serial.println("audio(raw): I2S driver installed");

  g_stereo = (initial_stereo != 0);
  pokey_glue_set_stereo(g_stereo ? 1 : 0);
  pokey_fast_reset();
  g_pokey_ok = true;

  Serial.printf("audio(raw): started i2s_ok=%d pokey_ok=%d stereo=%d\n",
                g_i2s_ok, g_pokey_ok, g_stereo);
  return true;
}

void set_muted(bool muted) {
  if (muted && !g_muted) {
    es8311_write(0x32, 0x00);
  } else if (!muted && g_muted) {
    es8311_write(0x32, g_vol_reg);
  }
  g_muted = muted;
}

void set_stereo(bool stereo) {
  if (stereo == g_stereo) return;
  pokey_glue_set_stereo(stereo ? 1 : 0);
  pokey_fast_reset();
  g_stereo = stereo;
}

void set_volume_delta(int8_t delta) {
  int v = (int)g_vol_reg + (int)delta;
  if (v < 0x00) v = 0x00;
  if (v > 0xFF) v = 0xFF;
  g_vol_reg = (uint8_t)v;
  if (!g_muted) es8311_write(0x32, g_vol_reg);
}

void pump() {
  if (!g_i2s_ok || !g_pokey_ok || g_muted) return;
  if (!g_buf) return;

  uint32_t t0 = micros();
  pokey_fast_fill(g_buf, FRAMES_PER_BUFFER, g_stereo ? 1 : 0, SAMPLE_RATE);
  if (!g_stereo) {
    audio_pcm_expand_mono_to_stereo(g_buf, FRAMES_PER_BUFFER);
  }
  uint32_t t1 = micros();

  size_t written = 0;
  i2s_write(I2S_PORT, g_buf,
            FRAMES_PER_BUFFER * OUTPUT_CHANNELS * sizeof(int16_t),
            &written, pdMS_TO_TICKS(30));
  uint32_t t2 = micros();

  static uint32_t pump_count = 0;
  if ((pump_count % 50) == 0) {
    Serial.printf("pump %lu: pokey=%lu i2s=%lu written=%u s[0..3]=%d,%d,%d,%d\n",
                  (unsigned long)pump_count,
                  (unsigned long)(t1 - t0),
                  (unsigned long)(t2 - t1),
                  (unsigned)written,
                  g_buf[0], g_buf[1], g_buf[2], g_buf[3]);
  }
  pump_count++;
}

} /* namespace audio_out */

extern "C" void Sound_Update(void) {
  audio_out::pump();
}
