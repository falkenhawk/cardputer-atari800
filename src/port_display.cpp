// port_display.cpp — implements port_present_frame as a call to renderer::present.
// Overrides the weak stub from port_impl.cpp at link time.

#include "../lib/atari800/port.h"
#include "display/renderer.h"

extern "C" void port_present_frame(const uint8_t* core_screen) {
  renderer::present(core_screen);
}
