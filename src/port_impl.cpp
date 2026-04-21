/* port_impl.cpp — M2 minimum viable port layer.
   Implements timing + input/sound stubs so the core links.
   ROM loading is handled by upstream EMUOS_ALTIRRA path (sysrom.c picks up
   ROM_altirraos_xl / ROM_altirra_basic directly); our port_load_*_rom
   functions are declared for M4's user-ROM-override feature but return
   "not available" for M2. Display hook lives in port_display.cpp (T11).

   Screen_atari: the upstream screen.c (not vendored) would normally define
   this. With BASIC removed from config.h ANTIC needs the buffer; we define
   it here. Screen_WIDTH=384, Screen_HEIGHT=240, +16 extra ANTIC lines per
   upstream convention. Declared as ULONG* for pointer-arithmetic alignment
   in antic.c, but the actual content is one palette-index byte per pixel. */

#include <Arduino.h>
#include "../../atari800/port.h"

extern "C" {
/* screen.h declares ULONG and Screen_atari with C linkage — include inside
   extern "C" so function prototypes match our definitions below. */
#include "../../atari800/src/screen.h"
} // close extern "C" temporarily — reopen below for the rest of the port

/* Screen_atari — 384 × (240+16) byte pixel buffer, ULONG-aligned.
   Upstream screen.c (not vendored) would define this; we provide it here.
   Screen_WIDTH=384, Screen_HEIGHT=240, +16 extra ANTIC lines.
   Allocated from PSRAM in Screen_Initialise to keep DRAM free. */
ULONG *Screen_atari = nullptr;

/* Visible-area extents (screen.h extern declarations). */
int Screen_visible_x1 = 24;
int Screen_visible_y1 = 0;
int Screen_visible_x2 = 360;
int Screen_visible_y2 = 240;

/* Display-stats overlay flags — unused in M2. */
int Screen_show_atari_speed      = 0;
int Screen_show_disk_led         = 0;
int Screen_show_sector_counter   = 0;
int Screen_show_1200_leds        = 0;
int Screen_show_multimedia_stats = 0;

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

/* screen.h — Screen_Initialise is called from Atari800_Initialise when BASIC
   is not defined. Allocate the Screen_atari buffer from PSRAM so we don't
   blow the 320 KB DRAM. Size: 384×(240+16) bytes. */
int Screen_Initialise(int *argc, char *argv[]) {
  (void)argc; (void)argv;
  constexpr size_t buf_bytes = 384 * (240 + 16);
  Screen_atari = (ULONG*) ps_malloc(buf_bytes);
  if (!Screen_atari) {
    Serial.println("Screen_Initialise: ps_malloc failed, falling back to heap");
    Screen_atari = (ULONG*) malloc(buf_bytes);
  }
  if (Screen_atari) memset(Screen_atari, 0, buf_bytes);
  return Screen_atari ? 1 : 0;
}

void Screen_DrawAtariSpeed(double unused)  { (void)unused; }
void Screen_DrawDiskLED(void)              {}
void Screen_Draw1200LED(void)              {}
void Screen_DrawMultimediaStats(void)      {}
void Screen_EntireDirty(void)              {}

/* ui.h — no on-screen menu in M2. */
int  UI_Initialise(int *argc, char *argv[])  { (void)argc; (void)argv; return 1; }
void UI_Run(void)                            {}
int  UI_SelectCartType(int k)                { (void)k; return 0; }
int  UI_is_active                            = 0;

/* colours.h — palette handled by our display/palette module; core colours
   init is a no-op. */
void Colours_PreInitialise(void)                        {}
int  Colours_Initialise(int *argc, char *argv[])        { (void)argc; (void)argv; return 1; }

/* artifact.h — no PAL/NTSC artifact emulation in M2. */
int  ARTIFACT_Initialise(int *argc, char *argv[])       { (void)argc; (void)argv; return 1; }

/* statesav.h — no state save/load in M2. */
int  StateSav_ReadAtariState(const char *fn, const char *mode) {
  (void)fn; (void)mode; return 0; /* not found */
}

/* input.h — all input stubs; M3 will replace. */
int  INPUT_key_code  = -1;   /* AKEY_NONE */
int  INPUT_key_shift = 0;
int  INPUT_key_consol = 0x07; /* INPUT_CONSOL_NONE */
int  INPUT_joy_autofire[4]                   = {0, 0, 0, 0};
int  INPUT_joy_block_opposite_directions     = 1;
int  INPUT_joy_multijoy                      = 0;
int  INPUT_joy_5200_min                      = 6;
int  INPUT_joy_5200_center                   = 114;
int  INPUT_joy_5200_max                      = 220;
int  INPUT_mouse_mode                        = 0; /* INPUT_MOUSE_OFF */
int  INPUT_mouse_port                        = 0;
int  INPUT_mouse_delta_x                     = 0;
int  INPUT_mouse_delta_y                     = 0;
int  INPUT_mouse_buttons                     = 0;
int  INPUT_mouse_speed                       = 3;
int  INPUT_mouse_pot_min                     = 1;
int  INPUT_mouse_pot_max                     = 228;
int  INPUT_mouse_pen_ofs_h                   = 0;
int  INPUT_mouse_pen_ofs_v                   = 0;
int  INPUT_mouse_joy_inertia                 = 10;
int  INPUT_direct_mouse                      = 0;
int  INPUT_cx85                              = 0;

int  INPUT_Initialise(int *argc, char *argv[])  { (void)argc; (void)argv; return 1; }
void INPUT_Exit(void)                           {}
void INPUT_Frame(void)                          {}
void INPUT_Scanline(void)                       {}
void INPUT_SelectMultiJoy(int no)               { (void)no; }
void INPUT_CenterMousePointer(void)             {}
void INPUT_DrawMousePointer(void)               {}
int  INPUT_Recording(void)                      { return 0; }
int  INPUT_Playingback(void)                    { return 0; }
void INPUT_RecordInt(int i)                     { (void)i; }
int  INPUT_PlaybackInt(void)                    { return 0; }

} /* extern "C" */
