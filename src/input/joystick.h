/* joystick.h — dual-cluster joystick resolver.
   Takes booleans for each cluster's direction + fire bits and produces
   one active-low nibble + one fire bit for Joystick-1.

   Pure C so it host-tests without M5Cardputer. */

#ifndef CARDPUTER_JOYSTICK_H
#define CARDPUTER_JOYSTICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned up    : 1;
  unsigned down  : 1;
  unsigned left  : 1;
  unsigned right : 1;
  unsigned fire  : 1;   /* primary: K / Z */
  unsigned fire2 : 1;   /* secondary: L / X */
} joy_cluster_t;

typedef struct {
  joy_cluster_t c1;   /* ESAD + KL */
  joy_cluster_t c2;   /* ;.,/ + ZX */
} joy_state_t;

/* Resolve dual-cluster state to a single Joystick-1 encoding.
   *nibble_out: active-low 4-bit value in [0x00..0x0F]. 0x0F = centre.
                Encoding matches atari800 INPUT_STICK_*:
                  bit 0 cleared = forward (up)
                  bit 1 cleared = back    (down)
                  bit 2 cleared = left
                  bit 3 cleared = right
   *fire_out: 1 if any fire bit (primary or secondary) pressed, else 0.

   Block-opposite policy: if both up+down or both left+right are pressed
   simultaneously, only the first-encountered direction (up, left) is
   emitted. This mirrors atari800's INPUT_joy_block_opposite_directions=1. */
void joystick_resolve(const joy_state_t* in,
                      uint8_t* nibble_out, int* fire_out);

/* Apply the resolved joystick state to Atari hardware-facing registers.
   Drives joystick port 1 directions and trigger 1. Also explicitly releases
   trigger 2; leaving it at the C zero default looks like a permanently
   pressed second fire button to games that check both trigger lines. */
void joystick_apply_to_atari(uint8_t* pia_port0,
                             uint8_t gtia_trig[4],
                             uint8_t nibble,
                             int fire);

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_JOYSTICK_H */
