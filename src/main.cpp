// cardputer-atari800 — entry point
// Milestone 1: bootstrap + HAL smoke test

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  // small delay so the USB CDC has time to enumerate before first print
  delay(500);
  Serial.println();
  Serial.println("cardputer-atari800 — boot");
  Serial.println("milestone 1: bootstrap + HAL smoke test");
}

void loop() {
  // idle heartbeat — proves the main loop is alive
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 5000) {
    last = now;
    Serial.printf("uptime %lu ms\n", now);
  }
  delay(10);
}
