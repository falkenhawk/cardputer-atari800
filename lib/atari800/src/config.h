/* config.h — hand-written for the cardputer-atari800 embedded build.
   Replaces the auto-generated config.h from upstream's autotools build. */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "atari800"
#define PACKAGE_VERSION "5.2.0-cardputer"

/* Emulated machines we enable */
#define BASIC                 /* built-in BASIC cartridge support */

/* use upstream's built-in Altirra ROM replacements.
   enables sysrom.c's EMUOS_ALTIRRA path, which auto-includes
   roms/altirraos_*.{c,h} + altirra_basic.{c,h} + altirra_5200_os.{c,h}
   and treats them as the default-available ROMs. */
#define EMUOS_ALTIRRA 1

/* Features we disable for embedded build */
#undef SOUND                  /* M3 will enable */
#undef STEREO_SOUND
#undef SERIO_SOUND
#undef CONSOLE_SOUND
#undef SYNCHRONIZED_SOUND
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
