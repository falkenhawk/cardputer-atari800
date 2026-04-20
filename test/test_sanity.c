// Smoke test — proves the CMake host toolchain is wired up. Replaced in M2
// by actual tests of the port layer / loaders / keymap.

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  // trivial invariant; a real test would assert against a known expected value
  int two_plus_two = 2 + 2;
  if (two_plus_two != 4) {
    fprintf(stderr, "FAIL: 2+2 = %d, expected 4\n", two_plus_two);
    return EXIT_FAILURE;
  }
  printf("PASS: sanity\n");
  return EXIT_SUCCESS;
}
