#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "dfile.h"
#include "dragonbox.h"

enum { PRINT_INCOMPLETE, PRINT_MALFORMED, PRINT_PERCENT, PRINT_CHAR, PRINT_STRING, PRINT_INT, PRINT_UINT, PRINT_BINARY, PRINT_OCTAL, PRINT_HEX, PRINT_DOUBLE };
enum { PRINT_ALLCAPS = 1, PRINT_LEFT_JUSTIFY = 2, PRINT_SIGN = 4, PRINT_HIDDEN_SIGN = 8, PRINT_ALTER = 16, PRINT_ZERO_EXTEND = 32, };

enum { PRINT_SHORTSHORT, PRINT_SHORT, PRINT_WORD, PRINT_LONG, PRINT_LONGLONG, PRINT_SIZE, PRINT_MAX, PRINT_PTRDIFF, PRINT_EXACT, PRINT_FAST };

typedef struct print_specifier {
  int print_kind;
  int kind_width;
  int kind_exact_width;
  int flags;
  int field_width;
  int precision;
  int chars_consumed;
} print_specifier;

static int scan_unsigned(char const ** pfmt) {
  char const * fmt = *pfmt;
  char c;
  int u = 0;
  while((c = *fmt)) {
    if('0' <= c && c <= '9')
      u = 10*u + c - '0';
    else
      break;
    fmt++;
  }
  *pfmt = fmt;
  return u;
}

static print_specifier parse_print_specifier(char const * fmt, va_list* args) {
  int print_kind = PRINT_INCOMPLETE;
  int flags = 0;
  char const * start = fmt;
  char c;
  bool parsing_flags = true;
  while((c = *fmt)) {
    switch(c) {
    case '-':
      flags |= PRINT_LEFT_JUSTIFY;
    case '+':
      flags |= PRINT_SIGN;
    case ' ':
      flags |= PRINT_HIDDEN_SIGN;
    case '#':
      flags |= PRINT_ALTER;
    case '0':
      flags |= PRINT_ZERO_EXTEND;
      break;
    default:
      parsing_flags = false;
    }
    if(!parsing_flags)
      break;
    fmt++;
  }

  int field_width = -1;
  if('0' <= *fmt && *fmt <= '9')
    field_width = scan_unsigned(&fmt);

  int precision = -1;
  if(fmt[0] == '.' && '0' <= fmt[1] && fmt[1] <= '9') {
    fmt++;
    precision = scan_unsigned(&fmt);
  }

  int kind_width = PRINT_WORD;
  int kind_exact_width = 8;
  int skips = 1;
  switch(*fmt) {
    case 'h':
      if(fmt[1] == 'h') {
        kind_width = PRINT_SHORTSHORT;
        skips = 2;
      } else {
        kind_width = PRINT_SHORT;
      }
      break;
    case 'l':
      if(fmt[1] == 'l') {
        kind_width = PRINT_LONGLONG;
        skips = 2;
      } else {
        kind_width = PRINT_LONG;
      }
      break;
    case 'z':
      kind_width = PRINT_SIZE;
      break;
    case 'j':
      kind_width = PRINT_MAX;
      break;
    case 't':
      kind_width = PRINT_PTRDIFF;
      break;
    case 'w':
      if(fmt[1] == 'f') {
        kind_width = PRINT_FAST;
        skips = 2;
      } else {
        kind_width = PRINT_EXACT;
      }
      char const * end = fmt + skips;
      if('0' <= *end && *end <= '9') {
        kind_exact_width = scan_unsigned(&end);
        skips = end - fmt;
      }
      break;
    case 'L':
      kind_width = PRINT_LONG;
      break;
    case 'H':
      kind_width = PRINT_SHORT;
      break;
    case 'D':
      if(fmt[1] == 'D') {
        kind_width = PRINT_LONGLONG;
        skips = 2;
      }
      break;
    default:
      skips = 0;
  }
  fmt += skips;

  switch(*fmt++) {
    case '%':
      print_kind = PRINT_PERCENT;
      break;
    case 'c':
      print_kind = PRINT_CHAR;
      break;
    case 's':
      print_kind = PRINT_STRING;
      break;
    case 'i':
    case 'd':
      print_kind = PRINT_INT;
      break;
    case 'u':
      print_kind = PRINT_UINT;
      break;
    case 'o':
      print_kind = PRINT_OCTAL;
      break;
    case 'b':
    case 'B':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      print_kind = PRINT_BINARY;
      break;
    case 'x':
    case 'X':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      print_kind = PRINT_HEX;
      break;
    case 'f':
    case 'F':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      print_kind = PRINT_DOUBLE;
      break;
    default:
      print_kind = PRINT_MALFORMED;
      break;
  }
  return (print_specifier) {
    .print_kind = print_kind,
    .kind_width = kind_width,
    .kind_exact_width = kind_exact_width,
    .precision = precision,
    .field_width = field_width,
    .flags = flags,
    .chars_consumed = fmt - start,
  };
}

