/* rom_browser.cpp — see header for design notes.

   Memory: entries are stored in a fixed-cap pool to avoid heap pressure on
   the tight ESP32-S3 internal DRAM. A 128-entry cap × 64-char names = 8 KB
   stack-allocated as a static — plenty for typical ROM directories. */

#include "rom_browser.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../audio/audio_out.h"

extern "C" {
#include "../input/mode.h"
/* afile.c — magic-byte-detected loader. Returns AFILE_<type> on success,
   AFILE_ERROR (0) on failure. With reboot=1 it cold-resets the machine. */
int AFILE_OpenFile(const char* filename, int reboot, int diskno, int readonly);
}

namespace rom_browser {

namespace {

/* String-pool layout: Entry stores a 16-bit offset into shared g_name_buf
   instead of inline name[64]. Per-entry shrinks from ~68 B to 4 B.
   See CLAUDE.md "memory-management pattern" for the contiguous-heap chain.

   On cap selection — empirically, raising cap above 256 with a string
   pool sized for 1024 entries still triggers SD-mount failure even
   though total BSS is unchanged. Root cause is BSS *placement* fragmenting
   the heap differently than the inline-name layout — splitting storage
   into two static blocks (struct array + name pool) shifts where heap
   gaps land, and SD's FATFS init can no longer find enough small chunks.
   Until lazy enumeration replaces full-load (allowing zero-pool design),
   keep the cap conservative. */
/* Empirical browser-BSS budget: each KB of static BSS here costs ~1 KB
   of DMA-capable contiguous heap at post-under-alloc time, and SD's
   esp_vfs_fat_register needs the largest DMA-cap chunk to be ≥ ~10 KB
   to mount. Verified by t10l shrunk-test (2.3 KB BSS → 16 KB largest →
   SD OK) vs t10j (9 KB BSS → 9.7 KB largest → SD fails ESP_ERR_NO_MEM).
   Budget: ~6 KB BSS. Sized 256 entries × 4 B struct = 1 KB + 4 KB name
   pool = 5 KB total — leaves 1 KB safety margin.
   Average name budget: 4096 / 256 = 16 chars per name. Typical ROM
   names (Lasermania.atr=14, atari_basic.rom=15, doom.bin=8) fit. */
constexpr int      MAX_ENTRIES             = 256;
constexpr int      NAME_BUF_SIZE           = 4096;
/* Loading indicator only appears for slow enumerations. Fast dir
   switches (a few hundred ms) stay flicker-free — the user sees the
   previous list until the new one is ready. Past LOAD_INDICATOR_DELAY,
   we paint a "loading…" overlay and tick the count every N entries. */
constexpr uint32_t LOAD_INDICATOR_DELAY_MS = 200;
constexpr int      LOAD_PROGRESS_STEP      = 50;
/* Repeat timing for held-key navigation. 400 ms before the first repeat
   keeps single taps from accidentally firing twice; 100 ms between
   repeats is fast enough to flick through 256 entries in ~25 s but slow
   enough that you can stop on a target. */
constexpr uint32_t REPEAT_INITIAL_MS  = 400;
constexpr uint32_t REPEAT_INTERVAL_MS = 100;
constexpr int   ENTRY_NAME_MAX      = 64;
constexpr int   PATH_MAX_BUF  = 192;

/* Layout (240 × 135 LCD at default font, 6×8 px):
   row 0      : current directory (path)
   row 1..9   : 9 visible entries
   row 10..11 : status / help */
constexpr int   ROW_H         = 12;       /* 12 px between rows */
constexpr int   HEADER_Y      = 2;
constexpr int   LIST_Y0       = HEADER_Y + ROW_H + 2;
constexpr int   VISIBLE_ROWS  = 9;
constexpr int   STATUS_Y      = LIST_Y0 + VISIBLE_ROWS * ROW_H + 2;

/* String-pool entry: name lives in g_name_buf[name_offset..]. The
   name_offset uses uint16_t which limits the pool to 64 KB; we use
   24 KB so that's safe. The struct is 4 B (uint16 + bool + 1 byte
   padding) → 1024 entries fit in 4 KB BSS. */
struct Entry {
  uint16_t name_offset;
  bool     is_dir;
};

bool g_open    = false;
bool g_dirty   = true;        /* needs redraw */
bool g_pre_muted = false;     /* mute state to restore on close */

/* Two path namespaces in play:
   - Arduino SD library (SD.open, File.openNextFile) takes paths RELATIVE
     to the SD mount point — root is "/", so "/atari800/roms".
   - stdio fopen + atari800 core (AFILE_OpenFile) take full VFS paths,
     so "/sd/atari800/roms".
   We store and display the SD-relative form; prepend "/sd" only when
   handing a path to the loader. */
char g_dir[PATH_MAX_BUF]  = "/atari800/roms";     /* SD-relative current dir */
char g_last_dir[PATH_MAX_BUF] = "";               /* persists across opens */
char g_status[ENTRY_NAME_MAX] = "";               /* one-line status (errors) */

Entry  g_entries[MAX_ENTRIES];
char   g_name_buf[NAME_BUF_SIZE];
size_t g_name_buf_used = 0;
int    g_count    = 0;
int    g_cursor   = 0;

/* Look up an entry's filename through the string pool. */
inline const char* entry_name(int idx) {
  return g_name_buf + g_entries[idx].name_offset;
}
inline const char* entry_name(const Entry& e) {
  return g_name_buf + e.name_offset;
}

/* Set whenever the *contents* (g_count, g_dir, status text, truncation
   flag) change such that header/footer/list-area all need a fresh draw.
   A bare cursor-only move leaves this false, enabling the 2-row swap. */
bool g_force_full = true;

/* Type-ahead search. While buffer non-empty AND not timed out, the
   status bar shows "search: <buf>" and the next typed char extends the
   buffer; cursor jumps to first entry whose name (case-insensitively)
   starts with the buffer. Buffer auto-replaces on next keystroke after
   the timeout, so quick re-search just works. */
constexpr uint32_t SEARCH_TIMEOUT_MS = 1500;
char     g_search_buf[16] = "";
size_t   g_search_len     = 0;
uint32_t g_search_last_ms = 0;

/* Forward decls — search helpers are defined after the rendering section
   (so they can use set_cursor) but draw_full needs to query
   search_active() to decide whether to show the search prefix in the
   status bar. */
bool search_active(uint32_t now);
int   g_scroll   = 0;
bool  g_truncated = false;

/* ---------- path helpers ---------- */

bool dir_exists(const char* path) {
  File f = SD.open(path);
  if (!f) return false;
  bool is_dir = f.isDirectory();
  f.close();
  return is_dir;
}

/* Strip the trailing "/component" from path. Returns false if already at
   "/" (SD root). Modifies path in place. */
bool path_pop(char* path) {
  if (strcmp(path, "/") == 0) return false;
  char* slash = strrchr(path, '/');
  if (!slash) return false;
  if (slash == path) {
    /* Path is "/something" — going up means we land at "/". */
    path[1] = '\0';
  } else {
    *slash = '\0';
  }
  return true;
}

/* Walk up from initial until a directory exists. Always succeeds because
   "/" (SD root) is valid by assumption (SD is mounted). */
void resolve_initial_dir(char* out, const char* preferred) {
  strncpy(out, preferred, PATH_MAX_BUF - 1);
  out[PATH_MAX_BUF - 1] = '\0';
  while (!dir_exists(out)) {
    if (!path_pop(out)) {
      strcpy(out, "/");
      break;
    }
  }
}

/* ---------- extension filter ---------- */

bool ends_with_ci(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  if (lf > ls) return false;
  const char* a = s + ls - lf;
  for (size_t i = 0; i < lf; i++) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)suffix[i])) return false;
  }
  return true;
}

