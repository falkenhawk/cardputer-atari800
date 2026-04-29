/* test_rom_browser_model.c - pure list/search helpers behind Fn+L browser. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ui/rom_browser_model.h"

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

static void test_compact_store_sort_and_search(void) {
  rom_browser_entry_t entries[4];
  char names[96];
  rom_browser_store_t store;
  rom_browser_store_init(&store, entries, 4, names, sizeof(names));

  CHECK(sizeof(rom_browser_entry_t) <= 4,
        "browser entry stays compact enough for large heap-backed lists");
  CHECK(rom_browser_store_add(&store, "zeta.atr", 0) == ROM_BROWSER_ADD_OK,
        "first file stored");
  CHECK(rom_browser_store_add(&store, "Beta", 1) == ROM_BROWSER_ADD_OK,
        "first dir stored");
  CHECK(rom_browser_store_add(&store, "alpha.xex", 0) == ROM_BROWSER_ADD_OK,
        "second file stored");
  CHECK(rom_browser_store_add(&store, "Games", 1) == ROM_BROWSER_ADD_OK,
        "second dir stored");

  rom_browser_store_sort(&store);

  CHECK(store.count == 4, "all entries retained after sort");
  CHECK(store.entries[0].is_dir && strcmp(rom_browser_store_name(&store, 0), "Beta") == 0,
        "directories sort before files, case-insensitive");
  CHECK(store.entries[1].is_dir && strcmp(rom_browser_store_name(&store, 1), "Games") == 0,
        "second directory sorted alphabetically");
  CHECK(!store.entries[2].is_dir && strcmp(rom_browser_store_name(&store, 2), "alpha.xex") == 0,
        "files sort after directories");
  CHECK(!store.entries[3].is_dir && strcmp(rom_browser_store_name(&store, 3), "zeta.atr") == 0,
        "last file sorted alphabetically");

  CHECK(rom_browser_find_prefix(&store, "ga") == 1,
        "prefix search is case-insensitive");
  CHECK(rom_browser_find_prefix_or_substring(&store, "lph") == 2,
        "substring fallback finds buried names when prefix misses");
  CHECK(rom_browser_find_prefix_or_substring(&store, "zz") == -1,
        "search miss returns -1");
}

static void test_caps_and_truncation(void) {
  rom_browser_entry_t entries[2];
  char names[80];
  rom_browser_store_t store;
  rom_browser_store_init(&store, entries, 2, names, sizeof(names));

  const char* long_name =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-extra.atr";
  CHECK(rom_browser_store_add(&store, long_name, 0) == ROM_BROWSER_ADD_OK,
        "long name can be stored");
  CHECK(strlen(rom_browser_store_name(&store, 0)) == ROM_BROWSER_ENTRY_NAME_MAX - 1,
        "long names are clipped to display-safe length");
  CHECK(rom_browser_store_add(&store, "second.atr", 0) == ROM_BROWSER_ADD_OK,
        "second entry fills count cap");
  CHECK(rom_browser_store_add(&store, "third.atr", 0) == ROM_BROWSER_ADD_ENTRY_FULL,
        "entry cap reports truncation");
  CHECK(store.truncated == 1 && store.count == 2,
        "entry cap leaves existing list intact and marks truncation");
}

static void test_name_pool_full_and_cursor_wrap(void) {
  rom_browser_entry_t entries[4];
  char names[8];
  rom_browser_store_t store;
  rom_browser_store_init(&store, entries, 4, names, sizeof(names));

  CHECK(rom_browser_store_add(&store, "abc", 0) == ROM_BROWSER_ADD_OK,
        "small pool accepts first name");
  CHECK(rom_browser_store_add(&store, "defg", 0) == ROM_BROWSER_ADD_NAME_FULL,
        "name pool cap reports truncation");
  CHECK(store.truncated == 1 && store.count == 1,
        "name pool failure leaves existing entry count intact");

  CHECK(rom_browser_cursor_wrap(-2, -1, 3) == 3,
        "cursor wraps above first item to last item");
  CHECK(rom_browser_cursor_wrap(4, -1, 3) == -1,
        "cursor wraps below last item to parent entry");
  CHECK(rom_browser_cursor_wrap(2, -1, 3) == 2,
        "cursor leaves in-range targets unchanged");
}

static void test_open_key_gate_ignores_launcher_chord_until_release(void) {
  rom_browser_key_gate_t gate;
  rom_browser_key_gate_opened(&gate);

  CHECK(rom_browser_key_gate_allows(&gate, 1) == 0,
        "browser open ignores the still-held Fn+L key");
  CHECK(rom_browser_key_gate_allows(&gate, 1) == 0,
        "held launcher key remains ignored across polls");
  CHECK(rom_browser_key_gate_allows(&gate, 0) == 0,
        "release disarms the open gate without firing a key");
  CHECK(rom_browser_key_gate_allows(&gate, 1) == 1,
        "next fresh press is accepted after release");
}

int main(void) {
  test_compact_store_sort_and_search();
  test_caps_and_truncation();
  test_name_pool_full_and_cursor_wrap();
  test_open_key_gate_ignores_launcher_chord_until_release();

  if (fail) return EXIT_FAILURE;
  printf("PASS: rom_browser_model\n");
  return EXIT_SUCCESS;
}
