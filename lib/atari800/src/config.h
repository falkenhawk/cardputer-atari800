/* config.h — hand-written for the cardputer-atari800 embedded build.
   Replaces the auto-generated config.h from upstream's autotools build. */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "atari800"
#define PACKAGE_VERSION "5.2.0-cardputer"

/* Emulated machines we enable */
/* BASIC: enabling this switches Atari800_Frame() to basic_frame(), which skips
   ANTIC rendering entirely and leaves Screen_atari unpopulated. Removed for
   M2 so ANTIC runs and the READY screen is visible on the LCD. */
/* #define BASIC */

/* use upstream's built-in Altirra ROM replacements.
   enables sysrom.c's EMUOS_ALTIRRA path, which auto-includes
   roms/altirraos_*.{c,h} + altirra_basic.{c,h} + altirra_5200_os.{c,h}
   and treats them as the default-available ROMs. */
#define EMUOS_ALTIRRA 1

/* Use page-based MEMORY_attrib instead of a flat 65 KB array.
   Replaces the 64 KB RAM/ROM/HARDWARE map with 256 page pointers
   (~1 KB) + per-page dispatch. Saves ~63 KB of heap pressure —
   the ADV has only 205 KB contiguous DRAM after M1/M2 static data. */
#define PAGED_ATTRIB 1

/* Sound: enabled as of M3/T9. POKEY_Update macro guard in pokey.c:181-183
   flips to the real POKEYSND_Update_ptr dispatch; pokeysnd.c (already
   compiled in M2 via srcFilter +<*>) now actually receives register writes.
   STEREO_SOUND must stay ON so a runtime dual-POKEY toggle can emit separate
   left/right samples when POKEYSND_num_pokeys is set to 2.
   SOUND_THIN_API stays OFF — we don't use the thin-API path (it would need
   a PLATFORM_SoundSetup we don't have). */
#define SOUND
#ifndef STEREO_SOUND
#define STEREO_SOUND 1
#endif
/* The firmware uses src/audio/pokey_fast.c for runtime audio. The upstream
   synchronized renderer is accurate enough to carry console clicks/bells, but
   it can block Atari800_Frame() on POKEY-heavy games. Keep it disabled for the
   no-PSRAM Cardputer runtime path. */
#undef  SERIO_SOUND
#undef  CONSOLE_SOUND
#undef  VOL_ONLY_SOUND
#undef  SYNCHRONIZED_SOUND
#undef  SOUND_THIN_API
#undef NETSIO
#undef MONITOR_BREAK
#undef MONITOR_BREAKPOINTS
#undef MONITOR_HINTS
#undef VOICEBOX
#undef XEP80_EMULATION
#undef AF80
#undef BIT3
#undef PBI_MIO
#undef PBI_BB
#undef PBI_XLD
#undef PBI_PROTO80

/* Target characteristics */
#define WORDS_BIGENDIAN 0     /* ESP32 is little-endian */
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1       /* ESP-IDF newlib provides strings.h */
#define HAVE_STRCASECMP 1      /* ESP-IDF newlib provides strcasecmp — prevents Util_stricmp fallback to nonexistent `stricmp` */
#define HAVE_TIME_H 1

/* Embedded: no file I/O beyond what we wrap */
#undef HAVE_DIRENT_H
#undef HAVE_UNISTD_H
#undef HAVE_OPENDIR
#undef HAVE_GETTIMEOFDAY
#undef HAVE_FCNTL_H
#undef HAVE_SYS_STAT_H

/* ESP32 newlib provides clock() — use it for Util_time() */
#define HAVE_CLOCK 1

#endif /* CONFIG_H */