bool is_supported_file(const char* name) {
  static const char* exts[] = {
    ".xex", ".exe", ".com", ".bas", ".lst",
    ".atr", ".xfd", ".atx", ".dcm", ".pro",
    ".car", ".rom", ".bin",
    ".cas",
    nullptr,
  };
  for (int i = 0; exts[i]; i++) {
    if (ends_with_ci(name, exts[i])) return true;
  }
  return false;
}

/* ---------- enumeration ---------- */

int compare_entries(const void* a, const void* b) {
  const Entry* ea = (const Entry*)a;
  const Entry* eb = (const Entry*)b;
  if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
  /* qsort swaps Entry structs, but the name pool is never moved,
     so the offsets remain valid before and after swaps. */
  return strcasecmp(g_name_buf + ea->name_offset,
                    g_name_buf + eb->name_offset);
}

/* Paint the browser shell (header + path + separator) plus a centered
   "loading…" overlay. Used when enumeration runs past the indicator
   delay so the user has visual feedback that work is happening.
   Not called for fast dirs — those go directly old-list → new-list. */
void paint_loading_shell() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextSize(1);
  d.setCursor(2, HEADER_Y);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  size_t plen = strlen(g_dir);
  if (plen > 38) {
    d.print("...");
    d.print(g_dir + (plen - 35));
  } else {
    d.print(g_dir);
  }
  d.drawFastHLine(0, LIST_Y0 - 2, 240, TFT_DARKGREY);
}

