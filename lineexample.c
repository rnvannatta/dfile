#include <stdio.h>
#include "dfile.h"

int main() {
  char buf[256];
  char c;
  printf("%d\n", EOF);
  while((c = d_getchar_unlocked()) >= 0)
  {
    d_putchar_unlocked(c);
    // shouldn't be necessary
    //d_fflush_unlocked(dstdout);
  }
}
