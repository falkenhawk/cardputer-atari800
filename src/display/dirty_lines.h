#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRTY_LINES_MAX 135

typedef struct {
  uint32_t hashes[DIRTY_LINES_MAX];
  uint8_t valid;
} dirty_lines_t;

void dirty_lines_reset(dirty_lines_t* dirty);
uint32_t dirty_lines_hash_bytes(const uint8_t* bytes, int width);
uint32_t dirty_lines_hash_rgb565(const uint16_t* pixels, int width);
int dirty_lines_check_and_store(dirty_lines_t* dirty, int y, uint32_t hash);
void dirty_lines_end_frame(dirty_lines_t* dirty);

#ifdef __cplusplus
}
#endif
