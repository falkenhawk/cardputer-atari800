/* test_settings.c — settings struct defaults + validation.
   Core calls are mocked; we just verify sequencing + value passing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/settings/settings.h"

/* ---- Mocks for atari800 core ---- */
int Atari800_machine_type = 1; /* XLXE default */
int Atari800_tv_mode      = 312; /* PAL */
int MEMORY_ram_size       = 64;
int Atari800_builtin_basic = 1;
int Atari800_builtin_game  = 0;
int Atari800_keyboard_leds = 0;
int Atari800_f_keys        = 0;
int Atari800_jumper        = 0;
int Atari800_keyboard_detached = 0;

static int mock_set_machine_calls = 0;
static int mock_set_tv_calls = 0;
static int mock_init_machine_calls = 0;
static int mock_coldstart_calls = 0;

void Atari800_SetMachineType(int t) { Atari800_machine_type = t; mock_set_machine_calls++; }
void Atari800_SetTVMode(int m)      { Atari800_tv_mode = m; mock_set_tv_calls++; }
int  Atari800_InitialiseMachine(void) { mock_init_machine_calls++; return 1; }
void Atari800_Coldstart(void)       { mock_coldstart_calls++; }

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  settings_t s;
  settings_load_defaults(&s);

  /* Defaults per spec 1.1 / 1.3: 65XE + PAL + BASIC on */
  CHECK(s.model  == SETTINGS_MODEL_65XE, "default model 65XE");
  CHECK(s.region == SETTINGS_REGION_PAL, "default region PAL");
  CHECK(s.boot_basic == 1,               "default basic on");
  CHECK(s.dual_pokey == 0,               "default dual_pokey off");
  CHECK(s.input_mode_auto == 1,          "default input_mode auto");

  /* Apply 130XE + NTSC */
  mock_set_machine_calls = mock_set_tv_calls = mock_init_machine_calls = mock_coldstart_calls = 0;
  s.model = SETTINGS_MODEL_130XE;
  s.region = SETTINGS_REGION_NTSC;
  settings_apply(&s);

  CHECK(Atari800_machine_type == 1 /* XLXE */,  "130XE -> XLXE enum");
  CHECK(MEMORY_ram_size == 128,                  "130XE -> 128 KB");
  CHECK(Atari800_tv_mode == 262 /* NTSC */,      "region NTSC -> 262");
  CHECK(mock_init_machine_calls == 1,            "InitialiseMachine called once");
  CHECK(mock_coldstart_calls == 1,               "Coldstart called after Init");

  /* XEGS: XLXE + 64 KB + builtin_game */
  settings_load_defaults(&s);
  s.model = SETTINGS_MODEL_XEGS;
  settings_apply(&s);
  CHECK(Atari800_builtin_game == 1, "XEGS -> builtin_game=1");
  CHECK(MEMORY_ram_size == 64,      "XEGS -> 64 KB");

  /* 800XL: XLXE + 64 KB + no builtin_game */
  settings_load_defaults(&s);
  s.model = SETTINGS_MODEL_800XL;
  settings_apply(&s);
  CHECK(Atari800_builtin_game == 0, "800XL -> builtin_game=0");

  if (fail) return EXIT_FAILURE;
  printf("PASS: settings\n");
  return EXIT_SUCCESS;
}
