#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "dfile.h"
#include <stdio.h>
#include <stdint.h>

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
    char * msg = "ello, Tempfiles?";
    dfwrite(msg, strlen(msg), f);
    dungetc('M', f);
    dfwrite("!", 1, f);
    dungetc('M', f);
    dfseek(f, 0, SEEK_SET);
    char buf[256];
    dungetc(dfgetc(f), f);
    dungetc('H', f);
    if(!dfgets(buf, sizeof buf, f))
      return -1;
    dputs(buf);
    dfclose(f);
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
    dfclose(f);
  }
  {
    DFILE * f = dfmemopen(NULL, 64, "r+");
    char * msg = "Hello, fmemopen?";
    dfputs(msg, f);
    dfseek(f, -1, SEEK_CUR);
    dfputc('!', f);
    dfseek(f, 0, SEEK_SET);
    char buf2[64];
    if(!dfgets(buf2, sizeof buf2, f))
      return -1;
    dputs(buf2);
    dfclose(f);
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

  {
    d_printf("Hell%c, %s!\n", 'o', "printf");
    d_printf("Hello, printf numbers! %+i %+u %i\n", 12, -2, -12);
    d_printf("Hey %x, wassup % X. %o %b\n", 0xbabe, 0xBAB3, 0776, 10);
    d_printf("%r.f % r.f %+r.f %r.f\n", 1234.0, 1.234, 0.1234, -12340.0);
    d_printf("%f %f %F\n", 0.0, -1 / 0.0, 0 / 0.0);
    d_printf("%rf %rf\n", 0.3, 0.1 + 0.2);
    d_printf("%w32u\n", (uint32_t)1);
    d_printf("%hhu\n", 257);
    d_printf("%#x %#B %#o %#o\n", 0x1337, 10, 0, 0777);
    d_printf("%10u %010u %-10u %01u\n", 1, 12, 23, 123);
    d_printf("%#10b %#010x\n", 10, 0x1337);
    d_printf("%#*.*x\n", 21, 10, 0x1337);
    d_printf("%.2f\n", 1.995);
    d_printf("%.2f\n", 1.985);
    d_printf("%.6f\n", 1.995);
    d_printf("%.1f\n", 1.94);
    d_printf("%.1f\n", 1.96);
    d_printf("%.6f\n", 133700.0);
    d_printf("%.f\n", 1.5);
    d_printf("%#.f\n", 1.5);
    d_printf("%r#f\n", 12340.0);
    d_printf("%r#e\n", 100.0);
    d_printf("%re\n", 100.0);
    d_printf("%e\n", 100.0);
    d_printf("%18p\n", (void*)0);
    d_printf("%.16p\n", (void*)main);
    int n;
    d_printf("hello%n", &n);
    d_printf(" %d\n", n);
    short arr[] = { 1, 5  };
    d_printf("hayo%hn", arr);
    d_printf(" %hd %hd\n", arr[0], arr[1]);
    d_printf("%g %g %g %g %.g\n", 0.1234567, 1234567.0, 123456.0, 0.00001234567, 1.234567);
    d_printf("%g %#g\n", 10000000.0, 10000000.0);

    d_printf("%a\n", 0.0);
    d_printf("%a\n", 1.0);
    d_printf("%a\n", 1.75);
    d_printf("%a\n", 0.1 + 0.2);
    d_printf("%a\n", 0.3f);
    d_printf("%m\n");
    errno = EINVAL;
    d_printf("%m\n");
    errno = 0;
    d_printf("%.f\n", 1.0);
    d_printf("%r8.6x\n", 0x1337);
    d_printf("%r.6f\n", 0.1337);
  }
  
  dfputs("this should show up\nbut not\nthis", dstdout);
  // fast exit to stop the this from flushing. change to exit(0) to validate
  // flush on exit
  _exit(0);
}
