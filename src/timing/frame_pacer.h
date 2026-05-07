#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_PACER_FRAME_US 20000U

typedef struct {
  uint32_t next_frame_us;
  uint8_t initialized;
  uint8_t skip_next_render;
} frame_pacer_t;

void frame_pacer_reset(frame_pacer_t* pacer, uint32_t now_us);
int frame_pacer_due(frame_pacer_t* pacer, uint32_t now_us);
void frame_pacer_advance(frame_pacer_t* pacer, uint32_t now_us);
int frame_pacer_take_render_slot(frame_pacer_t* pacer);
void frame_pacer_finish_frame(frame_pacer_t* pacer, uint32_t frame_us, int rendered);

#ifdef __cplusplus
}
#endif
