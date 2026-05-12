# ROM file browser — implementation notes & UX backlog

Living document. Captures the struggles we hit shipping `src/ui/rom_browser.{h,cpp}`
during the M3 work, and the open UX/architecture work that's still on the table.
Audience: codex (or whoever picks this up next).

## Architecture as shipped

- `src/ui/rom_browser.{h,cpp}` — full-screen overlay browser, owns LCD + keyboard
  while open. Hot-keyed by `Fn+L` (action `KM_ACT_LOAD_XEX`).
- Storage layout: **heap-backed string pool** (`rom_browser_entry_t { uint16_t
  name_offset; uint8_t is_dir; }` + shared name buffer). `main.cpp` allocates it
  after SD mount and before audio prealloc: primary 512 entries + 8 KB names,
  fallback 256 entries + 4 KB names.
- Loader dispatch via `AFILE_OpenFile()` (atari800 core) — magic-byte detection
  covers `.xex/.exe/.com`, `.atr/.xfd/.atx/.dcm/.pro`, `.car`, `.cas`. Raw `.rom`/`.bin`
  cartridges fail without a CARTHDR (no type-picker UI on embedded).
- Enumeration prefers ESP-IDF VFS `opendir/readdir` on `/sd/...`, with Arduino SD
  `File::openNextFile()` as a fallback.
- Sorting: `qsort` alphabetical (dirs first), comparator dereferences pool offsets.
- Loading indicator: 200 ms threshold before painting "loading…" overlay; counter
  updated every 50 entries.
- Type-ahead search: `g_search_buf[16]`, 1.5 s timeout, status bar shows live query
  in green; prefix search is attempted first, then substring fallback. Nav keys
  clear the buffer; Esc/Backspace are context-sensitive (clear search vs.
  close/parent-dir).

## Struggles

### 1. BSS layout vs. SD mount — the dominant constraint

ESP32-S3 has 320 KB internal SRAM total. The atari800 core init needs a sequential
chain of large allocations (`Screen_atari` 92 KB → `MEMORY_mem` 65 KB → `under_atarixl_os`
16 KB → two cart 8 KB) that totals ~173 KB and depends on the `heap@entry largest`
contiguous block being big enough to satisfy them sequentially. `CLAUDE.md` documents
this as the "memory-management pattern".

Adding browser BSS shrinks `heap@entry largest` byte-for-byte. Worse: BSS *placement*
matters too — splitting storage into two static blocks (struct array + name pool)
shifts where heap fragments form, fragmenting DMA-capable memory enough that SD's
`esp_vfs_fat_register` fails with `ESP_ERR_NO_MEM` (0x101).

Empirical thresholds (heap log per build):

| browser BSS | post-under-alloc DMA largest | SD mount |
|---|---|---|
| 65 KB (1024 naive) | xlos failed @ 14 KB largest | crash boot loop |
| 24 KB (1024 + 24 KB pool) | xlos failed | crash |
| 16 KB (1024 + 16 KB pool) | A0BF cart failed @ 22 KB | crash |
| 12 KB (1024 + 12 KB pool) | unders OK; SD largest=9.7 KB | SD fails |
| 9 KB (256 + 8 KB pool) | unders OK; SD largest=9.7 KB | SD fails |
| 5 KB (256 + 4 KB pool) | unders OK; SD largest=16 KB | **SD OK** |
| 2.3 KB (64 + 2 KB pool) | unders OK; SD largest=16 KB | **SD OK** |

**Practical budget:** browser static BSS should stay close to zero. t10bb moved
the entry array and name pool out of BSS and into post-SD heap allocation,
matching the successful console-speaker-buffer pattern from t10ba.

### 2. String-pool refactor was necessary but insufficient

Cut per-entry size from ~68 B (padded `name[64]+bool`) to 4 B (`uint16+uint8`).
Once the pool moved to post-SD heap allocation, the practical cap rose to 512
entries + 8 KB names with a 256 + 4 KB fallback.

### 3. M5Launcher's load-fast capability is misleading

Looking at Bruce launcher (`bmorcelli/Bruce`, the visible "launcher") shows it uses
`std::sort` + `std::vector<FileList>`, the same architecture as us. Difference is:

- Bruce loads from heap (no BSS pressure on atari core)
- Bruce doesn't run alongside a memory-hungry emulator
- Bruce's "fast" feeling is partly UX framing (entry-count visible during scan?)

