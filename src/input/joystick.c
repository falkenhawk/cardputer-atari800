#include "joystick.h"

void joystick_resolve(const joy_state_t* in,
                      uint8_t* nibble_out, int* fire_out) {
  /* OR the two cluster's direction bits. */
  unsigned up    = in->c1.up    | in->c2.up;
  unsigned down  = in->c1.down  | in->c2.down;
  unsigned left  = in->c1.left  | in->c2.left;
  unsigned right = in->c1.right | in->c2.right;

  /* Block opposites. */
  if (up && down) down = 0;
  if (left && right) right = 0;

  uint8_t nib = 0x0F;
  if (up)    nib &= (uint8_t)~0x01;    /* clear bit 0 */
  if (down)  nib &= (uint8_t)~0x02;
  if (left)  nib &= (uint8_t)~0x04;
  if (right) nib &= (uint8_t)~0x08;

  *nibble_out = nib;
  *fire_out   = (in->c1.fire | in->c1.fire2 | in->c2.fire | in->c2.fire2) ? 1 : 0;
}

void joystick_apply_to_atari(uint8_t* pia_port0,
                             uint8_t gtia_trig[4],
                             uint8_t nibble,
                             int fire) {
  if (pia_port0) {
    *pia_port0 = (uint8_t)(0xF0 | (nibble & 0x0F));
  }
  if (gtia_trig) {
    gtia_trig[0] = fire ? 0 : 1;
    gtia_trig[1] = 1;
  }
}
