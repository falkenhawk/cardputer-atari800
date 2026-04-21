// cardputer-atari800 — entry point
// Milestone 2: core init + 50 Hz frame loop → LCD

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <driver/periph_ctrl.h>   // periph_module_reset() — clears SDMMC
                                  // peripheral state left behind when
                                  // M5Launcher (which uses SD_MMC) hands
                                  // control off to us (which uses SD via SPI).
#include <errno.h>

#include "display/lcd.h"
#include "display/renderer.h"

extern "C" {
#include "../lib/atari800/src/atari.h"
#include "../lib/atari800/port.h"
#include "../lib/atari800/src/screen.h"
#include "../lib/atari800/src/memory.h"
}

#include "storage/loader.h"

extern "C" void ensure_memory_mem_allocated(void);
extern "C" void ensure_under_buffers_allocated(void);

static renderer::Mode g_display_mode = renderer::Mode::Stretch;

static const char* mode_name(renderer::Mode m) {
  switch (m) {
    case renderer::Mode::PixelPerfect: return "Pixel-perfect";
    case renderer::Mode::Pillarbox:    return "Pillarbox";
    case renderer::Mode::Cover:        return "Cover";
    case renderer::Mode::Stretch:      return "Stretch";
  }
  return "?";
}

// Cardputer-Adv SD pins per M5Stack official M5Cardputer/examples/Basic/sdcard.
static constexpr int SD_PIN_SCK  = 40;
static constexpr int SD_PIN_MISO = 39;
static constexpr int SD_PIN_MOSI = 14;
static constexpr int SD_PIN_CS   = 12;

static bool sd_mounted = false;