We **probably can't beat Bruce's perceived speed** without giving up either the
sorted display OR atari800 memory headroom. Worth investigating POSIX `dirent`
(option 5 below) for raw enumeration speed, but the BSS budget remains binding.

### 4. Loading time for big dirs

~5-10 ms/entry on Arduino SD over SPI. At 256 entries = 1.3-2.6 s. User-visible
hesitation. Mitigated with the 200 ms-delayed loading indicator + counter, but
not eliminated.

### 5. Memory-management pattern is brittle

Multiple attempts at "small optimizations" (volume default change, key bindings,
even unrelated UX) ended up tipping over the SD mount because of how thin the
DMA-capable contiguous-heap budget is. Future contributors need to verify SD
still mounts after ANY change that shifts BSS or static initialization order.

## What works as shipped

- Fn+L opens browser; walks up from `/sd/atari800/roms` to deepest existing dir
- Hold-to-repeat (400 ms initial, 100 ms cadence) for ;/. line nav and ,// page nav
- Line navigation wraps between first/last entry
- Page Up/Down (`,`/`/`), Home/End (`[`/`]`)
- Type-ahead search with 1.5 s timeout, query shown in status bar; substring
  fallback after prefix misses
- Backspace + Esc are context-sensitive (search vs. close/parent)
- Focus-on-return: descending into `B/` then back up puts cursor on `B`
- Partial redraw (only changed rows) — no flicker on cursor moves
- CP437 arrows in help line (`↑↓←→`)
- Loading indicator with 200 ms delay threshold + per-50-entry counter; Esc can
  cancel a slow scan
- Footer shows loaded entry count and `*` when the list was truncated
- Fn+`-`/`=` adjusts volume from inside browser
- Fn+`[`/`]` adjusts brightness from inside browser
- Audio mutes while browser is open (POKEY state freezes; no stuck tones)

## UX backlog (priorities subjective)

### High-value, low-effort

1. **List wrapping** ✅ shipped in t10bb

2. **Visual toast for volume/brightness changes**
   - Currently silent; user just hears/sees the change without confirmation
   - Add brief overlay (e.g. "Volume 50%" for 1.5 s) in status-bar region

3. **Cancel during load** ✅ shipped in t10bb

4. **Show entry count somewhere visible** ✅ shipped in t10bb

5. **Filename truncation feedback**
   - Long names currently just clip at right edge
   - Either ellipsize (`Reall...long.atr`) or scroll-on-cursor (when cursor is on a
     truncated entry, scroll its full name horizontally)

### Medium effort, high value

6. **Recent files / favorites quick-load**
   - Top-of-list special section with last N opened ROMs
   - Persist in NVS or SD-side `.recent` file
   - Saves drilling down to the same paths repeatedly

7. **POSIX `dirent` enumeration** ✅ shipped in t10bb

8. **Persistent dir index cache (`.romidx`)**
   - Write sorted name list into `dirname/.romidx` on first scan
   - On reopen, if dir mtime ≤ cache mtime, read cache (sequential file read,
     near-instant)
   - Invalidate on mtime change
   - Slow first scan still happens, but every subsequent visit is fast
   - Adds SD writes; needs design for cache atomicity vs. dir writes

9. **Substring search vs. prefix-only** ✅ shipped in t10bb

10. **In-browser file details panel**
    - When cursor parks for >1 s, show file size + detected magic at bottom
    - Helps user pick the right ATR/XEX/CAR before loading

11. **Multi-disk support**
    - atari800 supports D1: through D8:; currently we always mount to D1 via
      `AFILE_OpenFile(..., diskno=1, ...)`
    - UI: while cursor on a `.atr`, allow `1`-`8` to mount to that slot
    - Useful for multi-disk games

12. **Sort options**
    - Currently always alphabetical (dirs first)
    - Date-modified or size sorts could help large dirs
    - Save preference per-dir via NVS

12a. **CAS loading — deferred (future milestone)**
    - Dispatch is already wired through `AFILE_OpenFile` →
      `CASSETTE_Insert` → `CASSETTE_hold_start = TRUE` → `Atari800_Coldstart()`
      (lib/atari800/src/afile.c:210), but not hardware-verified
    - Needed before declaring CAS supported: end-to-end test with a known-good
      `.cas`; a visible "tape loading" UI hint (real-time minutes per program);
      decision on whether to flip `SERIO_SOUND` for turbo-loader compatibility;
      keyboard mapping for cassette deck controls if any multi-load tapes are
      in scope
    - Postponed — re-enter scope only when a specific CAS use case lands

