/* port_impl.cpp — M2 minimum viable port layer.
   Implements timing + input/sound stubs so the core links.
   ROM loading is handled by upstream EMUOS_ALTIRRA path (sysrom.c picks up
   ROM_altirraos_xl / ROM_altirra_basic directly); our port_load_*_rom
   functions are declared for M4's user-ROM-override feature but return
   "not available" for M2. Display hook lives in port_display.cpp (T11). */

#include <Arduino.h>
#include "../../atari800/port.h"

extern "C" {

uint32_t port_millis(void) {
  return millis();
}

/* M4 will populate these to point at user-provided ROMs from
   /atari800/roms/{atarixl,ataribas}.rom on SD. For M2 the upstream Altirra
   ROMs (via EMUOS_ALTIRRA in config.h) are used directly by sysrom.c. */
int port_load_os_rom(const uint8_t** rom_out, size_t* len_out) {
  (void)rom_out; (void)len_out;
  return 0;   /* not available — sysrom.c falls back to built-in Altirra */
}

int port_load_basic_rom(const uint8_t** rom_out, size_t* len_out) {
  (void)rom_out; (void)len_out;
  return 0;   /* not available — same fallback */
}

/* Display hook is implemented strongly in port_display.cpp (T11).
   Defined weak here so linkage works even before T11 lands. */
__attribute__((weak))
void port_present_frame(const uint8_t* core_screen) {
  (void)core_screen;
  /* no-op until T11 */
}

/* Input stubs — M3 will replace. */
int port_get_key(void)       { return -1; }   /* AKEY_NONE */
int port_get_joy0(void)      { return 0xFF; } /* idle stick */
int port_get_joy_fire0(void) { return 0; }

/* Sound stubs — M3 will replace. */
void port_sound_init(int freq)                              { (void)freq; }
void port_sound_write(const int16_t* buf, size_t n_frames)  { (void)buf; (void)n_frames; }

/* Routing for upstream Log_print — T5 may need it if log.c patches land.
   Weak so if T5 prefers a different routing, it overrides cleanly. */
__attribute__((weak))
void port_log_write(const char* msg) {
  Serial.print(msg);
}

/* ----- platform.h stubs required by the atari800 core ----- */

/* PLATFORM_Initialise: called during Atari800_Initialise(). Return TRUE (1)
   to signal success. We do nothing here; hardware is already set up in setup(). */
int PLATFORM_Initialise(int *argc, char *argv[]) {
  (void)argc; (void)argv;
  return 1;  /* TRUE */
}

/* PLATFORM_Exit: called on graceful shutdown / panic. On embedded we just hang. */
int PLATFORM_Exit(int run_monitor) {
  (void)run_monitor;
  Serial.println("PLATFORM_Exit called — hanging");
  for (;;) { delay(1000); }
  return 1;
}

/* PLATFORM_Keyboard: return AKEY_NONE (no key) — M3 hooks the real keyboard. */
int PLATFORM_Keyboard(void) {
  return -1;  /* AKEY_NONE */
}

/* PLATFORM_DisplayScreen: called when core has a frame ready to display.
   T11 (port_display.cpp) provides the real implementation.
   Stub here so linkage works before T11 lands. */
__attribute__((weak))
void PLATFORM_DisplayScreen(void) {
  /* no-op until T11 */
}

/* Joystick/trigger — return idle. M3 will override. */
int PLATFORM_PORT(int num) { (void)num; return 0xFF; }
int PLATFORM_TRIG(int num) { (void)num; return 1; }

} /* extern "C" */
