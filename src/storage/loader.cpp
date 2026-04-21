// loader.cpp — minimal .xex parser (M2 subset).
// Supports: 0xFFFF header, standard segments, RUNAD (0x02E0-0x02E1),
// INITAD (0x02E2-0x02E3), 0xFFFF/0xFFFF resync markers.
// Does NOT support: compressed .xex, OSS/Bounty-Bob cartridges, etc.

#include "loader.h"
#include <string.h>

extern "C" int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out) {
  if (!buf || !out || len < 6) return 0;
  memset(out, 0, sizeof(*out));

  size_t pos = 0;
  // optional 0xFFFF header (must appear once at the start)
  if (buf[0] == 0xFF && buf[1] == 0xFF) pos = 2;

  while (pos + 2 <= len) {
    uint16_t start = buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    pos += 2;

    // 0xFFFF resync marker: consume only 2 bytes and loop to re-read start
    if (start == 0xFFFF) continue;

    // need 2 more bytes for end address
    if (pos + 2 > len) return 0;
    uint16_t end = buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    pos += 2;

    if (end < start) return 0;             // malformed
    size_t data_len = (size_t)(end - start + 1);
    if (pos + data_len > len) return 0;    // truncated payload

    // special segments:
    //   0x02E0/0x02E1 -> RUNAD (2 bytes, little-endian run address)
    //   0x02E2/0x02E3 -> INITAD (2 bytes)
    if (start == 0x02E0 && end == 0x02E1 && data_len == 2) {
      out->run_addr = buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    } else if (start == 0x02E2 && end == 0x02E3 && data_len == 2) {
      out->init_addr = buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    } else {
      if (out->n_segs >= XEX_MAX_SEGMENTS) return 0;
      xex_segment_t* s = &out->segs[out->n_segs++];
      s->start_addr = start;
      s->end_addr   = end;
      s->data       = &buf[pos];
      s->data_len   = data_len;
    }
    pos += data_len;
  }

  return 1;
}
