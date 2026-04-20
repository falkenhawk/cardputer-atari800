/* config.h — hand-written for the cardputer-atari800 embedded build.
   Replaces the auto-generated config.h from upstream's autotools build. */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_NAME "atari800"
#define PACKAGE_VERSION "5.2.0-cardputer"

/* Emulated machines we enable */
#define BASIC                 /* built-in BASIC cartridge support */

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
#define HAVE_TIME_H 1

/* Embedded: no file I/O beyond what we wrap */
#undef HAVE_DIRENT_H
#undef HAVE_UNISTD_H
#undef HAVE_OPENDIR
#undef HAVE_GETTIMEOFDAY

#endif /* CONFIG_H */
