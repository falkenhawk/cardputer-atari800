/* test_rom_args.c - atari800 argv construction for SD ROM overrides. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/roms/rom_args.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

static int arg_is(char** argv, int index, const char* expected) {
  return strcmp(argv[index], expected) == 0;
}

int main(void) {
  char* argv[ROM_ARGS_MAX];

  int argc = rom_args_build(argv, ROM_ARGS_MAX, NULL, NULL);
  CHECK(argc == 2, "fallback argc");
  CHECK(arg_is(argv, 0, "atari800"), "fallback argv[0]");
  CHECK(arg_is(argv, 1, "-basic"), "fallback keeps BASIC enabled");

  argc = rom_args_build(argv, ROM_ARGS_MAX, ROM_ARGS_VFS_OS_ROM, NULL);
  CHECK(argc == 4, "OS override argc");
  CHECK(arg_is(argv, 2, "-xlxe_rom"), "OS override switch");
  CHECK(arg_is(argv, 3, ROM_ARGS_VFS_OS_ROM), "OS override VFS path");

  argc = rom_args_build(argv, ROM_ARGS_MAX, NULL, ROM_ARGS_VFS_BASIC_ROM);
  CHECK(argc == 4, "BASIC override argc");
  CHECK(arg_is(argv, 2, "-basic_rom"), "BASIC override switch");
  CHECK(arg_is(argv, 3, ROM_ARGS_VFS_BASIC_ROM), "BASIC override VFS path");

  argc = rom_args_build(argv, ROM_ARGS_MAX, ROM_ARGS_VFS_OS_ROM, ROM_ARGS_VFS_BASIC_ROM);
  CHECK(argc == 6, "both overrides argc");
  CHECK(arg_is(argv, 2, "-xlxe_rom"), "both OS switch");
  CHECK(arg_is(argv, 3, ROM_ARGS_VFS_OS_ROM), "both OS path");
  CHECK(arg_is(argv, 4, "-basic_rom"), "both BASIC switch");
  CHECK(arg_is(argv, 5, ROM_ARGS_VFS_BASIC_ROM), "both BASIC path");

  argc = rom_args_build(argv, ROM_ARGS_MAX, ROM_ARGS_VFS_OS_ROM_LC, ROM_ARGS_VFS_BASIC_ROM_LC);
  CHECK(argc == 6, "lowercase BIOS filenames argc");
  CHECK(arg_is(argv, 3, ROM_ARGS_VFS_OS_ROM_LC), "lowercase OS path");
  CHECK(arg_is(argv, 5, ROM_ARGS_VFS_BASIC_ROM_LC), "lowercase BASIC path");

  if (fail) return EXIT_FAILURE;
  printf("PASS: rom_args\n");
  return EXIT_SUCCESS;
}
