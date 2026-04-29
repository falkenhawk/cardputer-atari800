// cardputer-atari800 — entry point
// Milestone 2: core init + 50 Hz frame loop → LCD

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <errno.h>
#include <esp_heap_caps.h>

#include "audio/audio_out.h"
#include "display/lcd.h"
#include "display/renderer.h"
#include "display/screenshot.h"
#include "input/input_port.h"
#include "ui/rom_browser.h"
#include "roms/rom_args.h"

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
extern "C" void ensure_under_atarixl_os_allocated(void);
extern "C" void ensure_under_cart_buffers_allocated(void);
extern "C" void ensure_under_buffers_allocated(void);
extern "C" void* debug_get_under_atarixl_os(void);
extern "C" void* debug_get_under_cart809F(void);
extern "C" void* debug_get_under_cartA0BF(void);
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
      audio_out::suppress_for_ms(2000);
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
      /* Fn+L opens the ROM browser. Loader dispatch (xex/atr/car/cas/...)
         goes through atari800's AFILE_OpenFile, which magic-byte-detects
         the format. Browser walks up from /sd/atari800/roms to deepest
         existing dir on first open and remembers last dir until reboot. */
      Serial.println("action: open ROM browser");
      rom_browser::open();
      break;

    case KM_ACT_SCREENSHOT:
      /* Renderer::present() picks up the armed flag on the next frame and
         writes a BMP to /sd/atari800/screenshots/shot_<millis>.bmp. */
      Serial.println("action: screenshot");
      screenshot::arm();
      break;

    case KM_ACT_VOLUME_DOWN: {
      audio_out::set_volume_delta(-16);
      Serial.printf("volume: %u\n", M5Cardputer.Speaker.getVolume());
      break;
    }
    case KM_ACT_VOLUME_UP: {
      audio_out::set_volume_delta(+16);
      Serial.printf("volume: %u\n", M5Cardputer.Speaker.getVolume());
      break;
    }

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
static constexpr uint8_t SD_MAX_OPEN_FILES = 1;

static bool sd_mounted = false;

static bool sd_file_has_size(const char* path, size_t expected_size) {
  if (!sd_mounted || !path) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t actual = f.size();
  f.close();
  if (actual != expected_size) {
    Serial.printf("rom: ignoring %s size=%u expected=%u\n",
                  path, (unsigned)actual, (unsigned)expected_size);
    return false;
  }
  return true;
}

static const char* select_rom_path(const char* sd_upper, const char* vfs_upper,
                                   const char* sd_lower, const char* vfs_lower,
                                   size_t expected_size) {
  if (sd_file_has_size(sd_upper, expected_size)) return vfs_upper;
  if (sd_file_has_size(sd_lower, expected_size)) return vfs_lower;
  return nullptr;
}

