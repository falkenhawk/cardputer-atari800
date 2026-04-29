/* rom_browser_model.c - compact list/search helpers for the Fn+L browser. */

#include "rom_browser_model.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void rom_browser_store_init(rom_browser_store_t* store,
                            rom_browser_entry_t* entries,
                            int max_entries,
                            char* names,
                            size_t names_capacity) {
  if (!store) return;
  store->entries = entries;
  store->max_entries = max_entries;
  store->names = names;
  store->names_capacity = names_capacity;
  store->count = 0;
  store->names_used = 0;
  store->truncated = 0;
}

rom_browser_add_result_t rom_browser_store_add(rom_browser_store_t* store,
                                               const char* name,
                                               int is_dir) {
  if (!store || !store->entries || !store->names || !name) {
    if (store) store->truncated = 1;
    return ROM_BROWSER_ADD_NAME_FULL;
  }
  if (store->count >= store->max_entries) {
    store->truncated = 1;
    return ROM_BROWSER_ADD_ENTRY_FULL;
  }

  size_t name_len = strlen(name);
  if (name_len > ROM_BROWSER_ENTRY_NAME_MAX - 1) {
    name_len = ROM_BROWSER_ENTRY_NAME_MAX - 1;
  }
  if (store->names_used + name_len + 1 > store->names_capacity) {
    store->truncated = 1;
    return ROM_BROWSER_ADD_NAME_FULL;
  }

  rom_browser_entry_t* entry = &store->entries[store->count++];
  entry->name_offset = (uint16_t)store->names_used;
  entry->is_dir = is_dir ? 1 : 0;
  memcpy(store->names + store->names_used, name, name_len);
  store->names[store->names_used + name_len] = '\0';
  store->names_used += name_len + 1;
  return ROM_BROWSER_ADD_OK;
}

const char* rom_browser_store_name(const rom_browser_store_t* store, int idx) {
  if (!store || !store->names || idx < 0 || idx >= store->count) return "";
  return store->names + store->entries[idx].name_offset;
}

static const rom_browser_store_t* s_sort_store = NULL;

static int compare_entries(const void* a, const void* b) {
  const rom_browser_entry_t* ea = (const rom_browser_entry_t*)a;
  const rom_browser_entry_t* eb = (const rom_browser_entry_t*)b;
  if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
  return strcasecmp(s_sort_store->names + ea->name_offset,
                    s_sort_store->names + eb->name_offset);
}

void rom_browser_store_sort(rom_browser_store_t* store) {
  if (!store || !store->entries || store->count <= 1) return;
  s_sort_store = store;
  qsort(store->entries, (size_t)store->count, sizeof(store->entries[0]),
        compare_entries);
  s_sort_store = NULL;
}

int rom_browser_find_prefix(const rom_browser_store_t* store, const char* query) {
  if (!store || !query || query[0] == '\0') return -1;
  size_t query_len = strlen(query);
  for (int i = 0; i < store->count; i++) {
    if (strncasecmp(rom_browser_store_name(store, i), query, query_len) == 0) {
      return i;
    }
  }
  return -1;
}

static int contains_ci(const char* haystack, const char* needle) {
  size_t needle_len = strlen(needle);
  if (needle_len == 0) return 1;
  for (const char* p = haystack; *p; p++) {
    size_t i = 0;
    while (i < needle_len && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == needle_len) return 1;
  }
  return 0;
}

int rom_browser_find_prefix_or_substring(const rom_browser_store_t* store,
                                         const char* query) {
  int idx = rom_browser_find_prefix(store, query);
  if (idx >= 0) return idx;
  if (!store || !query || query[0] == '\0') return -1;
  for (int i = 0; i < store->count; i++) {
    if (contains_ci(rom_browser_store_name(store, i), query)) return i;
  }
  return -1;
}

int rom_browser_cursor_wrap(int target, int min_cursor, int max_cursor) {
  if (max_cursor < min_cursor) return min_cursor;
  if (target < min_cursor) return max_cursor;
  if (target > max_cursor) return min_cursor;
  return target;
}

void rom_browser_key_gate_opened(rom_browser_key_gate_t* gate) {
  if (!gate) return;
  gate->ignore_until_release = 1;
}

int rom_browser_key_gate_allows(rom_browser_key_gate_t* gate, int pressed) {
  if (!gate || !gate->ignore_until_release) return pressed ? 1 : 0;
  if (pressed) return 0;
  gate->ignore_until_release = 0;
  return 0;
}
