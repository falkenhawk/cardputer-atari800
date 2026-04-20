/* port.h — cardputer-atari800 port shim declarations.
   The atari800 core calls these when it needs to reach hardware or the host
   environment. Implementations live in lib/atari800_port/src/. */

#ifndef CARDPUTER_ATARI800_PORT_H
#define CARDPUTER_ATARI800_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- timing ---- */
uint32_t port_millis(void);

/* ---- ROM loading ---- */
int port_load_os_rom(const uint8_t** rom_out, size_t* len_out);
int port_load_basic_rom(const uint8_t** rom_out, size_t* len_out);

/* ---- display ---- */
void port_present_frame(const uint8_t* core_screen);

/* ---- input stubs (M3 will replace) ---- */
int port_get_key(void);
int port_get_joy0(void);
int port_get_joy_fire0(void);

/* ---- sound stubs (M3 will replace) ---- */
void port_sound_init(int freq);
void port_sound_write(const int16_t* buf, size_t n_frames);

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_ATARI800_PORT_H */
