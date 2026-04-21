/* cfg.c - cardputer-atari800 embedded build: CFG_LoadConfig / CFG_WriteConfig
   are no-ops. Upstream reads ~/.atari800.cfg or /etc/atari800.cfg via fopen;
   on embedded that's a NULL crash since there's no filesystem at those paths.
   We hardcode the machine profile and region at Atari800_Initialise time
   via argv; a real SD-backed config loader is an M3+ task. */

#include "atari.h"
#include "cfg.h"
#include <string.h>

int CFG_save_on_exit = FALSE;

int CFG_LoadConfig(const char *alternate_config_filename) {
    (void)alternate_config_filename;
    /* pretend we loaded a config successfully - the core's defaults apply. */
    return TRUE;
}

int CFG_WriteConfig(void) {
    return TRUE;   /* silently accept; nothing persisted. */
}

int CFG_MatchTextParameter(char const *param, char const * const cfg_strings[], int cfg_strings_size) {
    int i;
    for (i = 0; i < cfg_strings_size; i++) {
        if (strcasecmp(param, cfg_strings[i]) == 0)
            return i;
    }
    return -1;
}
