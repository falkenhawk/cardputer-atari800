/* input_port.h — public surface of input_port.cpp */
#pragma once

extern "C" {
#include "keymap.h"
}

namespace input_port {

/* Call once per frame from main.cpp, before Atari800_Frame(). */
void poll();

/* Register a handler for firmware-level Fn actions (menu/reset/brightness/...).
   Called from within poll() on the same (main) thread. */
void set_action_handler(void (*fn)(km_action_t act));

} /* namespace input_port */
