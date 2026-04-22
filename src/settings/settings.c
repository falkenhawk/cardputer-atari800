#include "settings.h"

extern int Atari800_machine_type;
extern int Atari800_tv_mode;
extern int MEMORY_ram_size;
extern int Atari800_builtin_basic;
extern int Atari800_builtin_game;
extern int Atari800_keyboard_leds;
extern int Atari800_f_keys;
extern int Atari800_jumper;
extern int Atari800_keyboard_detached;

extern void Atari800_SetMachineType(int);
extern void Atari800_SetTVMode(int);
extern int  Atari800_InitialiseMachine(void);
extern void Atari800_Coldstart(void);

#define ATARI800_MACHINE_XLXE  1
#define ATARI800_TV_PAL        312
#define ATARI800_TV_NTSC       262

void settings_load_defaults(settings_t* s) {
  s->model           = SETTINGS_MODEL_65XE;
  s->region          = SETTINGS_REGION_PAL;
  s->boot_basic      = 1;
  s->dual_pokey      = 0;
  s->input_mode_auto = 1;
}

void settings_apply(const settings_t* s) {
  /* XL/XE family tuple per atari.c:488-556.
     Only the fields that differ across our 4 models need to be set. */
  int ram_size     = 64;
  int builtin_game = 0;
  switch (s->model) {
    case SETTINGS_MODEL_800XL: ram_size = 64;  builtin_game = 0; break;
    case SETTINGS_MODEL_65XE:  ram_size = 64;  builtin_game = 0; break;
    case SETTINGS_MODEL_130XE: ram_size = 128; builtin_game = 0; break;
    case SETTINGS_MODEL_XEGS:  ram_size = 64;  builtin_game = 1; break;
    default: break;
  }

  Atari800_SetMachineType(ATARI800_MACHINE_XLXE);
  MEMORY_ram_size            = ram_size;
  Atari800_builtin_basic     = s->boot_basic ? 1 : 0;
  Atari800_builtin_game      = builtin_game;
  Atari800_keyboard_leds     = 0;    /* 1200XL-only; always 0 for our models */
  Atari800_f_keys            = 0;    /* 1200XL-only */
  Atari800_jumper            = 0;
  Atari800_keyboard_detached = 0;

  Atari800_SetTVMode(s->region == SETTINGS_REGION_PAL ? ATARI800_TV_PAL
                                                      : ATARI800_TV_NTSC);

  Atari800_InitialiseMachine();
  Atari800_Coldstart();
}

const char* settings_model_label(settings_model_t m) {
  switch (m) {
    case SETTINGS_MODEL_800XL: return "800XL";
    case SETTINGS_MODEL_65XE:  return "65XE";
    case SETTINGS_MODEL_130XE: return "130XE";
    case SETTINGS_MODEL_XEGS:  return "XEGS";
    default: return "?";
  }
}
