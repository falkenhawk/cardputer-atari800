// loader.h — minimal .xex parser (M2 subset).
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XEX_MAX_SEGMENTS 32

typedef struct {
  uint16_t start_addr;
  uint16_t end_addr;
  const uint8_t* data;
  size_t data_len;
} xex_segment_t;

typedef struct {
  xex_segment_t segs[XEX_MAX_SEGMENTS];
  int n_segs;
  uint16_t run_addr;
  uint16_t init_addr;
} xex_parsed_t;

int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out);

#ifdef __cplusplus
}
#endif
