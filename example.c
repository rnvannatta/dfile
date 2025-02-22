#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "dfile.h"
#include <stdio.h>

int main() {
  {
    char * msg = "Hello, fwrite!\n";
    dfwrite(msg, strlen(msg), dstdout);
  }
  {
    DFILE * fi = dfopen("testfile", "r");
    char buf[256];
    if(!dfgets(buf, sizeof buf, fi))
      return -1;
    dfputs(buf, dstdout);
    dfclose(fi);
  }
  {
    char buf[32];
    dungetc('\n', dstdin);
    dungetc('Q', dstdin);
    dfgets(buf, sizeof buf, dstdin);
    dfputs(buf, dstdout);
  }
  {
    DFILE * f = dtmpfile();
    char * msg = "ello, Tempfiles!";
    dfwrite(msg, strlen(msg), f);
    dungetc('M', f);
    dfseek(f, 0, SEEK_SET);
    char buf[256];
    dungetc(dfgetc(f), f);
    dungetc('H', f);
    if(!dfgets(buf, sizeof buf, f))
      return -1;
    dputs(buf);
  }
  {
    DFILE * f = dstrfile();
    char * msg = "Hello, Strings!";
    dfwrite(msg, strlen(msg), f);
    dfseek(f, 0, SEEK_SET);
    char buf[20];
    if(!dfgets(buf, sizeof buf, f))
      return -1;
    dputs(buf);
  }
  {
#ifdef _WIN64
    DFILE * f = dpopen("dir", "r");
#else
    DFILE * f = dpopen("ls", "r");
#endif
    char buf[1024], *ptr = buf;
    int nbytes;
    do {
      nbytes = dfread(ptr, buf + sizeof buf - ptr, f);
      if(nbytes > 0)
        ptr += nbytes;
    } while(nbytes > 0);
    if(ptr < buf + sizeof buf)
      *ptr = 0;
    else
      buf[sizeof buf - 1] = 0;
    dfputs(buf, dstdout);
    if(dpclose(f))
      return -1;
  }
  
  dfputs("this should show up\nbut not\nthis", dstdout);
  // fast exit to stop the this from flushing. change to exit(0) to validate
  // flush on exit
  _exit(0);
}
