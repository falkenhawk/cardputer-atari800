#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/timing/frame_pacer.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  frame_pacer_t pacer = {0};

  frame_pacer_reset(&pacer, 1000);
  CHECK(frame_pacer_due(&pacer, 999) == 0, "not due before first scheduled frame");
  CHECK(frame_pacer_due(&pacer, 1000) == 1, "due at first scheduled frame");

  frame_pacer_advance(&pacer, 1000);
  CHECK(pacer.next_frame_us == 21000, "advance uses fixed PAL cadence");
  CHECK(frame_pacer_due(&pacer, 20000) == 0, "not due before next cadence");
  CHECK(frame_pacer_due(&pacer, 21000) == 1, "due at next cadence");

  frame_pacer_advance(&pacer, 28000);
  CHECK(pacer.next_frame_us == 41000, "late frame does not move cadence to now");

  CHECK(frame_pacer_take_render_slot(&pacer) == 1, "renders by default");
  frame_pacer_finish_frame(&pacer, 27000, 1);
  CHECK(frame_pacer_take_render_slot(&pacer) == 0, "slow rendered frame skips next render");
  CHECK(frame_pacer_take_render_slot(&pacer) == 1, "skip is consumed once");

  frame_pacer_finish_frame(&pacer, 10000, 0);
  CHECK(frame_pacer_take_render_slot(&pacer) == 1, "fast skipped frame does not request another skip");

  pacer.next_frame_us = 41000;
  frame_pacer_advance(&pacer, 200000);
  CHECK(pacer.next_frame_us == 220000, "large stall resyncs schedule");

  if (fail) return EXIT_FAILURE;
  puts("PASS: frame_pacer");
  return EXIT_SUCCESS;
}
