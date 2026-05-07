#include "frame_pacer.h"

static int32_t elapsed_since(uint32_t now_us, uint32_t then_us) {
  return (int32_t)(now_us - then_us);
}

void frame_pacer_reset(frame_pacer_t* pacer, uint32_t now_us) {
  pacer->next_frame_us = now_us;
  pacer->initialized = 1;
  pacer->skip_next_render = 0;
}

int frame_pacer_due(frame_pacer_t* pacer, uint32_t now_us) {
  if (!pacer->initialized) {
    frame_pacer_reset(pacer, now_us);
    return 1;
  }
  return elapsed_since(now_us, pacer->next_frame_us) >= 0;
}

void frame_pacer_advance(frame_pacer_t* pacer, uint32_t now_us) {
  if (!pacer->initialized) {
    frame_pacer_reset(pacer, now_us);
    return;
  }

  pacer->next_frame_us += FRAME_PACER_FRAME_US;
  if (elapsed_since(now_us, pacer->next_frame_us) > (int32_t)(FRAME_PACER_FRAME_US * 5U)) {
    pacer->next_frame_us = now_us + FRAME_PACER_FRAME_US;
  }
}

int frame_pacer_take_render_slot(frame_pacer_t* pacer) {
  if (pacer->skip_next_render) {
    pacer->skip_next_render = 0;
    return 0;
  }
  return 1;
}

void frame_pacer_finish_frame(frame_pacer_t* pacer, uint32_t frame_us, int rendered) {
  if (rendered && frame_us > FRAME_PACER_FRAME_US) {
    pacer->skip_next_render = 1;
  }
}