void paint_load_progress(int count) {
  auto& d = M5Cardputer.Display;
  int y = LIST_Y0 + (VISIBLE_ROWS / 2) * ROW_H;
  d.fillRect(0, y - 1, 240, ROW_H, TFT_BLACK);
  d.setTextColor(TFT_YELLOW, TFT_BLACK);
  /* Center-ish: "loading 250…" is ~12 chars × 6 px = ~72 px wide,
     screen is 240 px → start at x=84. */
  d.setCursor(84, y);
  d.printf("loading %d...", count);
}

void load_entries() {
  g_count = 0;
  g_truncated = false;
  g_cursor = 0;
  g_scroll = 0;
  g_name_buf_used = 0;   /* implicit "free" of all previous names */
  g_force_full = true;

  File root = SD.open(g_dir);
  if (!root || !root.isDirectory()) {
    snprintf(g_status, sizeof(g_status), "open failed: %s", g_dir);
    if (root) root.close();
    return;
  }

  uint32_t load_start_ms = millis();
  bool indicator_shown = false;

  while (g_count < MAX_ENTRIES) {
    /* Arm the indicator on the first iteration past the threshold.
       Checked at the loop top so empty/fast dirs never paint it. */
    uint32_t now = millis();
    if (!indicator_shown && (now - load_start_ms) >= LOAD_INDICATOR_DELAY_MS) {
      paint_loading_shell();
      paint_load_progress(g_count);
      indicator_shown = true;
    }

    File f = root.openNextFile();
    if (!f) break;

    /* Arduino SD's f.name() returns a leaf name on ESP32 (no leading slash);
       skip dotfiles and macOS metadata cruft. */
    const char* leaf = f.name();
    /* Some ESP32 SD versions still prefix with the dir path — strip it. */
    const char* slash = strrchr(leaf, '/');
    if (slash) leaf = slash + 1;

    if (leaf[0] == '.' || leaf[0] == '_') {
      f.close();
      continue;
    }

    bool is_dir = f.isDirectory();
    if (!is_dir && !is_supported_file(leaf)) {
      f.close();
      continue;
    }

    /* Append leaf + null to the name pool. If the pool would overflow,
       stop loading (truncate) — caller sees the same g_truncated flag
       it would for a count-cap hit, and the existing UX surfaces it. */
    size_t leaf_len = strlen(leaf);
    if (leaf_len > ENTRY_NAME_MAX - 1) leaf_len = ENTRY_NAME_MAX - 1;
    if (g_name_buf_used + leaf_len + 1 > NAME_BUF_SIZE) {
      g_truncated = true;
      f.close();
      break;
    }
    Entry& e = g_entries[g_count++];
    e.name_offset = (uint16_t)g_name_buf_used;
    e.is_dir = is_dir;
    memcpy(g_name_buf + g_name_buf_used, leaf, leaf_len);
    g_name_buf[g_name_buf_used + leaf_len] = '\0';
    g_name_buf_used += leaf_len + 1;
    f.close();

    if (indicator_shown && (g_count % LOAD_PROGRESS_STEP) == 0) {
      paint_load_progress(g_count);
    }
  }

  /* Detect truncation: peek one more. */
  if (g_count == MAX_ENTRIES) {
    File f = root.openNextFile();
    if (f) {
      g_truncated = true;
      f.close();
    }
  }
  root.close();

  qsort(g_entries, g_count, sizeof(Entry), compare_entries);
  Serial.printf("rom_browser: %d entries%s\n",
                g_count, g_truncated ? " (truncated)" : "");
}

