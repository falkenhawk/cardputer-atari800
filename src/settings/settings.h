/* settings.h — in-memory settings for the running emulator.
   Persistence (writing these to /atari800/config/atari800.cfg) is M4. */

#ifndef CARDPUTER_SETTINGS_H
#define CARDPUTER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SETTINGS_MODEL_800XL = 0,
  SETTINGS_MODEL_65XE  = 1,
  SETTINGS_MODEL_130XE = 2,
  SETTINGS_MODEL_XEGS  = 3,
  SETTINGS_MODEL_COUNT
} settings_model_t;

typedef enum {
  SETTINGS_REGION_PAL  = 0,
  SETTINGS_REGION_NTSC = 1
} settings_region_t;

typedef struct {
  settings_model_t  model;
  settings_region_t region;
  int boot_basic;           /* 1 = boot with BASIC (default 1 for 65XE) */
  int dual_pokey;           /* 1 = true stereo (two POKEYs), 0 = mono->L+R */
  int input_mode_auto;      /* 1 = auto-detect from file ext, 0 = force */
  /* volume / brightness are held by the hardware subsystems; not in settings */
} settings_t;

/* Populate `s` with the spec's first-boot defaults. */
void settings_load_defaults(settings_t* s);

/* Apply `s` to the running atari800 core. Sequence:
     Atari800_SetMachineType(XLXE)
     MEMORY_ram_size = model's RAM
     Atari800_builtin_basic = s->boot_basic
     Atari800_builtin_game  = (model == XEGS)
     Atari800_SetTVMode(PAL|NTSC)
     Atari800_InitialiseMachine()
     Atari800_Coldstart()

   Caller is responsible for muting audio + pausing rendering around this
   call; it resets the machine. */
void settings_apply(const settings_t* s);

/* Pretty label for a model enum value. */
const char* settings_model_label(settings_model_t m);

#ifdef __cplusplus
}
#endif

#endif