### High effort, architectural

13. **Lazy/streamed enumeration**
    - For dirs >1024 entries (the user's stated future need)
    - Breaks: alphabetical sort, Home/End, search-by-prefix, focus-on-return
    - Possible compromise: enumerate-and-sort in a FreeRTOS task, show a "loading
      page X of Y" while in progress, browse only the loaded prefix
    - ~150-200 lines + careful UX design

14. **Heap-allocated browser storage post-SD-mount** ✅ shipped in t10bb
   - Primary allocation is 512 entries + 8 KB names.
   - Fallback allocation is the old 256 entries + 4 KB names.

15. **Fast POKEY + hybrid console speaker mixer** ✅ shipped in `m3-t10ba-good`
    - `config.h` keeps `SYNCHRONIZED_SOUND`, `CONSOLE_SOUND`, and `SERIO_SOUND`
      disabled so POKEY-heavy games do not block the emulator frame/input path.
    - `audio_out.cpp` streams normal POKEY audio from `pokey_fast` in the I2S
      task.
    - `lib/atari800/src/gtia.c` queues CONSOL bit-3 speaker transitions with
      Atari CPU-clock timestamps.
    - `src/audio/audio_console_speaker.{c,h}` renders those transitions into a
      small caller-owned ring buffer allocated after SD mount, then mixes them
      over the fast POKEY stream.
    - The first attempt used a 4 KB static ring and broke SD mount by shifting
      BSS. The shipped version keeps the console mixer static footprint below
      768 B and allocates the ring after SD is mounted.
    - This is a compromise: BASIC key clicks/error bells are back without the
      full synchronized POKEYSND cost, but SIO/serial warble is still disabled
      until there is a separate lightweight path for it.

16. **ZIP archive support**
    - Some emulator collections ship ROMs in `.zip`
    - Would need a tiny decompressor (miniz?) — significant flash + RAM cost
    - Skip unless explicitly requested

## Constants worth knowing

```cpp
// src/ui/rom_browser.cpp
constexpr int      PRIMARY_MAX_ENTRIES     = 512;
constexpr size_t   PRIMARY_NAME_BUF_SIZE   = 8192;
constexpr int      FALLBACK_MAX_ENTRIES    = 256;
constexpr size_t   FALLBACK_NAME_BUF_SIZE  = 4096;
constexpr int      ENTRY_NAME_MAX          = 64;      // per-name truncation
constexpr int      VISIBLE_ROWS            = 9;       // LCD layout
constexpr uint32_t REPEAT_INITIAL_MS       = 400;
constexpr uint32_t REPEAT_INTERVAL_MS      = 100;
constexpr uint32_t LOAD_INDICATOR_DELAY_MS = 200;
constexpr int      LOAD_PROGRESS_STEP      = 50;
constexpr uint32_t SEARCH_TIMEOUT_MS       = 1500;
```

## Files to touch

- `src/ui/rom_browser.cpp` — core implementation
- `src/ui/rom_browser.h` — public API (open/close/is_open/poll)
- `src/ui/rom_browser_model.{c,h}` — host-testable list/search/sort helpers
- `src/main.cpp` — `Fn+L` action, main-loop browser-suspend logic
- `src/input/input_port.{c,h}` — coordinate keyboard scope when browser is active
- `lib/atari800/src/afile.c` — loader dispatch (don't modify; it works)

## Heap log incantation when debugging memory

When SD mount or atari core init regresses, the canonical diagnostic is the heap
log from boot. `setup()` already prints `heap@entry`, `heap@post-screen`, etc.
A new build's first comparison should be against:

- baseline `t10b-edge` (~RAM 51.3%, last known good with 256-naive layout)
- current shipped (~RAM 47.7%, t10o-passthru with 256+4K-pool string layout)

If `largest=` after `under-alloc` drops below 12 KB, expect SD trouble.
If `xlos=0x0` at `under-alloc`, you've broken the contiguous-heap chain and
will boot-loop on first `MEMORY_HandlePORTB` deref.
