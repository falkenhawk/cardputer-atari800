// cardputer-atari800 — entry point
// Milestone 2: core init + 50 Hz frame loop → LCD

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <errno.h>
#include <esp_heap_caps.h>

#include "display/lcd.h"
#include "display/renderer.h"
#include "input/input_port.h"

extern "C" {
#include "input/mode.h"
}

extern "C" {
#include "../lib/atari800/src/atari.h"
#include "../lib/atari800/port.h"
#include "../lib/atari800/src/screen.h"
#include "../lib/atari800/src/memory.h"
}


extern "C" void ensure_memory_mem_allocated(void);
extern "C" void ensure_under_buffers_allocated(void);
extern "C" void* debug_get_under_atarixl_os(void);
extern "C" void* debug_get_under_cart809F(void);
extern "C" void* debug_get_under_cartA0BF(void);
extern "C" int BINLOAD_Loader(const char* filename);

/* atari800 core reset entry points (for Fn+4 / Fn+5 via action handler).
   Declared here rather than via atari.h to keep main.cpp's include set
   tight — atari.h pulls in a wide transitive graph. */
extern "C" {
void Atari800_Warmstart(void);
void Atari800_Coldstart(void);
void port_raise_break_irq(void);   /* defined in port_impl.cpp */
}

static void dump_shadow_ptrs(const char* tag) {
  Serial.printf("ptrs@%s: MEMORY_mem=%p under_xlos=%p under_809F=%p under_A0BF=%p\n",
                tag,
                (void*)MEMORY_mem,
                debug_get_under_atarixl_os(),
                debug_get_under_cart809F(),
                debug_get_under_cartA0BF());
  Serial.flush();
}

static const char* mode_name(renderer::Mode m) {
  switch (m) {
    case renderer::Mode::PixelPerfect: return "Pixel-perfect";
    case renderer::Mode::Pillarbox:    return "Pillarbox";
    case renderer::Mode::Cover:        return "Cover";
    case renderer::Mode::Stretch:      return "Stretch";
  }
  return "?";
}

/* Forward decl — try_load_xex is defined below (its SD/BINLOAD_Loader body
   references SD/SPI globals that are set up later in this file). */
static bool try_load_xex(const char* path);

/* Fn-layer firmware action handler. Registered with input_port at setup();
   called from within input_port::poll() when a Fn+<key> chord resolves to a
   KM_OUT_ACTION. Audio (volume), menu overlay, save/load-state are wired in
   later tasks (T10/T15/M5); brightness + display mode + reset/break are live. */
static void on_input_action(km_action_t act) {
  switch (act) {
    case KM_ACT_DISPLAY_MODE_CYCLE: {
      renderer::Mode next = static_cast<renderer::Mode>(
        (static_cast<int>(renderer::get_mode()) + 1) % 4);
      renderer::set_mode(next);
      Serial.printf("display: %s\n", mode_name(next));
      break;
    }
    case KM_ACT_BRIGHTNESS_DOWN: {
      uint8_t b = M5Cardputer.Display.getBrightness();
      if (b > 16) M5Cardputer.Display.setBrightness(b - 16);
      Serial.printf("brightness: %u\n", M5Cardputer.Display.getBrightness());
      break;
    }
    case KM_ACT_BRIGHTNESS_UP: {
      uint8_t b = M5Cardputer.Display.getBrightness();
      if (b <= 255 - 16) M5Cardputer.Display.setBrightness(b + 16);
      Serial.printf("brightness: %u\n", M5Cardputer.Display.getBrightness());
      break;
    }
    case KM_ACT_WARM_RESET:
      Serial.println("action: warm reset");
      Atari800_Warmstart();
      break;
    case KM_ACT_COLD_RESET:
      Serial.println("action: cold reset");
      Atari800_Coldstart();
      break;
    case KM_ACT_BREAK:
      Serial.println("action: break");
      port_raise_break_irq();   /* POKEY IRQ bit 7 — BASIC's STOP/BREAK handler reads this */
      break;

    case KM_ACT_TOGGLE_INPUT_MODE:
      mode_toggle();
      Serial.printf("input: mode = %s\n",
                    mode_current() == MODE_JOYSTICK ? "joystick" : "keyboard");
      break;

    case KM_ACT_LOAD_XEX:
      /* Re-entry-safe: BINLOAD_Loader internally calls Atari800_Coldstart()
         on success. try_load_xex logs failure. Same path as original M2 boot
         hook; just triggered by user keystroke instead of happening at setup. */
      Serial.println("action: load /atari800/test.xex");
      try_load_xex("/atari800/test.xex");
      break;

    case KM_ACT_VOLUME_DOWN: case KM_ACT_VOLUME_UP:       /* T10 wires audio */
    case KM_ACT_MENU_OPEN:                                /* T15 wires menu */
    case KM_ACT_SAVE_STATE: case KM_ACT_LOAD_STATE:       /* M5 stubs */
    case KM_ACT_INVERSE_VIDEO:
      Serial.printf("action: %d (not yet wired)\n", (int)act);
      break;
    default: break;
  }
}