/* ---------- rendering ----------

   Two redraw modes:
   - draw_full(): repaints header, all visible rows, separator, status.
     Used on dir change, scroll change, status-message change, first open.
   - draw_cursor_change(prev, curr): repaints just the two affected rows.
     Used on every plain cursor move within the visible window.

   g_prev_cursor / g_prev_scroll track what was last drawn so poll() can
   pick the cheapest path. -2 sentinel forces a full redraw on first
   open(). */

int  g_prev_cursor = -2;
int  g_prev_scroll = -1;

int row_of_idx(int idx) {
  /* Returns the on-screen row (0..VISIBLE_ROWS-1) for entry index `idx`,
     or -1 if it's not currently visible. idx == -1 represents the ".."
     virtual entry, only visible at the top of root-dir's first page. */
  bool show_parent = (strcmp(g_dir, "/") != 0);
  int top_offset = (show_parent && g_scroll == 0) ? 1 : 0;
  if (idx == -1) {
    return (show_parent && g_scroll == 0) ? 0 : -1;
  }
  if (idx < g_scroll) return -1;
  int row = top_offset + (idx - g_scroll);
  if (row >= VISIBLE_ROWS) return -1;
  return row;
}

void draw_row(int row, int idx, bool selected) {
  auto& d = M5Cardputer.Display;
  int y = LIST_Y0 + row * ROW_H;
  uint16_t fg, bg;
  if (idx == -1) {
    fg = selected ? TFT_BLACK : TFT_YELLOW;
    bg = selected ? TFT_YELLOW : TFT_BLACK;
    d.fillRect(0, y - 1, 240, ROW_H, bg);
    d.setTextColor(fg, bg);
    d.setCursor(2, y);
    d.print(" .. ");
  } else if (idx >= 0 && idx < g_count) {
    const Entry& e = g_entries[idx];
    fg = selected ? TFT_BLACK : (e.is_dir ? TFT_CYAN : TFT_WHITE);
    bg = selected ? TFT_WHITE : TFT_BLACK;
    d.fillRect(0, y - 1, 240, ROW_H, bg);
    d.setTextColor(fg, bg);
    d.setCursor(2, y);
    d.print(e.is_dir ? "/" : " ");
    d.print(entry_name(e));
  } else {
    d.fillRect(0, y - 1, 240, ROW_H, TFT_BLACK);
  }
}

void draw_list_area() {
  bool show_parent = (strcmp(g_dir, "/") != 0);
  int row = 0;
  if (show_parent && g_scroll == 0) {
    draw_row(row, -1, g_cursor == -1);
    row++;
  }
  for (; row < VISIBLE_ROWS; row++) {
    int idx = g_scroll + (row - (show_parent && g_scroll == 0 ? 1 : 0));
    draw_row(row, idx, idx == g_cursor && idx >= 0 && idx < g_count);
  }
}

