/* rom_args.h - atari800 argv construction for optional SD ROM overrides. */

#ifndef CARDPUTER_ROM_ARGS_H
#define CARDPUTER_ROM_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif

#define ROM_ARGS_SD_OS_ROM       "/atari800/bios/ATARIXL.ROM"
#define ROM_ARGS_SD_BASIC_ROM    "/atari800/bios/ATARIBAS.ROM"
#define ROM_ARGS_VFS_OS_ROM      "/sd/atari800/bios/ATARIXL.ROM"
#define ROM_ARGS_VFS_BASIC_ROM   "/sd/atari800/bios/ATARIBAS.ROM"
#define ROM_ARGS_SD_OS_ROM_LC    "/atari800/bios/atarixl.rom"
#define ROM_ARGS_SD_BASIC_ROM_LC "/atari800/bios/ataribas.rom"
#define ROM_ARGS_VFS_OS_ROM_LC   "/sd/atari800/bios/atarixl.rom"
#define ROM_ARGS_VFS_BASIC_ROM_LC "/sd/atari800/bios/ataribas.rom"
#define ROM_ARGS_OS_ROM_BYTES    16384
#define ROM_ARGS_BASIC_ROM_BYTES 8192
#define ROM_ARGS_MAX             6

int rom_args_build(char** argv, int capacity, const char* os_rom_vfs_path, const char* basic_rom_vfs_path);

#ifdef __cplusplus
}
#endif

#endif /* CARDPUTER_ROM_ARGS_H */