static bool mount_sd() {
  // M5Launcher mounts the SD via SD_MMC (1-bit SDIO) and jumps to our app
  // without fully resetting peripherals. The SDMMC hardware still "owns"
  // the SD bus, so our SPI-based SD.begin() sees "no card". Reset the
  // SDMMC peripheral to release the bus, then proceed as M5Stack's own
  // sdcard.ino example does.
  periph_module_reset(PERIPH_SDMMC_MODULE);
  delay(10);

  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, SPI, 25000000)) {
    Serial.println("SD: mount failed (no card? wrong format? pins held?)");
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

// M2 hardcoded .xex loader — reads a .xex from SD, pokes its segments into
// MEMORY_mem[], writes RUNAD/INITAD vectors, and Coldstarts the Atari.
// M4 will replace this with a proper file browser.
static bool try_load_xex(const char* path) {
  if (!sd_mounted) {
    Serial.printf("xex: SD not mounted, skipping %s\n", path);
    return false;
  }
  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.printf("xex: %s not found\n", path);
    return false;
  }
  size_t flen = f.size();
  if (flen == 0 || flen > 64 * 1024) {
    Serial.printf("xex: bad size %u (max 64 KB)\n", (unsigned)flen);
    f.close();
    return false;
  }

  // Heap buffer — freed before Atari800_Coldstart. Heap is tight (~36 KB free
  // at runtime); if malloc fails we bail gracefully.
  uint8_t* xex_buf = (uint8_t*) malloc(flen);
  if (!xex_buf) {
    Serial.printf("xex: malloc(%u) failed — heap too tight\n", (unsigned)flen);
    f.close();
    return false;
  }
  size_t nread = f.read(xex_buf, flen);
  f.close();
  if (nread != flen) {
    Serial.printf("xex: short read %u/%u\n", (unsigned)nread, (unsigned)flen);
    free(xex_buf);
    return false;
  }

  xex_parsed_t p;
  if (!xex_parse(xex_buf, flen, &p)) {
    Serial.println("xex: parse failed");
    free(xex_buf);
    return false;
  }

  // Poke segments into Atari RAM.
  for (int i = 0; i < p.n_segs; i++) {
    const xex_segment_t& s = p.segs[i];
    for (size_t b = 0; b < s.data_len; b++) {
      MEMORY_mem[s.start_addr + b] = s.data[b];
    }
  }

  // Set the run / init vectors in page 2.
  if (p.run_addr) {
    MEMORY_mem[0x02E0] = p.run_addr & 0xFF;
    MEMORY_mem[0x02E1] = (p.run_addr >> 8) & 0xFF;
  }
  if (p.init_addr) {
    MEMORY_mem[0x02E2] = p.init_addr & 0xFF;
    MEMORY_mem[0x02E3] = (p.init_addr >> 8) & 0xFF;
  }

  Serial.printf("xex: loaded %d segments from %s, RUN=0x%04X, INIT=0x%04X\n",
                p.n_segs, path, p.run_addr, p.init_addr);

  free(xex_buf);
  Atari800_Coldstart();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");

  // ---- Heap diagnostics BEFORE any big allocations ----
  size_t free0  = ESP.getFreeHeap();
  size_t largest0 = ESP.getMaxAllocHeap();
  Serial.printf("heap@entry: free=%u largest=%u psram=%u\n",
                (unsigned)free0, (unsigned)largest0, (unsigned)ESP.getFreePsram());

  // Try incremental alloc sizes to figure out where malloc fails
  for (size_t sz : { (size_t)32768, (size_t)65536, (size_t)98304 }) {
    void* p = malloc(sz);
    if (p) {
      Serial.printf("  malloc(%u) OK @ %p\n", (unsigned)sz, p);
      free(p);
    } else {
      Serial.printf("  malloc(%u) FAILED (errno=%d)\n", (unsigned)sz, errno);
    }
  }

  // ---- The real Screen_atari allocation ----
  constexpr size_t buf_bytes = 384 * (240 + 16);
  Screen_atari = (ULONG*) malloc(buf_bytes);
  if (Screen_atari) {
    memset(Screen_atari, 0, buf_bytes);
    Serial.printf("Screen_atari: pre-allocated %u bytes @ %p\n",
                  (unsigned)buf_bytes, Screen_atari);
  } else {
    Serial.printf("Screen_atari: ALLOC FAILED for %u bytes (errno=%d)\n",
                  (unsigned)buf_bytes, errno);
  }

  size_t free1 = ESP.getFreeHeap();
  Serial.printf("heap@post-alloc: free=%u\n", (unsigned)free1);

  // ---- Pre-allocate MEMORY_mem (65538 bytes). Frees another 64 KB of static
  //      DRAM so Screen_atari + MEMORY_mem both fit in the available heap
  //      without fragmenting into sub-65 KB chunks.
  ensure_memory_mem_allocated();
  if (MEMORY_mem) {
    Serial.printf("MEMORY_mem: pre-allocated @ %p\n", (void*)MEMORY_mem);
  } else {
    Serial.println("MEMORY_mem: ALLOC FAILED — core init will crash");
  }
  Serial.printf("heap@post-mem-alloc: free=%u\n", (unsigned)ESP.getFreeHeap());

  // Pre-alloc the three XL/XE shadow buffers (under_atarixl_os 16 KB,
  // under_cart809F 8 KB, under_cartA0BF 8 KB = 32 KB total). MEMORY_HandlePORTB
  // memcpy's to them on every bank switch, so if they're NULL the very
  // first frame crashes in memset/memcpy with EXCVADDR=0x1000.
  ensure_under_buffers_allocated();
  Serial.printf("heap@post-under-alloc: free=%u\n", (unsigned)ESP.getFreeHeap());

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enableKeyboard (default)

  // splash screen
  auto& d = M5Cardputer.Display;
  d.setRotation(1);                        // landscape, text-friendly
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.setCursor(8, 16);
  d.print("cardputer-atari800");
  d.setCursor(8, 32);
  d.print("v0.2-m2-t14d");
  d.setCursor(8, 56);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.print("xex: Fn+\\ modes");

  Serial.println("splash rendered");

  sd_mounted = mount_sd();
  if (sd_mounted) {
    list_sd_root();

    // also show mount status on LCD
    d.setCursor(8, 80);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.print("SD: mounted");
  } else {
    d.setCursor(8, 80);
    d.setTextColor(TFT_RED, TFT_BLACK);
    d.print("SD: not mounted");
  }

  // M2: init the atari800 core
  Serial.println("core: initialising atari800...");
  int argc = 1;
  char* argv[] = {(char*)"atari800"};
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

  // Try to boot a hardcoded test .xex. If absent, AltirraOS's default
  // prompt stays visible (the non-booted state we saw in T12).
  try_load_xex("/atari800/test.xex");
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
    // Fn+\ cycles display mode (M2 shortcut; full Fn layer comes in M3).
    if (status.fn) {
      for (auto c : status.word) {
        if (c == '\\') {
          g_display_mode = static_cast<renderer::Mode>(
            (static_cast<int>(g_display_mode) + 1) % 4);
          renderer::set_mode(g_display_mode);
          Serial.printf("display: %s\n", mode_name(g_display_mode));
        }
      }
    }
    Serial.println();
  }

  // Frame loop — run atari800 at ~50 Hz (PAL) and present.
  static uint32_t last_frame_ms = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) {
    last_frame_ms = now;
    Atari800_Frame();
    renderer::present(reinterpret_cast<const uint8_t*>(Screen_atari));
  }

  // Heartbeat (every 10s) — proof the main loop is healthy.
  static uint32_t last_hb = 0;
  if (now - last_hb >= 10000) {
    last_hb = now;
    Serial.printf("uptime %lu ms heap=%u\n", now, (unsigned)ESP.getFreeHeap());
  }
}