void draw_full() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextSize(1);

  /* Header — current SD-relative path. Truncate left if too long. */
  d.setCursor(2, HEADER_Y);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  size_t plen = strlen(g_dir);
  if (plen > 38) {
    d.print("...");
    d.print(g_dir + (plen - 35));
  } else {
    d.print(g_dir);
  }

  d.drawFastHLine(0, LIST_Y0 - 2, 240, TFT_DARKGREY);

  draw_list_area();

  d.drawFastHLine(0, STATUS_Y - 2, 240, TFT_DARKGREY);
  d.setCursor(2, STATUS_Y);
  uint32_t now = millis();
  if (g_status[0]) {
    d.setTextColor(TFT_RED, TFT_BLACK);
    d.print(g_status);
  } else if (search_active(now)) {
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.print("search: ");
    d.print(g_search_buf);
  } else {
    /* CP437 0x18..0x1B = ↑↓→← (IBM PC font glyphs, supported by
       LGFX/TFT_eSPI's default 6×8 font). If they ever render as
       boxes on this hardware we'll switch to UTF-8 ↑↓←→. */
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.print("\x18\x19 line  \x1B\x1A page  []hm/end \\up Ent Esc");
    if (g_truncated) {
      d.setTextColor(TFT_YELLOW, TFT_BLACK);
      d.print(" *");
    }
  }
  g_dirty = false;
  g_force_full = false;
  g_prev_cursor = g_cursor;
  g_prev_scroll = g_scroll;
}

void draw_cursor_change(int prev, int curr) {
  int prev_row = row_of_idx(prev);
  int curr_row = row_of_idx(curr);
  if (prev_row >= 0) draw_row(prev_row, prev, false);
  if (curr_row >= 0) draw_row(curr_row, curr, true);
  g_prev_cursor = curr;
}

/* ---------- navigation ----------

   set_cursor(target) clamps `target` to valid range and adjusts g_scroll
   so the new cursor is visible. All move-by-N operations (line, page,
   home, end) go through this. Caller doesn't trigger redraw — poll()
   compares g_cursor/g_scroll to g_prev_* and picks full vs. partial. */

void set_cursor(int target) {
  bool show_parent = (strcmp(g_dir, "/") != 0);
  int min_cursor = show_parent ? -1 : 0;
  int max_cursor = g_count - 1;
  if (max_cursor < min_cursor) max_cursor = min_cursor;  /* empty dir edge */
  if (target < min_cursor) target = min_cursor;
  if (target > max_cursor) target = max_cursor;
  g_cursor = target;

  /* Bring g_cursor into the visible window. ".." occupies row 0 when
     visible (g_scroll == 0); otherwise rows are entries g_scroll..+VR. */
  int file_rows = VISIBLE_ROWS - ((show_parent && g_scroll == 0) ? 1 : 0);

  if (g_cursor == -1) {
    g_scroll = 0;
  } else if (g_cursor < g_scroll) {
    g_scroll = g_cursor;
  } else if (g_cursor >= g_scroll + file_rows) {
    g_scroll = g_cursor - file_rows + 1;
    if (g_scroll < 0) g_scroll = 0;
  }
  if (g_scroll > g_count - 1) g_scroll = (g_count > 0) ? g_count - 1 : 0;
  if (g_scroll < 0) g_scroll = 0;
}

void cursor_up()        { set_cursor(g_cursor - 1); }
void cursor_down()      { set_cursor(g_cursor + 1); }
void cursor_pageup()    { set_cursor(g_cursor - (VISIBLE_ROWS - 1)); }
void cursor_pagedown()  { set_cursor(g_cursor + (VISIBLE_ROWS - 1)); }
void cursor_home()      {
  bool show_parent = (strcmp(g_dir, "/") != 0);
  set_cursor(show_parent ? -1 : 0);
}
void cursor_end()       { set_cursor(g_count - 1); }

void enter_parent() {
  /* Save the leaf we're leaving so we can put the cursor back on it
     in the parent view. e.g. /atari800/roms/B → "B" so user returns
     to a familiar landmark instead of jumping to the top of /roms. */
  char leaving[ENTRY_NAME_MAX] = "";
  const char* slash = strrchr(g_dir, '/');
  if (slash && slash[1] != '\0') {
    strncpy(leaving, slash + 1, ENTRY_NAME_MAX - 1);
    leaving[ENTRY_NAME_MAX - 1] = '\0';
  }

  if (!path_pop(g_dir)) return;
  g_status[0] = '\0';
  load_entries();   /* resets g_cursor=0, sets g_force_full */

  /* Find the directory we just left and focus it. */
  if (leaving[0]) {
    for (int i = 0; i < g_count; i++) {
      if (g_entries[i].is_dir && strcasecmp(entry_name(i), leaving) == 0) {
        set_cursor(i);
        break;
      }
    }
  }
}

