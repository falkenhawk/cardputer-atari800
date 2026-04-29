/* rom_browser_model.h - compact list/search helpers for the Fn+L browser. */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROM_BROWSER_ENTRY_NAME_MAX 64

typedef struct {
  uint16_t name_offset;
  uint8_t  is_dir;
} rom_browser_entry_t;

typedef struct {
  rom_browser_entry_t* entries;
  int                  max_entries;
  char*                names;
  size_t               names_capacity;
  int                  count;
  size_t               names_used;
  int                  truncated;
} rom_browser_store_t;

typedef struct {
  uint8_t ignore_until_release;
} rom_browser_key_gate_t;

typedef enum {
  ROM_BROWSER_ADD_OK = 0,
  ROM_BROWSER_ADD_ENTRY_FULL = 1,
  ROM_BROWSER_ADD_NAME_FULL = 2,
} rom_browser_add_result_t;

void rom_browser_store_init(rom_browser_store_t* store,
                            rom_browser_entry_t* entries,
                            int max_entries,
                            char* names,
                            size_t names_capacity);

rom_browser_add_result_t rom_browser_store_add(rom_browser_store_t* store,
                                               const char* name,
                                               int is_dir);

const char* rom_browser_store_name(const rom_browser_store_t* store, int idx);
void rom_browser_store_sort(rom_browser_store_t* store);

int rom_browser_find_prefix(const rom_browser_store_t* store, const char* query);
int rom_browser_find_prefix_or_substring(const rom_browser_store_t* store,
                                         const char* query);

int rom_browser_cursor_wrap(int target, int min_cursor, int max_cursor);

void rom_browser_key_gate_opened(rom_browser_key_gate_t* gate);
int rom_browser_key_gate_allows(rom_browser_key_gate_t* gate, int pressed);

#ifdef __cplusplus
}
#endif