// Cardputer-Adv SD pins per M5Stack's own M5Cardputer/examples/Basic/sdcard.ino
static constexpr int SD_PIN_SCK  = 40;
static constexpr int SD_PIN_MISO = 39;
static constexpr int SD_PIN_MOSI = 14;
static constexpr int SD_PIN_CS   = 12;

static bool sd_mounted = false;

static bool mount_sd() {
  // Absolute-minimum mount per M5Stack's official example. Previous attempts
  // at "defensive cleanup" (esp_vfs_unregister, ff_diskio_register(NULL),
  // periph_module_reset) all made the mount WORSE by leaving the VFS
  // subsystem in a partially-initialized state that esp_vfs_fat_register
  // then rejects with ESP_ERR_INVALID_STATE. The minimum works on cold boot;
  // if M5Launcher handoff breaks it, that's a separate known limitation.
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, SPI, 25000000)) {
    Serial.println("SD: mount failed (no card? wrong format? first boot after Launcher handoff?)");
    return false;
  }
  uint64_t size_mb = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD: mounted, %llu MB\n", size_mb);
  return true;
}

static void list_sd_root() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("SD: could not open root");
    return;
  }
  Serial.println("SD: root listing:");
  while (File f = root.openNextFile()) {
    Serial.printf("  %s%s  %u bytes\n",
                  f.isDirectory() ? "DIR " : "    ",
                  f.name(),
                  (unsigned)f.size());
    f.close();
  }
  root.close();
}