void enter_selection() {
  /* Cursor == -1 means ".." virtual entry */
  if (g_cursor == -1) {
    enter_parent();
    return;
  }
  if (g_cursor < 0 || g_cursor >= g_count) return;

  const Entry& e = g_entries[g_cursor];

  if (e.is_dir) {
    /* Append "/<name>" to g_dir. Special-case root "/" so we don't end
       up with "//name". */
    const char* leaf = entry_name(e);
    char joined[PATH_MAX_BUF];
    int n = (strcmp(g_dir, "/") == 0)
              ? snprintf(joined, sizeof(joined), "/%s", leaf)
              : snprintf(joined, sizeof(joined), "%s/%s", g_dir, leaf);
    if (n < 0 || n >= (int)sizeof(joined)) {
      snprintf(g_status, sizeof(g_status), "path too long");
      g_force_full = true;
      return;
    }
    strcpy(g_dir, joined);
    g_status[0] = '\0';
    load_entries();
    return;
  }

  /* It's a file. Build full VFS path (prepend "/sd") and hand off. */
  const char* leaf = entry_name(e);
  char full[PATH_MAX_BUF];
  int n = (strcmp(g_dir, "/") == 0)
            ? snprintf(full, sizeof(full), "/sd/%s", leaf)
            : snprintf(full, sizeof(full), "/sd%s/%s", g_dir, leaf);
  if (n < 0 || n >= (int)sizeof(full)) {
    snprintf(g_status, sizeof(g_status), "path too long");
    g_force_full = true;
    return;
  }
  Serial.printf("rom_browser: AFILE_OpenFile(\"%s\")\n", full);

  /* Persist last-used dir BEFORE the loader, in case the loader resets us. */
  strncpy(g_last_dir, g_dir, PATH_MAX_BUF - 1);
  g_last_dir[PATH_MAX_BUF - 1] = '\0';

  int rc = AFILE_OpenFile(full, /*reboot=*/1, /*diskno=*/1, /*readonly=*/1);
  Serial.printf("rom_browser: AFILE_OpenFile rc=%d\n", rc);

  if (rc == 0) {
    snprintf(g_status, sizeof(g_status), "load failed: %s", leaf);
    g_force_full = true;
    return;
  }

  /* Auto-pick input mode based on extension: .xex/.car/.bin → joystick,
     .atr/.bas → keyboard. mode_autodetect_for accepts any path-like string. */
  mode_autodetect_for(leaf);
  Serial.printf("input: mode = %s\n",
                mode_current() == MODE_JOYSTICK ? "joystick" : "keyboard");

  /* Success — close browser, resume Atari. */
  close();
}

/* ---------- key handling ---------- */

/* ---------- type-ahead search ---------- */

bool search_active(uint32_t now) {
  return g_search_len > 0 && (now - g_search_last_ms) <= SEARCH_TIMEOUT_MS;
}

bool is_search_char(char c) {
  unsigned char u = (unsigned char)c;
  if (u < 0x20 || u > 0x7E) return false;            /* non-printable */
  /* Reject keys that have a navigation meaning. Everything else
     (letters, digits, space, _ - ! etc.) becomes search input. */
  switch (c) {
    case ';': case '.': case ',': case '/':
    case '[': case ']': case '\\':
      return false;
    default:
      return true;
  }
}

void search_append(char c) {
  uint32_t now = millis();
  if (!search_active(now)) g_search_len = 0;   /* timeout → reset */

  if (g_search_len < sizeof(g_search_buf) - 1) {
    g_search_buf[g_search_len++] = c;
    g_search_buf[g_search_len] = '\0';
  }
  g_search_last_ms = now;

  /* Find first entry (case-insensitive) whose name starts with buffer.
     Skip the ".." virtual entry — search is for files/subdirs. */
  for (int i = 0; i < g_count; i++) {
    if (strncasecmp(entry_name(i), g_search_buf, g_search_len) == 0) {
      set_cursor(i);
      break;
    }
  }
  g_force_full = true;   /* status bar shows "search: <buf>" */
}

