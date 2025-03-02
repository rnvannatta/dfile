#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "dfile.h"
#include "dragonbox.h"

enum { PRINT_INCOMPLETE, PRINT_MALFORMED, PRINT_PERCENT, PRINT_CHAR, PRINT_STRING, PRINT_INT, PRINT_UINT, PRINT_BINARY, PRINT_OCTAL, PRINT_HEX, PRINT_DOUBLE, PRINT_EXPONENT, PRINT_GENERAL, PRINT_HEXPONENT, PRINT_POINTER, PRINT_TELL, PRINT_ERROR };
enum { PRINT_ALLCAPS = 1, PRINT_LEFT_JUSTIFY = 2, PRINT_SIGN = 4, PRINT_SPACE = 8, PRINT_ALTER = 16, PRINT_ZERO_EXTEND = 32, PRINT_ROUNDTRIP = 64 };

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
      flags |= PRINT_SPACE;
    case '#':
      flags |= PRINT_ALTER;
    case '0':
      flags |= PRINT_ZERO_EXTEND;
      break;
    case 'r':
      flags |= PRINT_ROUNDTRIP;
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
  else if(*fmt == '*') {
    fmt++;
    field_width = va_arg(*args, int);
  }

  bool precision_set = false;
  int precision = 0;
  if(fmt[0] == '.') {
    precision_set = true;
    if('0' <= fmt[1] && fmt[1] <= '9') {
      fmt++;
      precision = scan_unsigned(&fmt);
    } else if(fmt[1] == '*') {
      fmt+=2;
      precision = va_arg(*args, int);
    } else if(fmt[1] == '#') {
      fmt+=2;
      precision = 0;
      flags |= PRINT_ROUNDTRIP;
    } else {
      fmt++;
      precision = 0;
    }
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
    default:
      skips = 0;
  }
  fmt += skips;

  switch((c = *fmt++)) {
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
      if(!(flags & PRINT_ROUNDTRIP) && !precision_set) precision = 6;
      print_kind = PRINT_DOUBLE;
      break;
    case 'g':
    case 'G':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      if(!precision_set) precision = 6;
      print_kind = PRINT_GENERAL;
      break;
    case 'a':
    case 'A':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      if(!precision_set) flags |= PRINT_ROUNDTRIP;
      print_kind = PRINT_HEXPONENT;
      break;
    case 'e':
    case 'E':
      if(c < 'a') flags |= PRINT_ALLCAPS;
      if(!(flags & PRINT_ROUNDTRIP) && !precision_set) precision = 6;
      print_kind = PRINT_EXPONENT;
      break;
    case 'p':
      print_kind = PRINT_POINTER;
      break;
    case 'n':
      print_kind = PRINT_TELL;
      break;
    case 'm':
      print_kind = PRINT_ERROR;
      break;
    default:
      print_kind = PRINT_MALFORMED;
      break;
  }

  // unsetting flags specified to be unset in situations
  if(flags & PRINT_LEFT_JUSTIFY)
    flags &= ~PRINT_ZERO_EXTEND;
  if(precision_set) {
    switch(print_kind) {
      case PRINT_BINARY:
      case PRINT_OCTAL:
      case PRINT_INT:
      case PRINT_UINT:
      case PRINT_HEX:
      case PRINT_POINTER:
        flags &= ~PRINT_ZERO_EXTEND;
    }
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

static uint64_t truncu(uint64_t u, int * e, int precision) {
  if(*e > 0)
    return u;
  int last = 0;
  int lastlast = 0;
  while(-*e > precision) {
    lastlast += last;
    last = u % 10;
    u = u / 10;
    (*e)++;
  }
  if(last > 5 || (last == 5 && (lastlast || ((u % 10) & 1))))
    u += 1;
  return u;
}

static int dtoaz(dragonbox dragon, char buf[static 2048], int * decimal) {
  char * og = buf;
  uint64_t u = dragon.significand;
  int e = dragon.exponent;
  char tmp[21];
  int ret = 0;
  int len = u64toa10(u, tmp);
  *decimal = 0;
  if(-e >= len) {
    *buf++ = '0';
    *buf++ = '.';
    *decimal = -e;
    ret = 2;
  }
  while(-e > len) {
    *buf++ = '0';
    e++;
    ret++;
  }
  for(int i = 0; i < len; i++) {
    if(i - len == e && i != 0) {
      *buf++ = '.';
      *decimal = -e;
      ret++;
    }
    *buf++ = tmp[i];
    ret++;
  }
  while(e > 0) {
    *buf++ = '0';
    e--;
    ret++;
  }

  return ret;
}

static int print_number(DFILE * f, const char prefix[], const char number[], int len, print_specifier specifier, int nzeroes) {
  int ret = 0;
  int prefixlen = strlen(prefix);
  if(prefixlen && (specifier.flags & PRINT_ZERO_EXTEND))
    dfwrite(prefix, prefixlen, f);
  ret += prefixlen;

  char fill = specifier.flags & PRINT_ZERO_EXTEND ? '0' : ' ';
  int fill_len = specifier.field_width - len - strlen(prefix) - nzeroes;
  if(!(specifier.flags & PRINT_LEFT_JUSTIFY)) {
    for(int i = 0; i < fill_len; i++)
      dfputc(fill, f);
  }
  ret += fill_len > 0 ? fill_len : 0;

  if(prefixlen && !(specifier.flags & PRINT_ZERO_EXTEND))
    dfwrite(prefix, prefixlen, f);

  while(nzeroes--)
    dfputc('0', f);
  ret += nzeroes;

  dfwrite(number, len, f);
  ret += len;

  if(specifier.flags & PRINT_LEFT_JUSTIFY) {
    for(int i = 0; i < fill_len; i++)
      dfputc(fill, f);
  }

  return ret;
}

static int get_exponent(dragonbox dragon, int * dragonexp) {
  int exponent = dragon.exponent;
  int dragon_exponent = 0;
  uint64_t u = dragon.significand;
  while(u > 9) {
    u /= 10;
    exponent++;
    dragon_exponent--;
  }
  if(dragonexp)
    *dragonexp = dragon_exponent;
  return exponent;
}

static int print_double(DFILE * f, print_specifier specifier, va_list * args) {
  double d;
  switch(specifier.kind_width) {
  case PRINT_LONG:
    d = va_arg(*args, long double);
    break;
  default:
    d = va_arg(*args, double);
  }
  char buf[2048];
  int len;

  char sign[2] = {0};
  if(d < 0)
    sign[0] = '-';
  else if(specifier.flags & PRINT_SIGN)
    sign[0] = '+';
  else if(specifier.flags & PRINT_SPACE)
    sign[0] = ' ';

  int decimal = 0;
  char suffix[32] = "";

  uint64_t u;
  memcpy(&u, &d, sizeof d);
  enum { EXP = (1 << 11) - 1, MANT = (1ull << 52) - 1, };
  if(((u >> 52) & EXP) == EXP) {
    if(u & MANT) {
      len = 3;
      memcpy(buf, specifier.flags & PRINT_ALLCAPS ? "NAN" : "nan", 4);
      specifier.flags |= PRINT_ROUNDTRIP;
      specifier.precision = 0;
    } else {
      len = 3;
      memcpy(buf, specifier.flags & PRINT_ALLCAPS ? "INF" : "inf", 4);
      specifier.flags |= PRINT_ROUNDTRIP;
      specifier.precision = 0;
    }
    specifier.flags &= ~PRINT_ZERO_EXTEND;
    specifier.flags &= ~PRINT_ALTER;
  }
  else if(d == 0) {
    len = 0;
    buf[len++] = '0';
    decimal = 0;
    buf[len] = '\0';
  } else {
    dragonbox dragon = dtodragon(d);
    bool print_exponent = specifier.print_kind == PRINT_EXPONENT;
    if(specifier.print_kind == PRINT_GENERAL) {
      int exponent = get_exponent(dragon, NULL);
      if(-4 <= exponent && exponent < specifier.precision) {
        print_exponent = false;
        specifier.precision = specifier.precision - exponent - 1;
      } else {
        print_exponent = true;
        specifier.precision = specifier.precision - 1;
      }
      if(specifier.precision < 0) specifier.precision = 0;
    }

    if(print_exponent) {
      int exponent = get_exponent(dragon, &dragon.exponent);
      decimal = -dragon.exponent;
      char * ptr = suffix;
      *ptr++ = specifier.flags & PRINT_ALLCAPS ? 'E' : 'e';
      *ptr++ = exponent >= 0 ? '+' : '-';
      char buf[21];
      if(exponent < 0) exponent = -exponent;
      int len = u64toa10(exponent, buf);
      if(len == 1)
        *ptr++ = '0';
      strcpy(ptr, buf);
      specifier.field_width -= strlen(suffix);
    }
    if(!(specifier.flags & PRINT_ROUNDTRIP))
      dragon.significand = truncu(dragon.significand, &dragon.exponent, specifier.precision);
    len = dtoaz(dragon, buf, &decimal);
  }
  // if the decimal hasn't been printed yet
  //   then if we force the decimal, or we have trailing zeroes to print
  //   and general doesn't print trailing zeroes
  bool print_trailing_zeroes = ((specifier.flags & PRINT_ALTER) || (specifier.print_kind != PRINT_GENERAL && specifier.precision));
  if(decimal == 0 && print_trailing_zeroes) {
    buf[len] = '.';
    len++;
  }
  int nzeroes = 0;
  if(print_trailing_zeroes && decimal < specifier.precision) {
    nzeroes = specifier.precision - decimal;
    specifier.field_width -= nzeroes;
  }
  int ret = print_number(f, sign, buf, len, specifier, 0);
  while(nzeroes--) {
    dfputc('0', f);
    ret++;
  }
  if(*suffix) {
    int len = strlen(suffix);
    dfwrite(suffix, len, f);
    ret += len;
  }
  return ret;
}
static int print_hexponent(DFILE * f, print_specifier specifier, va_list * args) {
  double d;
  switch(specifier.kind_width) {
  case PRINT_LONG:
    d = va_arg(*args, long double);
    break;
  default:
    d = va_arg(*args, double);
  }
  char prefix[8] = {0};
  if(d < 0)
    prefix[0] = '-';
  else if(specifier.flags & PRINT_SIGN)
    prefix[0] = '+';
  else if(specifier.flags & PRINT_SPACE)
    prefix[0] = ' ';

  int decimal = 0;
  int len = 0;
  char buf[64] = "";
  char suffix[32] = "";
  uint64_t u;
  memcpy(&u, &d, sizeof d);
  enum { EXP = (1 << 11) - 1, MANT = (1ull << 52) - 1, };
  if(((u >> 52) & EXP) == EXP) {
    if(u & MANT) {
      len = 3;
      memcpy(buf, specifier.flags & PRINT_ALLCAPS ? "NAN" : "nan", 4);
      specifier.flags |= PRINT_ROUNDTRIP;
      specifier.precision = 0;
    } else {
      len = 3;
      memcpy(buf, specifier.flags & PRINT_ALLCAPS ? "INF" : "inf", 4);
      specifier.flags |= PRINT_ROUNDTRIP;
      specifier.precision = 0;
    }
    specifier.flags &= ~PRINT_ZERO_EXTEND;
    specifier.flags &= ~PRINT_ALTER;
  } else if(d == 0) {
    strcat(prefix, "0x");
    char const * zero;
    char const  * zerosuffix;
    if(specifier.flags & PRINT_ALLCAPS) {
      zero = "0";
      zerosuffix = "P+00";
    } else {
      zero = "0";
      zerosuffix = "p+00";
    }
    strcat(buf, zero);
    len = strlen(zero);
    strcat(suffix, zerosuffix);
  } else {
    strcat(prefix, specifier.flags & PRINT_ALLCAPS ? "0X" : "0x");
    uint64_t rawmant = u & MANT;
    uint64_t mant = u & MANT;
    uint64_t raw_exp = (u >> 52) & EXP;
    enum { IMPLIED_ONE = (1ull << 52) };
    int exponent;
    if(raw_exp) {
      // normal number
      exponent = raw_exp - 1023;
    } else {
      // subnormal number
      exponent = -1023;
      mant = 0;
      while(!(mant & IMPLIED_ONE)) {
        exponent--;
        mant <<= 1;
      }
      mant &= ~IMPLIED_ONE;
    }
    strcat(buf, "1");
    len += 1;

    if(!(specifier.flags & PRINT_ROUNDTRIP))
    {
      int lastlast = 0;
      int last = 0;
      int prec = 52 / 4;
      while(prec > specifier.precision) {
        lastlast += last;
        last = mant & 15;
        mant >>= 4;
        prec--;
      }
      if(last > 8 || (last == 8 && (lastlast || ((mant % 16) & 1))))
        mant += 1;
      if(mant & IMPLIED_ONE) {
        exponent++;
        mant &= ~IMPLIED_ONE;
      }
    }

    while(mant != 0 && (mant & 15) == 0) {
      mant >>= 4;
    }

    char tmp[65];
    if(mant) {
      decimal = u64toa(mant, tmp, 16, specifier.flags & PRINT_ALLCAPS);
      strcat(buf, ".");
      len += 1;
      strcat(buf, tmp);
      len += decimal;
    } else {
      decimal = 0;
    }

    strcat(suffix, specifier.flags & PRINT_ALLCAPS ? "P" : "p");

    if(exponent < 0) {
      strcat(suffix, "-");
      exponent = -exponent;
    } else {
      strcat(suffix, "+");
    }
    int len2 = u64toa10(exponent, tmp);
    if(len2 == 1)
      strcat(suffix, "0");
    strcat(suffix, tmp);
  }
  if(((specifier.flags & PRINT_ALTER) || specifier.precision) && decimal == 0) {
    strcat(buf, ".");
    len += 1;
  }
  int ret = print_number(f, prefix, buf, len, specifier, 0);
  int nzeroes = 0;
  if(decimal < specifier.precision) {
    nzeroes = specifier.precision - decimal;
    specifier.field_width -= nzeroes;
  }
  while(nzeroes--) {
    dfputc('0', f);
    ret++;
  }
  if(*suffix) {
    int len = strlen(suffix);
    dfwrite(suffix, len, f);
    ret += len;
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
  char buf[23];
  int len = u64toa10(u, buf);
  int ret = 0;

  int nzeroes = specifier.precision - len;
  if(nzeroes < 0)
    nzeroes = 0;
  return print_number(f, "", buf, len, specifier, nzeroes);
}
static int print_uint(DFILE * f, print_specifier specifier, va_list* args, int base, bool allcaps) {
  uint64_t u = read_va_uint(specifier, args);
  char buf[67];
  int len = u64toa(u, buf, base, allcaps);
  char prefix[4] = "";
  if(specifier.flags & PRINT_ALTER) {
    switch(base) {
      case 2:
        prefix[0] = '0';
        prefix[1] = specifier.flags & PRINT_ALLCAPS ? 'B' : 'b';
        prefix[2] = '\0';
        break;
      case 8:
        if(u)
        {
          prefix[0] = '0';
          prefix[1] = '\0';
        }
        break;
      case 16:
        prefix[0] = '0';
        prefix[1] = specifier.flags & PRINT_ALLCAPS ? 'X' : 'x';
        prefix[2] = '\0';
        break;
    }
  }

  int nzeroes = specifier.precision - len;
  if(nzeroes < 0)
    nzeroes = 0;
  return print_number(f, prefix, buf, len, specifier, nzeroes);
}
static int print_ptr(DFILE * f, print_specifier specifier, void * ptr) {
  char buf[67];
  int len;
  char prefix[4] = "";
  if(!ptr) {
    memcpy(buf, "(nil)", sizeof "(nil)");
    len = 5;
    specifier.flags &= ~PRINT_ZERO_EXTEND;
  } else {
    uint64_t u = (uintptr_t)ptr;
    len = u64toa(u, buf, 16, false);
    memcpy(prefix, "0x", sizeof "0x");
  }

  int nzeroes = 0;
  if(ptr) 
    nzeroes = specifier.precision - len;
  if(nzeroes < 0)
    nzeroes = 0;
  return print_number(f, prefix, buf, len, specifier, nzeroes);
}

static int print_int(DFILE * f, print_specifier specifier, va_list* args) {
  int64_t i = read_va_int(specifier, args);
  bool neg = i < 0;
  uint64_t u = neg ? -i : i;
  char buf[22];
  int len = u64toa10(u, buf);

  char sign[2] = {0};
  if(neg)
    sign[0] = '-';
  else if(specifier.flags & PRINT_SIGN)
    sign[0] = '+';
  else if(specifier.flags & PRINT_SPACE)
    sign[0] = ' ';

  int nzeroes = specifier.precision - len;
  if(nzeroes < 0)
    nzeroes = 0;
  return print_number(f, sign, buf, len, specifier, nzeroes);
}

static void print_tell(print_specifier specifier, int nchars, va_list* args) {
  switch(specifier.kind_width) {
    case PRINT_SHORTSHORT:
      *va_arg(*args, signed char*) = nchars;
      break;
    case PRINT_SHORT:
      *va_arg(*args, short*) = nchars;
      break;
    case PRINT_WORD:
      *va_arg(*args, int*) = nchars;
      break;
    case PRINT_LONG:
      *va_arg(*args, long*) = nchars;
      break;
    case PRINT_LONGLONG:
      *va_arg(*args, long long*) = nchars;
      break;
    case PRINT_SIZE:
      *va_arg(*args, size_t*) = nchars;
      break;
    case PRINT_MAX:
      *va_arg(*args, intmax_t*) = nchars;
      break;
    case PRINT_PTRDIFF:
      *va_arg(*args, ptrdiff_t*) = nchars;
      break;
    case PRINT_EXACT:
      switch(specifier.kind_exact_width) {
        case 8:
          *va_arg(*args, typeof(1+(int8_t)1)*) = nchars;
          break;
        case 16:
          *va_arg(*args, typeof(1+(int16_t)1)*) = nchars;
          break;
        case 32:
          *va_arg(*args, typeof(1+(int32_t)1)*) = nchars;
          break;
        case 64:
          *va_arg(*args, typeof(1+(int64_t)1)*) = nchars;
          break;
      }
      break;
    case PRINT_FAST:
      switch(specifier.kind_exact_width) {
        case 8:
          *va_arg(*args, typeof(1+(int_fast8_t)1)*) = nchars;
          break;
        case 16:
          *va_arg(*args, typeof(1+(int_fast16_t)1)*) = nchars;
          break;
        case 32:
          *va_arg(*args, typeof(1+(int_fast32_t)1)*) = nchars;
          break;
        case 64:
          *va_arg(*args, typeof(1+(int_fast64_t)1)*) = nchars;
          break;
      }
      break;
  }
}

static int print_string(DFILE * f, print_specifier specifier, char const * str) {
  int len = strlen(str);
  return print_number(f, "", str, len, specifier, 0);
}

static int print_format(DFILE * f, char const ** pfmt, va_list* args, int nchars) {
  print_specifier specifier = parse_print_specifier(*pfmt, args);
  *pfmt = *pfmt + specifier.chars_consumed;
  switch(specifier.print_kind) {
    case PRINT_PERCENT: {
      if(dfputc('%', f) < 0)
        return -1;
      return 1;
    }
    case PRINT_CHAR: {
      if(specifier.kind_width == PRINT_LONG)
        return -1;
      char arr[2] = { va_arg(*args, int), 0 };
      return print_string(f, specifier, arr);
    }
    case PRINT_STRING: {
      if(specifier.kind_width == PRINT_LONG)
        return -1;
      char const * str = va_arg(*args, char*);
      return print_string(f, specifier, str);
    }
    case PRINT_ERROR: {
      char * str = strerror(errno);
      return print_string(f, specifier, str);
    }
    case PRINT_BINARY:
      return print_uint(f, specifier, args, 2, false);
    case PRINT_OCTAL:
      return print_uint(f, specifier, args, 8, false);
    case PRINT_HEX:
      return print_uint(f, specifier, args, 16, specifier.flags & PRINT_ALLCAPS);
    case PRINT_UINT:
      return print_uint10(f, specifier, args);
    case PRINT_POINTER:
      return print_ptr(f, specifier, va_arg(*args, void*));
    case PRINT_INT:
      return print_int(f, specifier, args);
    case PRINT_DOUBLE:
    case PRINT_EXPONENT:
    case PRINT_GENERAL:
      return print_double(f, specifier, args);
    case PRINT_HEXPONENT:
      return print_hexponent(f, specifier, args);
    case PRINT_TELL:
      print_tell(specifier, nchars, args);
      return 0;
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
        int ret = print_format(f, &fmt, args, printed);
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
int d_vfprintf(DFILE * f, char const * fmt, va_list args) {
  return dvfprintf_impl(f, fmt, VA_POINTER(args));
}

int d_fprintf(DFILE * f, char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = d_vfprintf(f, fmt, args);
  va_end(args);
  return ret;
}
int d_printf(char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = d_vfprintf(dstdout, fmt, args);
  va_end(args);
  return ret;
}
int d_vprintf(char const * fmt, va_list args) {
  int ret = d_vfprintf(dstdout, fmt, args);
  return ret;
}