static bool mount_sd() {
  // Absolute-minimum mount per M5Stack's official example.
  // Diagnostic logging added to find why SD started failing — we want to
  // see the DMA-capable heap state, timing of SPI.begin/SD.begin, and
  // whether a retry pattern recovers.
  multi_heap_info_t hi_dma;
  heap_caps_get_info(&hi_dma, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf("SD: pre-mount heap DMA|INT|8: free=%u largest=%u min_free=%u\n",
                (unsigned)hi_dma.total_free_bytes,
                (unsigned)hi_dma.largest_free_block,
                (unsigned)hi_dma.minimum_free_bytes);

  uint32_t t0 = millis();
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  uint32_t t1 = millis();
  Serial.printf("SD: SPI.begin took %lums\n", (unsigned long)(t1 - t0));

  bool ok = SD.begin(SD_PIN_CS, SPI, 25000000, "/sd", SD_MAX_OPEN_FILES);
  uint32_t t2 = millis();
  Serial.printf("SD: SD.begin returned %d after %lums\n", ok ? 1 : 0,
                (unsigned long)(t2 - t1));

  if (!ok) {
    Serial.println("SD: trying SD.end() + retry...");
    SD.end();
    delay(50);
    ok = SD.begin(SD_PIN_CS, SPI, 25000000, "/sd", SD_MAX_OPEN_FILES);
    uint32_t t3 = millis();
    Serial.printf("SD: retry returned %d after %lums\n", ok ? 1 : 0,
                  (unsigned long)(t3 - t2));
  }

  if (!ok) {
    Serial.println("SD: trying lower clock 4MHz...");
    SD.end();
    delay(50);
    ok = SD.begin(SD_PIN_CS, SPI, 4000000, "/sd", SD_MAX_OPEN_FILES);
    uint32_t t4 = millis();
    Serial.printf("SD: 4MHz retry returned %d after %lums\n", ok ? 1 : 0,
                  (unsigned long)(t4 - t2));
  }

  if (!ok) {
    Serial.println("SD: mount failed after retries");
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


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");
  Serial.println("FW_VER=v0.3-m3-t10ba-good");

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
  //      contiguous (Screen + MEMORY_mem + under_xlos), which fits in 196 KB.
  //      SD cannot mount before those three allocations, because that
  //      fragments the large block. The cart shadows must also exist before
  //      the core starts. To make SD fit afterward, mount it with max_files=1.

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
  //      under_cartA0BF 8 KB = 32 KB). All three must be non-null before
  //      Atari800_Initialise(), otherwise PORTB bank switching can crash.
  ensure_under_buffers_allocated();
  Serial.printf("heap@post-under-alloc: free=%u largest=%u xlos=%p 809F=%p A0BF=%p\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                debug_get_under_atarixl_os(),
                debug_get_under_cart809F(),
                debug_get_under_cartA0BF());

  // ---- Mount SD after the required Atari shadows, using a small VFS file
  //      table. t10ad used the default max_files=5 and failed with
  //      ESP_ERR_NO_MEM while largest free was 19444.
  sd_mounted = mount_sd();
  Serial.printf("heap@post-sd: free=%u largest=%u sd=%d\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                sd_mounted ? 1 : 0);

  // Audio pre-alloc AFTER SD mount. t10e-diag confirmed SD mount works when
  // this call is SKIPPED; t10f re-enables it to see if the malloc path
  // affects anything in-flight.
  if (!audio_out::preallocate_buffers()) {
    Serial.println("audio: preallocate_buffers FAILED");
  } else {
    Serial.println("audio: buffers pre-allocated");
  }
  Serial.printf("heap@post-audio-prealloc: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

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
  d.print("v0.3-m3-t10ba-good");
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
  const char* os_rom_path = select_rom_path(ROM_ARGS_SD_OS_ROM, ROM_ARGS_VFS_OS_ROM,
                                            ROM_ARGS_SD_OS_ROM_LC, ROM_ARGS_VFS_OS_ROM_LC,
                                            ROM_ARGS_OS_ROM_BYTES);
  const char* basic_rom_path = select_rom_path(ROM_ARGS_SD_BASIC_ROM, ROM_ARGS_VFS_BASIC_ROM,
                                               ROM_ARGS_SD_BASIC_ROM_LC, ROM_ARGS_VFS_BASIC_ROM_LC,
                                               ROM_ARGS_BASIC_ROM_BYTES);
  Serial.printf("rom: %s %s\n",
                os_rom_path ? os_rom_path : "using embedded XL OS",
                basic_rom_path ? basic_rom_path : "using embedded BASIC");

  char* argv[ROM_ARGS_MAX];
  int argc = rom_args_build(argv, ROM_ARGS_MAX, os_rom_path, basic_rom_path);
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

  // Audio — now running raw ESP-IDF i2s_std + direct ES8311 codec writes via
  // M5.In_I2C. No M5Unified Speaker_Class involved, so the 1 KB BSS-shrink-
  // breaks-SD regression doesn't apply here.
  if (audio_out::start(0 /* mono */)) {
    Serial.println("audio: started (mono, raw i2s_std path)");
  } else {
    Serial.println("audio: start FAILED");
  }
  Serial.printf("heap@post-audio-start: free=%u largest=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  size_t free_heap = ESP.getFreeHeap();
  Serial.printf("heap: free=%u bytes after core init\n", (unsigned)free_heap);

  dump_shadow_ptrs("post-core-init");

  // M3: boot into AltirraBASIC READY by default. User triggers xex load via
  // Fn+L (KM_ACT_LOAD_XEX), which calls try_load_xex("/atari800/test.xex").
  // Avoids the "must rename test.xex" friction from early M3 testing.
}

void loop() {
  M5Cardputer.update();

  // (Removed the M2 keyboard debug print: M5Cardputer.Keyboard.isChange()
  // is consumed-on-read [Keyboard.cpp:66 mutates _last_key_size], so any
  // caller after it sees false and misses the keystroke. Real keyboard
  // routing goes through input_port::poll() reading keysState() directly,
  // and rom_browser::poll() does the same — neither needs isChange().)

  // ROM browser overlay — owns screen + keyboard while open. Atari core
  // stepping suspends so user input doesn't leak into the emulated machine.
  if (rom_browser::is_open()) {
    rom_browser::poll();
    return;
  }

  // Frame loop — run atari800 at ~50 Hz (PAL) and present.
  static uint32_t last_frame_ms = 0;
  static int frame_count = 0;
  uint32_t now = millis();
  if (now - last_frame_ms >= 20) {
    last_frame_ms = now;
    input_port::poll();          // snapshot keyboard before stepping the core
    /* If the input action just opened the ROM browser, skip the rest of
       this frame — otherwise Atari800_Frame() + renderer::present()
       would clobber the browser screen we just drew. Subsequent loop
       iterations short-circuit at the top via the is_open() check. */
    if (rom_browser::is_open()) return;
    if (frame_count < 3) {
      Serial.printf("frame %d begin\n", frame_count);
      dump_shadow_ptrs("pre-frame");
    }
    uint32_t t_af = micros();
    Atari800_Frame();
    uint32_t t_render = micros();
    renderer::present(reinterpret_cast<const uint8_t*>(Screen_atari));
    uint32_t t_end = micros();
    if ((frame_count % 50) == 0) {
      Serial.printf("frame %d: atari=%lu render=%lu total=%lu\n",
                    frame_count,
                    (unsigned long)(t_render - t_af),
                    (unsigned long)(t_end - t_render),
                    (unsigned long)(t_end - t_af));
    }
    if (frame_count < 3) {
      Serial.printf("frame %d done\n", frame_count);
    }
    frame_count++;
  }

  // Heartbeat (every 10s) — proof the main loop is healthy.
  static uint32_t last_hb = 0;
  if (now - last_hb >= 10000) {
    last_hb = now;
    Serial.printf("uptime %lu ms heap=%u\n", now, (unsigned)ESP.getFreeHeap());
  }
}