void search_clear() {
  if (g_search_len == 0) return;
  g_search_len = 0;
  g_search_buf[0] = '\0';
  g_force_full = true;
}

/* Returns true if this key participates in hold-to-repeat (line + page
   nav only — Enter/Esc/parent/Home/End must NOT auto-repeat. Home/End
   are one-shot: repeating them just sits at the boundary doing nothing
   visible, and accidentally holding either feels broken). */
bool is_repeating_key(char c, uint8_t hid_key) {
  if (hid_key == 0x52 || hid_key == 0x51 || hid_key == 0x50 || hid_key == 0x4F)
    return true;  /* HID arrow keys */
  switch (c) {
    case ';': case '.': case ',': case '/':
      return true;
    default:
      return false;
  }
}

void handle_key(char c, uint8_t hid_key, bool ctrl, bool fn, bool alt) {
  (void)ctrl; (void)alt;

  /* Fn-modified chords pass through to firmware actions (volume +
     brightness) so the user can adjust them while the browser is open.
     These don't conflict with the browser's plain-key bindings because
     fn=true is required to take this branch. */
  if (fn) {
    switch (c) {
      case '-':
        audio_out::set_volume_delta(-16);
        Serial.println("rom_browser: volume down");
        return;
      case '=':
        audio_out::set_volume_delta(+16);
        Serial.println("rom_browser: volume up");
        return;
      case '[': {
        uint8_t b = M5Cardputer.Display.getBrightness();
        if (b > 16) M5Cardputer.Display.setBrightness(b - 16);
        Serial.printf("rom_browser: brightness=%u\n", M5Cardputer.Display.getBrightness());
        return;
      }
      case ']': {
        uint8_t b = M5Cardputer.Display.getBrightness();
        if (b <= 255 - 16) M5Cardputer.Display.setBrightness(b + 16);
        Serial.printf("rom_browser: brightness=%u\n", M5Cardputer.Display.getBrightness());
        return;
      }
      default: break;
    }
  }

  /* Esc and Backspace are CONTEXT-SENSITIVE while search is live:
     - Esc with search active → exit search mode (don't close browser)
     - Backspace with search active → delete last char (don't go up)
     This matches typical file-manager UX. Without active search they
     do their normal thing. */
  uint32_t now = millis();
  bool searching = search_active(now);

  if (hid_key == 0x29) {                                             /* ESC */
    if (searching) { search_clear(); return; }
    close();
    return;
  }
  if (hid_key == 0x2A || c == 0x08) {                                /* Backspace */
    if (searching) {
      if (g_search_len > 0) {
        g_search_len--;
        g_search_buf[g_search_len] = '\0';
        g_search_last_ms = millis();
        /* Re-find first match for shorter prefix; if buffer became
           empty, leave cursor where it is. */
        if (g_search_len > 0) {
          for (int i = 0; i < g_count; i++) {
            if (strncasecmp(entry_name(i), g_search_buf, g_search_len) == 0) {
              set_cursor(i);
              break;
            }
          }
        }
        g_force_full = true;
      }
      return;
    }
    enter_parent();
    return;
  }

  /* Other navigation keys clear search and act as usual. */
  if (hid_key == 0x52) { search_clear(); cursor_up();      return; } /* HID Up */
  if (hid_key == 0x51) { search_clear(); cursor_down();    return; } /* HID Down */
  if (hid_key == 0x50) { search_clear(); cursor_pageup();  return; } /* HID Left  → PageUp */
  if (hid_key == 0x4F) { search_clear(); cursor_pagedown();return; } /* HID Right → PageDown */
  if (hid_key == 0x28 || c == '\r' || c == '\n') {
    search_clear(); enter_selection(); return;                       /* Enter */
  }

  /* Printable-character bindings: nav keys first, then anything else
     (letters/digits/punct except nav) feeds into type-ahead search. */
  switch (c) {
    case ';':  search_clear(); cursor_up();        return;
    case '.':  search_clear(); cursor_down();      return;
    case ',':  search_clear(); cursor_pageup();    return;
    case '/':  search_clear(); cursor_pagedown();  return;
    case '[':  search_clear(); cursor_home();      return;
    case ']':  search_clear(); cursor_end();       return;
    case '\\': search_clear(); enter_parent();     return;
    case 27:   search_clear(); close();            return;     /* literal ESC */
    default:   break;
  }

  if (is_search_char(c)) search_append(c);
}

} /* anonymous namespace */

