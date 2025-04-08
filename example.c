#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "dfile.h"
#include <stdint.h>
#include <assert.h>

int main() {
  {
    char * msg = "Hello, fwrite!\n";
    d_fwrite(msg, strlen(msg), dstdout);
  }
  {
    DFILE * fi = d_fopen("testfile", "r");
    char buf[256];
    if(!d_fgets(buf, sizeof buf, fi))
      return -1;
    d_fputs(buf, dstdout);
    assert(!d_fclose(fi));
  }
  {
    char buf[32];
    d_ungetc('\n', dstdin);
    d_ungetc('Q', dstdin);
    d_fgets(buf, sizeof buf, dstdin);
    d_fputs(buf, dstdout);
  }
  {
    DFILE * f = d_tmpfile();
    char * msg = "ello, Tempfiles?";
    d_fwrite(msg, strlen(msg), f);
    d_ungetc('M', f);
    d_fwrite("!", 1, f);
    d_ungetc('M', f);
    d_fseek(f, 0, D_SEEK_SET);
    char buf[256];
    d_ungetc(d_fgetc(f), f);
    d_ungetc('H', f);
    if(!d_fgets(buf, sizeof buf, f))
      return -1;
    d_puts(buf);
    assert(!d_fclose(f));
  }
  {
    DFILE * f = d_strfile();
    char * msg = "Hello, Strings!";
    d_fwrite(msg, strlen(msg), f);
    d_fseek(f, 0, D_SEEK_SET);
    char buf[20];
    if(!d_fgets(buf, sizeof buf, f))
      return -1;
    d_puts(buf);
    assert(!d_fclose(f));
  }
  {
    DFILE * f = d_fmemopen(NULL, 64, "w+");
    char * msg = "Hello, fmemopen?";
    d_fputs(msg, f);
    d_fseek(f, -1, D_SEEK_CUR);
    d_fputc('!', f);
    d_fseek(f, 0, D_SEEK_SET);
    char buf2[64];
    if(!d_fgets(buf2, sizeof buf2, f))
      return -1;
    d_puts(buf2);
    assert(!d_fclose(f));
  }
  {
#ifdef _WIN64
    DFILE * f = d_popen("dir", "r");
#else
    DFILE * f = d_popen("ls", "r");
#endif
    char buf[1024], *ptr = buf;
    int nbytes;
    do {
      nbytes = d_fread(ptr, buf + sizeof buf - ptr, f);
      if(nbytes > 0)
        ptr += nbytes;
    } while(nbytes > 0);
    if(ptr < buf + sizeof buf)
      *ptr = 0;
    else
      buf[sizeof buf - 1] = 0;
    d_fputs(buf, dstdout);
    d_fflush(dstdout);
    if(d_pclose(f))
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
    d_printf("%m\n");
    d_printf("%.f\n", 1.0);
    d_printf("%r8.6x\n", 0x1337);
    d_printf("%r.6f\n", 0.1337);
  }

  {
    char buf[5];
    DFILE * f = d_fmemopen(buf, sizeof buf, "w0+");
    char * msg = "Mello, Nerds!";
    int nchars_written = d_fwrite(msg, strlen(msg), f);
    char buf2[40];
    d_fseek(f, 0, D_SEEK_SET);
    d_fputc('H', f);
    d_fseek(f, 0, D_SEEK_SET);
    int nchars_read = d_fread(buf2, sizeof buf2, f);
    d_printf("%s, Friends! Wrote %d. Read %d. But the buffer is only %zu.\n", buf2, nchars_written, nchars_read, sizeof buf);
    assert(!d_fclose(f));
  }

  {
    int ret;
    char buf[5];
    ret = d_snprintf(buf, sizeof buf, "%c!", 'X');
    d_printf("Says %s, wrote %d\n", buf, ret);
    ret = d_snprintf(buf, sizeof buf, "%s!", "Hello, World");
    d_printf("Says %s, tried to write %d\n", buf, ret);
  }

  {
    DFILE * f = d_fmemopen(NULL, 0, "w0+");
    char * msg = "Into the void...";
    int nchars_written = d_fwrite(msg, strlen(msg), f);
    char buf[64];
    memset(buf, 1, sizeof buf);
    int nchars_read = d_fread(buf, sizeof buf, f);
    for(int i = 0; i < sizeof buf; i++) {
      if(buf[i] != 0)
        return -1;
    }
    d_printf("Sent %d chars into the void and read %d zeroes\n", nchars_written, nchars_read);
    assert(!d_fclose(f));
  }

  {
    char * buf;
    size_t len;
    DFILE * f = d_open_memstream(&buf, &len);
    d_fputs("Mayo", f);
    d_fseek(f, 0, D_SEEK_SET);
    d_fputc('H', f);
    d_fseek(f, 2, D_SEEK_END);
    d_fseek(f, -2, D_SEEK_CUR);
    d_fputc('!', f);
    assert(!d_fclose(f));
    d_printf("Stream says %s, %zu chars\n", buf, len);
    d_free(buf);
  }

  {
    char * buf;
    int ret = d_asprintf(&buf, "Hayo, %s!", "friend");
    d_printf("Wrote '%s' to buf, strlen %d, ret %d\n", buf, strlen(buf), ret);
    d_free(buf);
  }

  {
    char * str = "Hello, nobody at all";
    DFILE * f = d_open_strstream(str);
    char buf[99];
    d_fscanf(f, "%s", buf);
    d_printf("%s strstreams!\n", buf);
    assert(!d_fclose(f));
  }

  {
    char * str = "Hello, nobody at all";
    char buf[99];
    d_sscanf(str, "%s", buf);
    d_printf("%s sscanf!\n", buf);
  }

  {
    d_printf("type an int:\n");
    int nchars = 0;
    double d = 0;
    char buf[256];
    int nfields = d_scanf("%lf ;%n", &d, &nchars);
    d_printf("result: %r.1f\n", d);
    //int nfields = d_scanf("%[1-9] ;%n", &buf, &nchars);
    //d_printf("result: %s\n", buf);
    d_printf("scanned %d fields and %d chars\n", nfields, nchars);
  }
  
  static char linebuffer[2];
  d_setvbuf(dstdout, linebuffer, D_IOFBF, 2);
  d_setlinebuf(dstdout);
  d_fputs("this should show up\nbut not\nthis", dstdout);
  // fast exit to stop the this from flushing. change to exit(0) to validate
  // flush on exit
  _exit(0);
}
