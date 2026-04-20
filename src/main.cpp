// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

#include <Arduino.h>
#include <M5Cardputer.h>

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
}

void loop() {
  M5Cardputer.update();  // required each loop for keyboard/button state

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 5000) {
    last = now;
    Serial.printf("uptime %lu ms\n", now);
  }
  delay(10);
}
