// cardputer-atari800 — entry point
// Milestone 2: core init + 50 Hz frame loop → LCD

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

#include "display/lcd.h"
#include "display/renderer.h"

extern "C" {
#include "../lib/atari800/src/atari.h"
#include "../lib/atari800/port.h"
#include "../lib/atari800/src/screen.h"
}

// Cardputer-Adv SD card pins — verified on hardware.
static constexpr int SD_PIN_SCK  = 40;
static constexpr int SD_PIN_MISO = 39;
static constexpr int SD_PIN_MOSI = 14;
static constexpr int SD_PIN_CS   = 12;

static bool sd_mounted = false;

static bool mount_sd() {
  // Cardputer-Adv wires the SD card to hardware SPI2 on the ESP32-S3, which is
  // separate from the LCD's bus. Re-initializing the `SPI` singleton here is
  // safe because LGFX (the display driver) uses its own bus instance.
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, SPI, 25000000)) {
    Serial.println("SD: mount failed (no card? wrong format? wrong pins?)");
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
  d.print("v0.1-m1");
  d.setCursor(8, 56);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.print("bootstrap + HAL smoke");

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
