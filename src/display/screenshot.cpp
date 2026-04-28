/* screenshot.cpp — capture the exact bytes we pushed to the LCD as a BMP.
   BMP format chosen because: macOS Preview / Finder / any image viewer
   opens it without extra tools.

   Format: BITMAPINFOHEADER + BI_BITFIELDS (compression=3) with RGB565 masks,
   negative height (top-down rows). That way we can stream lines in render
   order (top to bottom) without buffering a whole frame. */

#include "screenshot.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>

namespace screenshot {

namespace {

bool g_armed = false;
File g_file;
int  g_width  = 0;
int  g_height = 0;
int  g_rows_written = 0;

/* Pack 4 / 2 / 4 / 2 bytes little-endian into buf. */
static void w32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static void w16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}

} /* anonymous namespace */

void arm() {
  if (g_armed) return;
  g_armed = true;
  Serial.println("screenshot: armed");
}

bool is_armed() { return g_armed; }

bool begin(int width, int height) {
  if (!g_armed) return false;
  /* Ensure folder exists. SD.mkdir is idempotent. */
  SD.mkdir("/atari800/screenshots");

  char path[96];
  snprintf(path, sizeof(path), "/atari800/screenshots/shot_%lu.bmp",
           (unsigned long)millis());
  Serial.printf("screenshot: opening %s (%dx%d)\n", path, width, height);

  g_file = SD.open(path, FILE_WRITE);
  if (!g_file) {
    Serial.println("screenshot: SD.open failed");
    g_armed = false;
    return false;
  }

  g_width  = width;
  g_height = height;
  g_rows_written = 0;

  /* BMP header: 14 (file) + 40 (info) + 12 (bitfields masks) = 66 bytes. */
  constexpr uint32_t kHeaderSize = 14 + 40 + 12;
  const uint32_t img_size = (uint32_t)width * (uint32_t)height * 2;

  uint8_t hdr[66] = {0};
  /* BITMAPFILEHEADER */
  hdr[0] = 'B'; hdr[1] = 'M';
  w32(hdr + 2, kHeaderSize + img_size);   /* total file size */
  /* reserved 0 at offset 6..9 */
  w32(hdr + 10, kHeaderSize);              /* pixel-data offset */

  /* BITMAPINFOHEADER (40 bytes) at offset 14 */
  w32(hdr + 14, 40);                       /* this header size */
  w32(hdr + 18, (uint32_t)width);
  w32(hdr + 22, (uint32_t)(-height));      /* negative = top-down */
  w16(hdr + 26, 1);                        /* planes */
  w16(hdr + 28, 16);                       /* bits per pixel */
  w32(hdr + 30, 3);                        /* compression = BI_BITFIELDS */
  w32(hdr + 34, img_size);
  w32(hdr + 38, 2835);                     /* x pix/meter (72 dpi) */
  w32(hdr + 42, 2835);                     /* y pix/meter */
  /* colors used + important at 46..53 = 0 (already zeroed) */

  /* Bitfield masks (RGB565) at offset 54 */
  w32(hdr + 54, 0xF800);   /* red   mask */
  w32(hdr + 58, 0x07E0);   /* green mask */
  w32(hdr + 62, 0x001F);   /* blue  mask */

  size_t wrote = g_file.write(hdr, sizeof(hdr));
  if (wrote != sizeof(hdr)) {
    Serial.printf("screenshot: header write short: %u / %u\n",
                  (unsigned)wrote, (unsigned)sizeof(hdr));
    g_file.close();
    g_armed = false;
    return false;
  }
  return true;
}

void write_line(const uint16_t* rgb565, int width) {
  if (!g_armed || !g_file) return;
  /* BMP 16-bit pixel rows are naturally 2-byte-aligned at width 240 (no pad). */
  g_file.write(reinterpret_cast<const uint8_t*>(rgb565), width * 2);
  g_rows_written++;
}

void end() {
  if (g_file) {
    g_file.close();
    Serial.printf("screenshot: wrote %d rows, closed\n", g_rows_written);
  }
  g_armed = false;
  g_width = g_height = g_rows_written = 0;
}

} /* namespace screenshot */
