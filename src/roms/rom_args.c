#include "rom_args.h"

int rom_args_build(char** argv, int capacity, const char* os_rom_vfs_path, const char* basic_rom_vfs_path) {
  int argc = 0;
  if (!argv || capacity < 2) return 0;

  argv[argc++] = (char*)"atari800";
  argv[argc++] = (char*)"-basic";

  if (os_rom_vfs_path && argc + 2 <= capacity) {
    argv[argc++] = (char*)"-xlxe_rom";
    argv[argc++] = (char*)os_rom_vfs_path;
  }

  if (basic_rom_vfs_path && argc + 2 <= capacity) {
    argv[argc++] = (char*)"-basic_rom";
    argv[argc++] = (char*)basic_rom_vfs_path;
  }

  return argc;
}