/* ---------- public API ---------- */

void open() {
  if (g_open) return;
  g_open = true;
  g_status[0] = '\0';

  /* Mute audio while browser is up — POKEY state is frozen but the audio
     task keeps running, so any active wave would become a stuck tone. */
  g_pre_muted = false; /* TODO: query mute state when audio_out exposes a getter */
  audio_out::set_muted(true);

  /* Resolve directory: prefer last-used; else walk up from default. */
  if (g_last_dir[0] && dir_exists(g_last_dir)) {
    strncpy(g_dir, g_last_dir, PATH_MAX_BUF - 1);
    g_dir[PATH_MAX_BUF - 1] = '\0';
  } else {
    resolve_initial_dir(g_dir, "/atari800/roms");
  }
  Serial.printf("rom_browser: open dir=%s\n", g_dir);

  load_entries();
  draw_full();   /* always full on first open */
}

void close() {
  if (!g_open) return;
  /* Remember where we left off. */
  strncpy(g_last_dir, g_dir, PATH_MAX_BUF - 1);
  g_last_dir[PATH_MAX_BUF - 1] = '\0';

  g_open = false;
  audio_out::set_muted(g_pre_muted);
}

bool is_open() { return g_open; }

void poll() {
  if (!g_open) return;

  /* M5Cardputer.Keyboard.isChange() is consumed-on-read (Keyboard.cpp:66
     mutates _last_key_size), so we can't share it with main.cpp's input
     pipeline. Track press edges locally: snapshot keysState every poll,
     compare to last seen, fire handle_key on rising edge AND on
     hold-with-elapsed-timer for navigation keys. */
  static char     held_c   = 0;
  static uint8_t  held_hid = 0;
  static uint32_t next_fire_ms = 0;

  auto& status = M5Cardputer.Keyboard.keysState();
  bool pressed = !status.word.empty() || !status.hid_keys.empty();
  char c = status.word.empty() ? 0 : status.word[0];
  uint8_t hid = status.hid_keys.empty() ? 0 : status.hid_keys[0];
  uint32_t now = millis();

  if (!pressed) {
    held_c = 0;
    held_hid = 0;
    next_fire_ms = 0;
  } else if (c != held_c || hid != held_hid) {
    /* Rising edge or different key now held — fire once, set repeat
       timer if eligible. */
    handle_key(c, hid, status.ctrl, status.fn, status.alt);
    held_c = c;
    held_hid = hid;
    next_fire_ms = is_repeating_key(c, hid) ? (now + REPEAT_INITIAL_MS) : 0;
  } else if (next_fire_ms != 0 && now >= next_fire_ms) {
    /* Same nav key still held past the delay — fire again. */
    handle_key(c, hid, status.ctrl, status.fn, status.alt);
    next_fire_ms = now + REPEAT_INTERVAL_MS;
  }

  /* Search-timeout transition: when buffer goes stale, the help-line
     should revert from "search: …" to the key reference. Detect the
     first poll() after timeout elapsed and force a status-bar redraw. */
  static bool prev_search_active = false;
  bool now_search_active = search_active(now);
  if (prev_search_active && !now_search_active) {
    g_search_len = 0;
    g_search_buf[0] = '\0';
    g_force_full = true;
  }
  prev_search_active = now_search_active;

  /* Redraw routing — pick cheapest path based on what changed. */
  if (g_force_full) {
    draw_full();
  } else if (g_scroll != g_prev_scroll) {
    /* Scroll changed — repaint the list area (no fillScreen, header
       and status stay put). */
    draw_list_area();
    g_prev_cursor = g_cursor;
    g_prev_scroll = g_scroll;
  } else if (g_cursor != g_prev_cursor) {
    draw_cursor_change(g_prev_cursor, g_cursor);
  }
}

} /* namespace rom_browser */
