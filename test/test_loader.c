/* test_loader.c — .xex parser. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

extern int xex_parse(const uint8_t* buf, size_t len, xex_parsed_t* out);

static int fail = 0;
#define CHECK(expr, msg) do { if (!(expr)) { fprintf(stderr, "FAIL: " msg "\n"); fail = 1; } } while (0)

int main(void) {
  /* Minimal .xex: header 0xFFFF, one segment 0x2000-0x2003 containing 4 bytes,
     plus a RUNAD segment */
  uint8_t xex[] = {
    0xFF, 0xFF,                               /* header */
    0x00, 0x20,                               /* seg start 0x2000 */
    0x03, 0x20,                               /* seg end   0x2003 */
    0xAA, 0xBB, 0xCC, 0xDD,                   /* 4 data bytes */
    /* RUNAD: start=0x02E0, end=0x02E1, data = 0x2000 (little-endian) */
    0xE0, 0x02, 0xE1, 0x02, 0x00, 0x20
  };

  xex_parsed_t p;
  int ok = xex_parse(xex, sizeof(xex), &p);
  CHECK(ok, "xex_parse should succeed");
  CHECK(p.n_segs == 1, "should parse exactly 1 data segment (RUNAD is not a data seg)");
  CHECK(p.segs[0].start_addr == 0x2000, "seg[0] start addr");
  CHECK(p.segs[0].end_addr   == 0x2003, "seg[0] end addr");
  CHECK(p.segs[0].data_len   == 4,       "seg[0] data len");
  CHECK(p.segs[0].data[0]    == 0xAA,    "seg[0] data[0]");
  CHECK(p.run_addr           == 0x2000,  "RUNAD should be 0x2000");

  /* Truncated xex — parser should reject */
  uint8_t bad[] = { 0xFF, 0xFF, 0x00, 0x20 };  /* header + partial segment header, no end_addr bytes */
  xex_parsed_t p2;
  int ok2 = xex_parse(bad, sizeof(bad), &p2);
  CHECK(!ok2, "truncated xex should fail parse");

  /* Null buf */
  CHECK(!xex_parse(NULL, 100, &p2), "NULL buf should fail");

  /* Multiple segments with 0xFFFF resync between */
  uint8_t multi[] = {
    0xFF, 0xFF,                               /* header */
    0x00, 0x30, 0x01, 0x30,                   /* seg 1: 0x3000-0x3001 */
    0x11, 0x22,
    0xFF, 0xFF,                               /* resync marker (optional per xex spec) */
    0x00, 0x40, 0x01, 0x40,                   /* seg 2: 0x4000-0x4001 */
    0x33, 0x44,
  };
  xex_parsed_t p3;
  CHECK(xex_parse(multi, sizeof(multi), &p3), "multi-segment xex");
  CHECK(p3.n_segs == 2, "should parse 2 segments");
  CHECK(p3.segs[0].start_addr == 0x3000, "seg[0] start");
  CHECK(p3.segs[1].start_addr == 0x4000, "seg[1] start");

  /* Some XEX files contain repeated 0xFFFF markers at the start. The core
     binary loader supports this, so file-type detection must not reject it
     before the loader sees the file. */
  uint8_t repeated_header[] = {
    0xFF, 0xFF,
    0xFF, 0xFF,
    0x00, 0x50, 0x01, 0x50,
    0x55, 0x66,
  };
  xex_parsed_t p4;
  CHECK(xex_parse(repeated_header, sizeof(repeated_header), &p4),
        "xex with repeated leading FFFF markers");
  CHECK(p4.n_segs == 1 && p4.segs[0].start_addr == 0x5000,
        "repeated leading FFFF markers resync to first segment");

  if (fail) return EXIT_FAILURE;
  printf("PASS: loader\n");
  return EXIT_SUCCESS;
}