static int u64toa10(uint64_t u, char buf[static 21]) {
  if(u == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return 1;
  }
  int idx = 0;
  while(u) {
    buf[idx++] = (u % 10) + '0';
    u /= 10;
  }
  buf[idx] = '\0';
  
  char * start = buf;
  char * end = buf + idx - 1;
  while(start < end) {
    char tmp = *start;
    *start = *end;
    *end = tmp;
    start++;
    end--;
  }
  return idx;
}
static int u64toa(uint64_t u, char buf[static 65], int base, bool allcaps) {
  if(u == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return 1;
  }
  int idx = 0;
  char a = allcaps ? 'A' : 'a';
  while(u) {
    int rem = (u % base);
    buf[idx++] = rem < 10 ? rem + '0' : rem - 10 + a;
    u /= base;
  }
  buf[idx] = '\0';
  
  char * start = buf;
  char * end = buf + idx - 1;
  while(start < end) {
    char tmp = *start;
    *start = *end;
    *end = tmp;
    start++;
    end--;
  }
  return idx;
}

static int dtoaz(double d, char buf[static 21], int * e) {
  dragonbox dragon = dtodragon(d);
  uint64_t u = dragon.significand;
  *e = dragon.exponent;
  return u64toa10(u, buf);
}

static int print_double(DFILE * f, print_specifier specifier, double d) {
  char buf[21];
  int ret = 0;
  int e;
  int len;

  if(d < 0) {
    dfputc('-', f);
    ret++;
  }

  uint64_t u;
  memcpy(&u, &d, sizeof d);
  enum { EXP = (1 << 11) - 1, MANT = (1ull << 52) - 1, };
  if(((u >> 52) & EXP) == EXP) {
    if(u & MANT) {
      dfputs("nan", f);
      ret += 3;
      return ret;
    } else {
      dfputs("inf", f);
      ret += 3;
      return ret;
    }
  }

  if(d == 0) {
    len = 1;
    buf[0] = '0';
    buf[1] = '\0';
    e = 0;
  } else {
    len = dtoaz(d, buf, &e);
  }
  if(-e >= len) {
    dfputs("0.", f);
    ret += 2;
  }
  while(-e > len) {
    dfputc('0', f);
    e++;
    ret++;
  }
  for(int i = 0; i < len; i++) {
    if(i - len == e && i != 0) {
      dfputc('.', f);
      ret++;
    }
    dfputc(buf[i], f);
    ret++;
  }
  while(e > 0) {
    dfputc('0', f);
    e--;
    ret++;
  }
  return ret;
}

uint64_t read_va_uint(print_specifier specifier, va_list* args) {
  uint64_t u = ~0ull;
  switch(specifier.kind_width) {
    case PRINT_SHORTSHORT:
      u = (unsigned char)va_arg(*args, int);
      break;
    case PRINT_SHORT:
      u = (unsigned short)va_arg(*args, int);
      break;
    case PRINT_WORD:
      u = va_arg(*args, unsigned int);
      break;
    case PRINT_LONG:
      u = va_arg(*args, unsigned long);
      break;
    case PRINT_LONGLONG:
      u = va_arg(*args, unsigned long long);
      break;
    case PRINT_SIZE:
      u = va_arg(*args, size_t);
      break;
    case PRINT_MAX:
      u = va_arg(*args, uintmax_t);
      break;
    case PRINT_PTRDIFF:
      u = va_arg(*args, ptrdiff_t);
      break;
    case PRINT_EXACT:
      switch(specifier.kind_exact_width) {
        case 8:
          u = (uint8_t)va_arg(*args, typeof(1+(uint8_t)1));
          break;
        case 16:
          u = (uint16_t)va_arg(*args, typeof(1+(uint16_t)1));
          break;
        case 32:
          u = (uint32_t)va_arg(*args, typeof(1+(uint32_t)1));
          break;
        case 64:
          u = (uint64_t)va_arg(*args, typeof(1+(uint64_t)1));
          break;
      }
      break;
    case PRINT_FAST:
      switch(specifier.kind_exact_width) {
        case 8:
          u = (uint_fast8_t)va_arg(*args, typeof(1+(uint_fast8_t)1));
          break;
        case 16:
          u = (uint_fast16_t)va_arg(*args, typeof(1+(uint_fast16_t)1));
          break;
        case 32:
          u = (uint_fast32_t)va_arg(*args, typeof(1+(uint_fast32_t)1));
          break;
        case 64:
          u = (uint_fast64_t)va_arg(*args, typeof(1+(uint_fast64_t)1));
          break;
      }
      break;
  }
  return u;
}
int64_t read_va_int(print_specifier specifier, va_list* args) {
  int64_t i = ~0ull;
  switch(specifier.kind_width) {
    case PRINT_SHORTSHORT:
      i = (signed char)va_arg(*args, int);
      break;
    case PRINT_SHORT:
      i = (short)va_arg(*args, int);
      break;
    case PRINT_WORD:
      i = va_arg(*args, int);
      break;
    case PRINT_LONG:
      i = va_arg(*args, long);
      break;
    case PRINT_LONGLONG:
      i = va_arg(*args, long long);
      break;
    case PRINT_SIZE:
      i = va_arg(*args, size_t);
      break;
    case PRINT_MAX:
      i = va_arg(*args, intmax_t);
      break;
    case PRINT_PTRDIFF:
      i = va_arg(*args, ptrdiff_t);
      break;
    case PRINT_EXACT:
      switch(specifier.kind_exact_width) {
        case 8:
          i = (int8_t)va_arg(*args, typeof(1+(int8_t)1));
          break;
        case 16:
          i = (int16_t)va_arg(*args, typeof(1+(int16_t)1));
          break;
        case 32:
          i = (int32_t)va_arg(*args, typeof(1+(int32_t)1));
          break;
        case 64:
          i = (int64_t)va_arg(*args, typeof(1+(int64_t)1));
          break;
      }
      break;
    case PRINT_FAST:
      switch(specifier.kind_exact_width) {
        case 8:
          i = (int_fast8_t)va_arg(*args, typeof(1+(int_fast8_t)1));
          break;
        case 16:
          i = (int_fast16_t)va_arg(*args, typeof(1+(int_fast16_t)1));
          break;
        case 32:
          i = (int_fast32_t)va_arg(*args, typeof(1+(int_fast32_t)1));
          break;
        case 64:
          i = (int_fast64_t)va_arg(*args, typeof(1+(int_fast64_t)1));
          break;
      }
      break;
  }
  return i;
}

