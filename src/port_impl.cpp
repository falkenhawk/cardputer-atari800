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
   extern "C" so function prototypes match our definitions below.
   pokey.h + cpu.h give us POKEY_KBCODE / POKEY_IRQEN / POKEY_IRQST /
   POKEY_SKSTAT / CPU_GenerateIRQ() used by our INPUT_Frame() pump below. */
#include "../../atari800/src/screen.h"
#include "../../atari800/src/pokey.h"
#include "../../atari800/src/cpu.h"
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
   is not defined. On Cardputer-Adv there is no PSRAM, and by the time this
   runs M5Cardputer.begin() has eaten ~200 KB of the 320 KB DRAM heap leaving
   only ~65 KB — not enough for the 96 KB buffer. main.cpp therefore pre-
   allocates Screen_atari at the very top of setup() before anything else
   touches the heap. We just reuse that buffer here. */
int Screen_Initialise(int *argc, char *argv[]) {
  (void)argc; (void)argv;
  if (Screen_atari) {
    // buffer was pre-allocated in main.cpp setup() before heap was eaten
    // by M5Cardputer.begin(). Nothing to do.
    return 1;
  }
  // fallback path: try to allocate here. Likely to fail on Cardputer-Adv
  // because by now the heap is mostly gone. main.cpp should always
  // pre-allocate, so reaching this is unexpected.
  constexpr size_t buf_bytes = 384 * (240 + 16);
  Screen_atari = (ULONG*) ps_malloc(buf_bytes);
  if (!Screen_atari) {
    Serial.println("Screen_Initialise: ps_malloc failed, falling back to heap");
    Screen_atari = (ULONG*) malloc(buf_bytes);
  }
  if (Screen_atari) memset(Screen_atari, 0, buf_bytes);
  if (!Screen_atari) {
    Serial.println("Screen_Initialise: CRITICAL - heap too depleted for 96 KB alloc");
  }
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

/* INPUT_Frame — once-per-Atari-frame keyboard pump.
   Copies INPUT_key_code (set by input_port::poll() before Atari800_Frame)
   into POKEY_KBCODE and raises the POKEY kbd IRQ on key-press transitions.
   This is the minimal slice of upstream input.c we need; upstream does it
   per-scanline but per-frame is sufficient for typing latency (20 ms ok).

   Edge-triggered: IRQ fires only when INPUT_key_code CHANGES, so holding a
   key doesn't hammer BASIC's key-repeat logic at 50 Hz. */
void INPUT_Frame(void) {
  static int last_code = -1;
  if (INPUT_key_code >= 0) {
    if (INPUT_key_code != last_code) {
      POKEY_KBCODE = (UBYTE)(INPUT_key_code & 0xFF);
      POKEY_SKSTAT &= ~0x04;           /* bit 2 active-low: "last key still down" */
      if (POKEY_IRQEN & 0x40) {
        POKEY_IRQST &= ~0x40;          /* assert kbd IRQ (bit 6 active-low) */
        CPU_GenerateIRQ();
      }
    }
  } else {
    POKEY_SKSTAT |= 0x04;              /* no key down — clear the "key pressed" flag */
  }
  last_code = INPUT_key_code;
}

void INPUT_Scanline(void)                       {}

/* Raise the POKEY BREAK IRQ — same shape as the kbd IRQ in INPUT_Frame but
   bit 7 of IRQST/IRQEN. Called from main.cpp's on_input_action() when the
   user presses Fn+7. Vendored core has NO consumer of AKEY_BREAK (grep
   confirms — upstream input.c would handle it but it's not vendored), so
   we raise the IRQ directly the same way a real Atari BREAK key does. */
void port_raise_break_irq(void) {
  if (POKEY_IRQEN & 0x80) {
    POKEY_IRQST &= ~0x80;
    CPU_GenerateIRQ();
  }
}
void INPUT_SelectMultiJoy(int no)               { (void)no; }
void INPUT_CenterMousePointer(void)             {}
void INPUT_DrawMousePointer(void)               {}
int  INPUT_Recording(void)                      { return 0; }
int  INPUT_Playingback(void)                    { return 0; }
void INPUT_RecordInt(int i)                     { (void)i; }
int  INPUT_PlaybackInt(void)                    { return 0; }

/* ----- sound.h stubs — real I2S pump lives in src/audio/audio_out.cpp (T10).
   Weak Sound_Update so T10 can strong-override. Pause/Continue are no-ops
   here; T10 may extend. */

int  Sound_enabled = 1;

int Sound_Initialise(int* argc, char* argv[]) {
  (void)argc; (void)argv;
  Serial.println("Sound_Initialise (stub — real init in audio_out.cpp when T10 lands)");
  return 1;
}
void Sound_Exit(void) {}

__attribute__((weak))
void Sound_Update(void) {
  /* Overridden by audio_out.cpp in T10. Weak so linkage stays clean while
     T10 is in progress. */
}

void Sound_Pause(void)    {}
void Sound_Continue(void) {}

} /* extern "C" */