// M2 .xex loader — delegates to atari800 core's BINLOAD_Loader, which uses
// stdio fopen() (routed through ESP-IDF VFS to the SD mount at /sd) and
// intercepts the SIO boot-sector read to inject a fake boot sector that
// drives proper xex segment loading + RUNAD/INITAD handling. This is what
// the desktop atari800 does when you pass a .xex on the command line.
//
// The earlier manual approach (poke into MEMORY_mem then Atari800_Coldstart)
// didn't work because Coldstart resets RAM *after* our pokes.
static bool try_load_xex(const char* path) {
  if (!sd_mounted) {
    Serial.printf("xex: SD not mounted, skipping %s\n", path);
    return false;
  }

  // Translate SD-relative path to a VFS absolute path under /sd.
  char vfs_path[128];
  if (path[0] == '/') snprintf(vfs_path, sizeof vfs_path, "/sd%s", path);
  else                snprintf(vfs_path, sizeof vfs_path, "/sd/%s", path);

  Serial.printf("xex: BINLOAD_Loader(\"%s\")\n", vfs_path);
  int ok = BINLOAD_Loader(vfs_path);
  Serial.printf("xex: BINLOAD_Loader returned %d\n", ok);
  if (ok) {
    mode_autodetect_for(path);   /* .xex -> MODE_JOYSTICK */
    Serial.printf("input: mode = %s\n",
                  mode_current() == MODE_JOYSTICK ? "joystick" : "keyboard");
  }
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");
  Serial.println("FW_VER=v0.3-m3-t7");

  // ---- Heap diagnostics BEFORE any big allocations ----
  size_t free0  = ESP.getFreeHeap();
  size_t largest0 = ESP.getMaxAllocHeap();
  Serial.printf("heap@entry: free=%u largest=%u psram=%u\n",
                (unsigned)free0, (unsigned)largest0, (unsigned)ESP.getFreePsram());

  // Per-region details (so we can see if the "fragmented 38 KB" outside the
  // big block contains any single chunk >= 16 KB — critical for under_atarixl_os).
  multi_heap_info_t hi;
  heap_caps_get_info(&hi, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf("heap INTERNAL|8BIT: free=%u largest=%u min_free=%u blocks(free=%u,alloc=%u)\n",
                (unsigned)hi.total_free_bytes,
                (unsigned)hi.largest_free_block,
                (unsigned)hi.minimum_free_bytes,
                (unsigned)hi.free_blocks,
                (unsigned)hi.allocated_blocks);
  heap_caps_get_info(&hi, MALLOC_CAP_INTERNAL);
  Serial.printf("heap INTERNAL     : free=%u largest=%u\n",
                (unsigned)hi.total_free_bytes,
                (unsigned)hi.largest_free_block);

  // ---- Big contiguous allocations FIRST, while heap@entry has a 196 KB
  //      contiguous block. Packing is tight: we need 92 + 65 + 16 = 173 KB
  //      contiguous (Screen + MEMORY_mem + under_xlos), which fits in 196 KB
  //      with ~23 KB to spare. Previous ordering (SD first) dropped largest
  //      to 172 KB and failed this packing by ~1 KB. SD mount is last now —
  //      its FATFS state (~27 KB across several small allocs) goes into the
  //      fragments left behind.

  // ---- The real Screen_atari allocation ----
  // atari800 core only needs Screen_WIDTH * Screen_HEIGHT = 384*240 = 92160
  // bytes (per screen.h comments).
  constexpr size_t buf_bytes = 384 * 240;
  Screen_atari = (ULONG*) malloc(buf_bytes);
  if (Screen_atari) {
    memset(Screen_atari, 0, buf_bytes);
    Serial.printf("Screen_atari: pre-allocated %u bytes @ %p\n",
                  (unsigned)buf_bytes, Screen_atari);
  } else {
    Serial.printf("Screen_atari: ALLOC FAILED for %u bytes (errno=%d)\n",
                  (unsigned)buf_bytes, errno);
  }
  Serial.printf("heap@post-screen: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  // ---- MEMORY_mem (65538 bytes) — 6502 address space.
  ensure_memory_mem_allocated();
  if (MEMORY_mem) {
    Serial.printf("MEMORY_mem: pre-allocated @ %p\n", (void*)MEMORY_mem);
  } else {
    Serial.println("MEMORY_mem: ALLOC FAILED — core init will crash");
  }
  Serial.printf("heap@post-mem-alloc: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  // ---- XL/XE shadow buffers (under_atarixl_os 16 KB + under_cart809F 8 KB +
  //      under_cartA0BF 8 KB = 32 KB). 16 KB goes into the big block's
  //      leftover (~23 KB after Screen+MEMORY_mem); the two 8 KB ones find
  //      small-block slots.
  ensure_under_buffers_allocated();
  Serial.printf("heap@post-under-alloc: free=%u largest=%u xlos=%p 809F=%p A0BF=%p\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                debug_get_under_atarixl_os(),
                debug_get_under_cart809F(),
                debug_get_under_cartA0BF());

  // ---- Mount SD LAST, after the big contiguous allocs. FATFS' internal
  //      allocations are small (~4-8 KB chunks) and fit in fragments.
  sd_mounted = mount_sd();
  Serial.printf("heap@post-sd: free=%u largest=%u sd=%d\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                sd_mounted ? 1 : 0);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enableKeyboard (default)

  // Wire the Fn-layer action handler now that M5Cardputer.Display (used by
  // brightness actions) is alive. input_port::poll() will fire this back
  // synchronously from the main loop whenever Fn+<action-key> is pressed.
  input_port::set_action_handler(on_input_action);

  // splash screen
  auto& d = M5Cardputer.Display;
  d.setRotation(1);                        // landscape, text-friendly
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.setCursor(8, 16);
  d.print("cardputer-atari800");
  d.setCursor(8, 32);
  d.print("v0.3-m3-t7");
  d.setCursor(8, 56);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.print("xex: Fn+\\ modes");

  Serial.println("splash rendered");

  // Hold the splash on-screen for 2 seconds so a human can read the version
  // string before the LCD gets taken over by the emulator frame renderer.
  // Remove this delay once builds stabilize (M3+).
  delay(2000);

  // SD was already mounted at the very top of setup(), before the heap got
  // fragmented. Here we just report the result visually and list root.
  if (sd_mounted) {
    list_sd_root();
    d.setCursor(8, 80);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.print("SD: mounted");
  } else {
    d.setCursor(8, 80);
    d.setTextColor(TFT_RED, TFT_BLACK);
    d.print("SD: not mounted");
  }

  // M2: init the atari800 core.
  // "-basic" clears Atari800_disable_basic (core default is TRUE, which
  // simulates OPTION-held at boot in gtia.c:571 and leaves the OS stuck
  // in SIO-wait mode showing a diskette animation — no BASIC READY).
  // M2 never hit this because T14 jumped straight to xex loading; T2's
  // HUMAN CHECKPOINT needs BASIC to boot so typing has something to echo to.
  Serial.println("core: initialising atari800...");
  int argc = 2;
  char* argv[] = {(char*)"atari800", (char*)"-basic"};
  int init_ok = Atari800_Initialise(&argc, argv);
  Serial.printf("core: init_ok=%d\n", init_ok);

  if (!init_ok) {
    auto& d2 = M5Cardputer.Display;
    d2.setCursor(8, 100);
    d2.setTextColor(TFT_RED, TFT_BLACK);
    d2.print("core init failed");
    return;
  }

  // M2: set up display renderer for PAL + Stretch (default).
  lcd::init();
  renderer::set_mode(renderer::Mode::Stretch);
  renderer::set_region_ntsc(false);

  size_t free_heap = ESP.getFreeHeap();
  Serial.printf("heap: free=%u bytes after core init\n", (unsigned)free_heap);

  dump_shadow_ptrs("post-core-init");

  // M3: boot into AltirraBASIC READY by default. User triggers xex load via
  // Fn+L (KM_ACT_LOAD_XEX), which calls try_load_xex("/atari800/test.xex").
  // Avoids the "must rename test.xex" friction from early M3 testing.
}

void loop() {
  M5Cardputer.update();

  // Keep the T5 keyboard debug serial output — useful for M2 diagnostics
  // and harmless. M3 routes these to the Atari core properly.
  if (M5Cardputer.Keyboard.isChange()) {
    auto status = M5Cardputer.Keyboard.keysState();
    Serial.print("keys:");
    if (status.ctrl)  Serial.print(" CTRL");
    if (status.shift) Serial.print(" SHIFT");
    if (status.alt)   Serial.print(" ALT");
    if (status.fn)    Serial.print(" FN");
    if (status.opt)   Serial.print(" OPT");
    for (auto c : status.word)     Serial.printf(" '%c'(0x%02x)", c, c);
    for (auto k : status.hid_keys) Serial.printf(" hid=0x%02x", k);
    Serial.println();
    // Fn+<action-key> dispatch now flows through input_port::poll() →
    // on_input_action(); the inline handler that used to live here is gone.
  }

  // Frame loop — run atari800 at ~50 Hz (PAL) and present.
  static uint32_t last_frame_ms = 0;
  static int frame_count = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) {
    last_frame_ms = now;
    input_port::poll();          // snapshot keyboard before stepping the core
    if (frame_count < 3) {
      Serial.printf("frame %d begin\n", frame_count);
      dump_shadow_ptrs("pre-frame");
    }
    Atari800_Frame();
    if (frame_count < 3) {
      Serial.printf("frame %d done\n", frame_count);
    }
    frame_count++;
    renderer::present(reinterpret_cast<const uint8_t*>(Screen_atari));
  }

  // Heartbeat (every 10s) — proof the main loop is healthy.
  static uint32_t last_hb = 0;
  if (now - last_hb >= 10000) {
    last_hb = now;
    Serial.printf("uptime %lu ms heap=%u\n", now, (unsigned)ESP.getFreeHeap());
  }
}