static int print_uint10(DFILE * f, print_specifier specifier, va_list* args) {
  uint64_t u = read_va_uint(specifier, args);
  char buf[21];
  int len = u64toa10(u, buf);
  return dfwrite(buf, len, f);
}
static int print_uint(DFILE * f, print_specifier specifier, va_list* args, int base, bool allcaps) {
  uint64_t u = read_va_uint(specifier, args);
  char buf[65];
  int len = u64toa(u, buf, base, allcaps);
  return dfwrite(buf, len, f);
}
static int print_int(DFILE * f, print_specifier specifier, va_list* args) {
  int64_t i = read_va_int(specifier, args);
  bool neg = i < 0;
  uint64_t u = neg ? -i : i;
  char buf[22];
  int len = u64toa10(u, buf);
  if(neg) {
    memmove(buf + 1, buf, sizeof buf - 1);
    buf[0] = '-';
    len += 1;
  }
  return dfwrite(buf, len, f);
}

static int print_format(DFILE * f, char const ** pfmt, va_list* args) {
  print_specifier specifier = parse_print_specifier(*pfmt, args);
  *pfmt = *pfmt + specifier.chars_consumed;
  switch(specifier.print_kind) {
    case PRINT_PERCENT: {
      if(dfputc('%', f) < 0)
        return -1;
      return 1;
    }
    case PRINT_CHAR: {
      char c = va_arg(*args, int);
      if(dfputc(c, f) < 0)
        return -1;
      return 1;
    }
    case PRINT_STRING: {
      char * str = va_arg(*args, char*);
      int len = strlen(str);
      int ret = dfwrite(str, len, f);
      if(ret < 0)
        return -1;
      return ret;
    }
    case PRINT_BINARY:
      return print_uint(f, specifier, args, 2, false);
    case PRINT_OCTAL:
      return print_uint(f, specifier, args, 8, false);
    case PRINT_HEX:
      return print_uint(f, specifier, args, 16, specifier.flags & PRINT_ALLCAPS);
    case PRINT_UINT:
      return print_uint10(f, specifier, args);
    case PRINT_INT:
      return print_int(f, specifier, args);
    case PRINT_DOUBLE:
      return print_double(f, specifier, va_arg(*args, double));
    default:
      return -1;
  }
}

#ifdef _WIN64
#define VA_POINTER(x) (&x)
#else
#define VA_POINTER(x) ((va_list*)x)
#endif
static int  dvfprintf_impl(DFILE * f, char  const * fmt, va_list *args) {
  int printed = 0;
  char c;
  while((c = *fmt++)) {
    switch(c) {
      case '%': {
        int ret = print_format(f, &fmt, args);
        if(ret < 0) {
          return -1;
        }
        printed += ret;
        break;
      }
      default:
        if(dfputc(c, f) < 0) {
          return -1;
        }
        printed++;
      }
  }
  return printed;
}
int dvfprintf(DFILE * f, char const * fmt, va_list args) {
  return dvfprintf_impl(f, fmt, VA_POINTER(args));
}

int dfprintf(DFILE * f, char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = dvfprintf(f, fmt, args);
  va_end(args);
  return ret;
}
int deeprintf(char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = dvfprintf(dstdout, fmt, args);
  va_end(args);
  return ret;
}
int dvprintf(char const * fmt, va_list args) {
  int ret = dvfprintf(dstdout, fmt, args);
  return ret;
}
