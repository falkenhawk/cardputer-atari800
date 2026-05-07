#include "dirty_lines.h"
#include <string.h>

void dirty_lines_reset(dirty_lines_t* dirty) {
  memset(dirty->hashes, 0, sizeof(dirty->hashes));
  dirty->valid = 0;
}

static uint32_t fnv1a_append(uint32_t hash, uint8_t byte) {
  hash ^= byte;
  hash *= 16777619U;
  return hash;
}

uint32_t dirty_lines_hash_bytes(const uint8_t* bytes, int width) {
  uint32_t hash = 0x9e3779b9U;
  int i = 0;
  for (; i + 4 <= width; i += 4) {
    uint32_t word = (uint32_t)bytes[i]
                  | ((uint32_t)bytes[i + 1] << 8)
                  | ((uint32_t)bytes[i + 2] << 16)
                  | ((uint32_t)bytes[i + 3] << 24);
    hash ^= word + 0x9e3779b9U + (hash << 6) + (hash >> 2);
  }
  for (; i < width; i++) {
    hash ^= (uint32_t)bytes[i] + 0x9e3779b9U + (hash << 6) + (hash >> 2);
  }
  return hash;
}

uint32_t dirty_lines_hash_rgb565(const uint16_t* pixels, int width) {
  uint32_t hash = 2166136261U;
  for (int i = 0; i < width; i++) {
    uint16_t pixel = pixels[i];
    hash = fnv1a_append(hash, (uint8_t)(pixel & 0xffU));
    hash = fnv1a_append(hash, (uint8_t)(pixel >> 8));
  }
  return hash;
}

int dirty_lines_check_and_store(dirty_lines_t* dirty, int y, uint32_t hash) {
  if (y < 0 || y >= DIRTY_LINES_MAX) return 0;
  int changed = !dirty->valid || dirty->hashes[y] != hash;
  dirty->hashes[y] = hash;
  return changed;
}

void dirty_lines_end_frame(dirty_lines_t* dirty) {
  dirty->valid = 1;
}
