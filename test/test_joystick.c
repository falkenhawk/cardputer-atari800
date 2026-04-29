/* test_joystick.c — dual-cluster joystick encoding.
   A pressed direction CLEARS its bit (active-low). Centre = 0x0F. */

#include <stdio.h>
#include <stdlib.h>
#include "../src/input/joystick.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  joy_state_t in = {0};

  /* Idle -> 0x0F (INPUT_STICK_CENTRE), fire=0 */
  uint8_t nib; int fire;
  joystick_resolve(&in, &nib, &fire);
  CHECK(nib == 0x0f, "idle -> CENTRE 0x0F");
  CHECK(fire == 0,   "idle -> fire=0");

  /* Cluster-1 UP only */
  in = (joy_state_t){0};
  in.c1.up = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_FORWARD = 0x0E (bit 0 cleared) */
  CHECK(nib == 0x0e, "cluster1 up -> 0x0E");

  /* Cluster-2 RIGHT only */
  in = (joy_state_t){0};
  in.c2.right = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_RIGHT = 0x07 (bit 3 cleared) */
  CHECK(nib == 0x07, "cluster2 right -> 0x07");

  /* Both clusters press DIFFERENT directions -> OR (diagonal) */
  in = (joy_state_t){0};
  in.c1.up = 1;
  in.c2.right = 1;
  joystick_resolve(&in, &nib, &fire);
  /* INPUT_STICK_UR = 0x06 (bits 0 and 3 cleared) */
  CHECK(nib == 0x06, "up + right -> UR 0x06");

  /* Opposing directions: up + down should resolve to one of them, not centre.
     Block-opposite-directions policy: if both up AND down, prefer up
     (arbitrary but deterministic). */
  in = (joy_state_t){0};
  in.c1.up = 1;
  in.c1.down = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(nib == 0x0e, "up+down -> up (0x0E) per block-opposite policy");

  /* Fire primary only (cluster 1 K) */
  in = (joy_state_t){0};
  in.c1.fire = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(fire == 1, "cluster1 fire -> 1");

  /* Fire secondary only (cluster 2 X) -> still fire=1 (single logical fire) */
  in = (joy_state_t){0};
  in.c2.fire2 = 1;
  joystick_resolve(&in, &nib, &fire);
  CHECK(fire == 1, "cluster2 fire2 -> 1");

  uint8_t pia0 = 0x00;
  uint8_t trig[4] = {0, 0, 0, 0};
  joystick_apply_to_atari(&pia0, trig, 0x0b, 0);
  CHECK(pia0 == 0xfb, "apply writes joy1 low nibble and keeps joy2 idle");
  CHECK(trig[0] == 1, "apply releases joy1 trigger when fire is not pressed");
  CHECK(trig[1] == 1, "apply releases joy2 trigger by default");
  CHECK(trig[2] == 0 && trig[3] == 0, "apply leaves non-joystick trigger lines alone");

  joystick_apply_to_atari(&pia0, trig, 0x07, 1);
  CHECK(pia0 == 0xf7, "apply updates joy direction nibble");
  CHECK(trig[0] == 0, "apply presses joy1 trigger when fire is pressed");
  CHECK(trig[1] == 1, "joy2 trigger stays released even while joy1 fires");

  if (fail) return EXIT_FAILURE;
  printf("PASS: joystick\n");
  return EXIT_SUCCESS;
}
