#include <Arduino.h>
#include <M5Cardputer.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = clearDisplay

  Serial.println("M5Cardputer library initialized");
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
