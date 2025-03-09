# DFILE Stdio Alternative

An alternate implementation of stdio that brings Linux goodies and fixes to Windows. At the moment, only mingw is supported for windows and gcc for linux.

# Why???

The main reason I implemented this is because `tmpfile()` was broken on Windows with Windows Vista, and the workaround is excruciatingly bad performance. I need something _like_ `tmpfile()` in order to implement `open-output-string` in my lisp runtime. It came to a head when porting a little slither.io clone I wrote to windows, and seeing the debug text routine taking an average of 10 miliseconds with frequent catastrophic stutters.

Truly one of the wackiest yak shaves I've done yet.

The killer feature of DFILE is the `d_fopencookie()` function, which is like the glibc function, except you can use it on windows!

And I needed it to implement the `dstrfile()` function, which returns a DFILE stream, just as `tmpfile()` returns a FILE stream. Except `dstrfile()` does not write to a real tempfile, but instead flushes to an unrolled linked list of pages 4096 chars long.

In addition, `dtmpfile()` is also not broken on Windows (though as noted above, it is catastrophically slow), and DFILEs use unix style line endings for everything. So, `dfseek()` is not broken, there is no need for binary file modes, and ugly carriage returns don't infect your files. Likewise, DFILE doesn't use locales, so no random chaos happening upon print or scan. For localization, stdio is the incorrect choice anyway.

# How to use DFILE

Replace FILE* with DFILE* throughout your code. All dfile functions and variables have the same as stdio, just prefixed with d_. So `d_puts("Hello, world!", d_stdout)`.

Most of the function calls are the same: the only major difference is dfread and dfwrite, which have the simplified interface. Also, dftell and dfseek use off64\_t instead of off\_t, so that isn't broken on windows. Definitions of off64\_t and ssize\_t are provided.

```
int dfread(void * ptr, int ct, DFILE * f);
int dfwrite(void * ptr, int ct, DFILE * f);
```

The `dungetc` function supports the minimum necessary to implement scanf wthout flushing at the end and be posix compliant, 2 ungets.

Bonus: d\_fmemopen accepts a '0' flag which causes it to ignore writes and read 0s past the end of a buffer, similar to "robust buffer access" on desktop GPUs. For example:
```
char buf[5];
DFILE * f = dfmemopen(buf, sizeof buf, "w0+");
char * msg = "Hello, Dweebs!";
int nchars = dfwrite(msg, strlen(msg), f);
// writes Hello to the buffer, but reports 14 chars written.
char buf2[32];
dfseek(f, 0, SEEK_SET);
nchars = dfread(buf2, sizeof buf2, f);
// reads 32 chars into buf2, reading Hello\0\0\0\0...
```
This may be how snprintf is implemented :)

# Unimplemented functionality

Things my yak shave didn't require

* "a" and "a+" mode
* setbuf
* setvbuf
* fgetpos
* fsetpos
* scanf and friends

* flockfile
* ftrylockfile
* funlockfile
* all stdio\_unlocked
ie DFILE is thread unsafe

printf custom formatter

# Printf Implementation Matrix

Notably, d\_printf supports wN and wfN formats. Free from PRIu64 at last.

d\_printf also adds a 'roundtrip' flag, which *always* prints enough floating point digits to roundtrip the number. When roundtrip is set, the precision defaults to 0 instead of 6. Example usage: `d_printf("%rf %rf\n", 0.3, 0.1 + 0.2);` and `d_printf("%r.6f\n", 0.3);`

| format | + | - | \# | space | 0 | r | width | precision | size |
|--------|---|---|----|-------|---|---|-------|-----------|------|
| d      |yes|yes| -  | yes   |yes| - | yes   | yes       | yes  |
| u      | - |yes| -  |  -    |yes| - | yes   | yes       | yes  |
| x      | - |yes|yes |  -    |yes| - | yes   | yes       | yes  |
| o      | - |yes|yes |  -    |yes| - | yes   | yes       | yes  |
| b      | - |yes|yes |  -    |yes| - | yes   | yes       | yes  |
| f      |yes|yes|yes | yes   |yes|yes| yes   | yes       | yes  |
| g      |yes|yes|yes | yes   |yes|yes| yes   | yes       | yes  |
| a      |yes|yes|yes | yes   |yes|yes| yes   | yes       | yes  |
| e      |yes|yes|yes | yes   |yes|yes| yes   | yes       | yes  |
| c      | - |yes| -  |  -    | - | - | yes   |  -        | no   |
| s      | - |yes| -  |  -    | - | - | yes   |  -        | no   |
| m      | - |yes| -  |  -    | - | - | yes   |  -        |  -   |
| p      | - |yes| -  |  -    | - | - | yes   | yes       |  -   |
| n      | - | - | -  |  -    | - | - |  -    |  -        | yes  |

Widechar strings are not supported because a basic implementation would invite locales, and I'm not ready for an advanced implementation.
