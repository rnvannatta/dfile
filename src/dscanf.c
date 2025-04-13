/*
 * Copyright 2025 Richard N Van Natta
 *
 * This file is part of the DFILE stdio alternative.
 *
 * DFILE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 * 
 * DFILE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
 * Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with DFILE.
 *
 * If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "dfile.h"
#include "dprintf.h"
#include "fast_float.h"

__attribute__((visibility("hidden")))
print_specifier parse_print_specifier(char const * fmt, va_list* args, bool is_scan);

static int skip_whitespace(DFILE * f, int *chars_scanned) {
  for(;;) {
    int ret = d_fgetc_unlocked(f);
    if(ret < 0) {
      d_funlockfile(f);
      return -1;
    }
    if(!strchr(" \f\n\r\t\v", ret)) {
      d_ungetc(ret, f);
      break;
    }
    (*chars_scanned)++;
  }
  return 0;
}

static int scan_chars(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(specifier.kind_width == SCAN_LONG)
    return -1;
  int nchars = specifier.field_width < 0 ? 1 : specifier.field_width;
  char * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  bool any = false, eof = false;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    (*chars_scanned)++;
    if(dst)
      *dst++ = c;
    any = true;
  }
  if(any && dst) {
    (*fields_scanned)++;
  }
  return eof && !any ? -1 : any;
}
static int scan_set(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(specifier.kind_width == SCAN_LONG)
    return -1;
  int nchars = specifier.field_width < 0 ? INT32_MAX : specifier.field_width;
  char * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  bool set[256] = { 0 };
  for(char const * cur = specifier.label; cur != specifier.label_end; cur++) {
    if(cur + 2 < specifier.label_end && cur[1] == '-') {
      unsigned char s = cur[0], e = cur[2];
      if(s > e) {
        unsigned char tmp = e;
        e = s;
        s = tmp;
      }
      e += 1;
      for(unsigned char i = s; i != e; i++)
        set[i] = true;
    } else {
      set[(unsigned char)*cur] = true;
    }
  }
  bool any = false, eof = false;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    if(!set[c] ^ !!(specifier.flags & SCAN_INVERTED)) {
      d_ungetc(c, f);
      break;
    }
    any = true;
    (*chars_scanned)++;
    if(dst)
      *dst++ = c;
  }
  if(dst) {
    *dst = '\0';
    (*fields_scanned)++;
  }
  return eof && !any ? -1 : any;
}
static int scan_string(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(specifier.kind_width == SCAN_LONG)
    return -1;
  if(skip_whitespace(f, chars_scanned) < 0)
    return -1;
  int nchars = specifier.field_width < 0 ? INT32_MAX : specifier.field_width;
  char * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  bool any = false, eof = false;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    if(strchr(" \f\n\r\t\v", c)) {
      d_ungetc(c, f);
      break;
    }
    (*chars_scanned)++;
    any = true;
    if(dst)
      *dst++ = c;
  }
  *dst++ = '\0';
  if(any && dst) {
    (*fields_scanned)++;
  }
  return eof && !any ? -1 : any;
}

static void write_signed_int(void * dst, int sign, uint64_t u, print_specifier specifier) {
  switch(specifier.kind_width) {
    case PRINT_SHORTSHORT:
      *(signed char*)dst = sign*u;
      break;
    case PRINT_SHORT:
      *(signed short*)dst = sign*u;
      break;
    case PRINT_WORD:
      *(signed int*)dst = sign*u;
      break;
    case PRINT_LONG:
      *(signed long*)dst = sign*u;
      break;
    case PRINT_LONGLONG:
      *(signed long long*)dst = sign*u;
      break;
    case PRINT_SIZE:
      *(ssize_t*)dst = sign*u;
      break;
    case PRINT_MAX:
      *(intmax_t*)dst = sign*u;
      break;
    case PRINT_PTRDIFF:
      *(ptrdiff_t*)dst = sign*u;
      break;
    case PRINT_EXACT:
      switch(specifier.kind_exact_width) {
        case 8:
          *(int8_t*)dst = sign*u;
          break;
        case 16:
          *(int16_t*)dst = sign*u;
          break;
        case 32:
          *(int32_t*)dst = sign*u;
          break;
        case 64:
          *(int64_t*)dst = sign*u;
          break;
      }
      break;
    case PRINT_FAST:
      switch(specifier.kind_exact_width) {
        case 8:
          *(int_fast8_t*)dst = sign*u;
          break;
        case 16:
          *(int_fast16_t*)dst = sign*u;
          break;
        case 32:
          *(int_fast32_t*)dst = sign*u;
          break;
        case 64:
          *(int_fast64_t*)dst = sign*u;
          break;
      }
      break;
  }
}
static void write_unsigned_int(void * dst, int sign, uint64_t u, print_specifier specifier) {
  switch(specifier.kind_width) {
    case PRINT_SHORTSHORT:
      *(unsigned char*)dst = sign*u;
      break;
    case PRINT_SHORT:
      *(unsigned short*)dst = sign*u;
      break;
    case PRINT_WORD:
      *(unsigned int*)dst = sign*u;
      break;
    case PRINT_LONG:
      *(unsigned long*)dst = sign*u;
      break;
    case PRINT_LONGLONG:
      *(unsigned long long*)dst = sign*u;
      break;
    case PRINT_SIZE:
      *(size_t*)dst = sign*u;
      break;
    case PRINT_MAX:
      *(uintmax_t*)dst = sign*u;
      break;
    case PRINT_PTRDIFF:
      *(ptrdiff_t*)dst = sign*u;
      break;
    case PRINT_EXACT:
      switch(specifier.kind_exact_width) {
        case 8:
          *(uint8_t*)dst = sign*u;
          break;
        case 16:
          *(uint16_t*)dst = sign*u;
          break;
        case 32:
          *(uint32_t*)dst = sign*u;
          break;
        case 64:
          *(uint64_t*)dst = sign*u;
          break;
      }
      break;
    case PRINT_FAST:
      switch(specifier.kind_exact_width) {
        case 8:
          *(uint_fast8_t*)dst = sign*u;
          break;
        case 16:
          *(uint_fast16_t*)dst = sign*u;
          break;
        case 32:
          *(uint_fast32_t*)dst = sign*u;
          break;
        case 64:
          *(uint_fast64_t*)dst = sign*u;
          break;
      }
      break;
  }
}
static void write_int(void * dst, int sign, uint64_t u, print_specifier specifier) {
  // TODO formats
  switch(specifier.print_kind) {
    case SCAN_HEX:
    case SCAN_UINT:
    case SCAN_OCTAL:
    case SCAN_BINARY:
      write_unsigned_int(dst, sign, u, specifier);
      break;
    case SCAN_INT:
    case SCAN_TELL:
      write_signed_int(dst, sign, u, specifier);
      break;
  }
}


static int scan_sign(DFILE * f, int *nchars, int *chars_scanned) {
  int c = d_fgetc_unlocked(f);
  if(c < 0)
    return 0;
  if(c == '-') {
    *nchars -= 1;
    *chars_scanned += 1;
    return -1;
  }
  if(c == '+') {
    *nchars -= 1;
    *chars_scanned += 1;
    return 1;
  }
  int ret = d_ungetc(c, f);
  return ret >= 0;
}

static int scan_int(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(skip_whitespace(f, chars_scanned) < 0)
    return -1;
  int nchars = specifier.field_width < 0 ? INT32_MAX : specifier.field_width;
  int base = 10;
  switch(specifier.print_kind) {
    case SCAN_HEX:
      base = 16;
      break;
    case SCAN_UINT:
    case SCAN_INT:
      base = 10;
      break;
    case SCAN_OCTAL:
      base = 8;
      break;
    case SCAN_BINARY:
      base = 2;
      break;
  }
  void * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  int sign = scan_sign(f, &nchars, chars_scanned);
  if(!sign)
    return -1;

  if(!nchars)
    return -1;

  if((base == 2 || base == 16) && nchars >= 3) {
    int zero = d_fgetc_unlocked(f);
    if(zero < 0)
      return -1;
    if(zero == '0') {
      int sigil = d_fgetc_unlocked(f);
      if(sigil < 0)
        return -1;
      if(((base == 2 && (sigil == 'b' || sigil == 'B')) ||
          (base == 16 && (sigil == 'x' || sigil == 'X')))) {
        *chars_scanned += 2;
        nchars -= 2;
      } else {
        d_ungetc(sigil, f);
        d_ungetc(zero, f);
      }
    } else {
      d_ungetc(zero, f);
    }
  }

  uint64_t u = 0;
  bool any = false, eof = false;
  int base_min_10 = base < 10 ? base : 10;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    if('0' <= c && c <= '0' + base_min_10 - 1) {
      u = base*u + c - '0';
    } else if(base > 10 && 'a' <= c && c <= 'a' + base - 11) {
      u = base*u + 10 + c - 'a';
    } else if(base > 10 && 'A' <= c && c <= 'A' + base - 11) {
      u = base*u + 10 + c - 'A';
    } else {
      d_ungetc(c, f);
      break;
    }
    any = true;
    (*chars_scanned)++;
  }
  if(dst)
    write_int(dst, sign, u, specifier);
  if(any && dst) {
    (*fields_scanned)++;
  }
  return eof && !any ? -1 : any;
}

static int scan_pointer(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(skip_whitespace(f, chars_scanned) < 0)
    return -1;
  
  int nchars = specifier.field_width < 0 ? INT32_MAX : specifier.field_width;

  char const * nil = "(nil)";
  bool fail = false;
  char buf[5];
  // (nil)
  int i;
  for(i = 0; i < 5; i++) {
    int c = d_fgetc_unlocked(f);
    buf[i] = c;
    if(c < 0) {
      fail = true;
      i++;
      break;
    }
    if(c != nil[i]) {
      fail = true;
      i++;
      break;
    }
  }
  print_specifier dummy = specifier;
  dummy.print_kind = PRINT_HEX;
  dummy.kind_width = PRINT_EXACT;
  dummy.kind_exact_width = 8*sizeof(void*);
  if(fail) {
    while(i --> 0) {
      d_ungetc(buf[i], f);
    }
    return scan_int(f, dummy, args, fields_scanned, chars_scanned);
  } else {
    *chars_scanned += 5;
    void * dst = NULL;
    if(!(specifier.flags & SCAN_IGNORE)) {
      dst = va_arg(*args, void*);
    }
    if(dst) {
      *fields_scanned += 1;
      write_int(dst, +1, (uintptr_t)NULL, dummy);
    }
    return 1;
  }
}

// format of a double:
// (sign) digits (dot) (digits)
// (sign) dot digits

// format of an exponent
// e|p (sign) digits

enum {
  DOUBLE_DONE,

  DOUBLE_INITIAL,
  DOUBLE_POST_SIGN,
  DOUBLE_POST_ZERO,

  DOUBLE_DIGITS,
  DOUBLE_INIT_FRACTION,
  DOUBLE_FRACTION,

  DOUBLE_INIT_HEXDIGITS,
  DOUBLE_HEXDIGITS,
  DOUBLE_INIT_HEXFRACTION,
  DOUBLE_HEXFRACTION,

  DOUBLE_EXP,
  DOUBLE_EXP_POST_SIGN,

  DOUBLE_SPEC_NAN,
  DOUBLE_SPEC_INF,

  NUM_DOUBLE_STATES,
};

static int8_t parse_double_table[NUM_DOUBLE_STATES][128] = {
  [DOUBLE_INITIAL] = {
    ['+'] = DOUBLE_POST_SIGN,
    ['-'] = DOUBLE_POST_SIGN,
    ['0'] = DOUBLE_POST_ZERO,
    ['1' ... '9'] = DOUBLE_DIGITS,
    ['.'] = DOUBLE_INIT_FRACTION,

    ['n'] = DOUBLE_SPEC_NAN,
    ['N'] = DOUBLE_SPEC_NAN,
    ['i'] = DOUBLE_SPEC_INF,
    ['I'] = DOUBLE_SPEC_INF,
  },
  [DOUBLE_POST_SIGN] = {
    ['0'] = DOUBLE_DIGITS,
    ['1' ... '9'] = DOUBLE_DIGITS,
    ['.'] = DOUBLE_INIT_FRACTION,

    ['n'] = DOUBLE_SPEC_NAN,
    ['N'] = DOUBLE_SPEC_NAN,
    ['i'] = DOUBLE_SPEC_INF,
    ['I'] = DOUBLE_SPEC_INF,
  },
  [DOUBLE_POST_ZERO] = {
    ['x'] = DOUBLE_INIT_HEXDIGITS,
    ['X'] = DOUBLE_INIT_HEXDIGITS,
    ['0' ... '9'] = DOUBLE_DIGITS,
    ['.'] = DOUBLE_FRACTION,
    ['e'] = DOUBLE_EXP,
    ['E'] = DOUBLE_EXP,
  },
  [DOUBLE_DIGITS] = {
    ['0' ... '9'] = DOUBLE_DIGITS,
    // don't need any more digits if we've had one digit,
    // the difference between FRACTION and INIT_FRACTION
    // is INIT_FRACTION demands a digit before being
    // able to jump into an exponent state
    ['.'] = DOUBLE_FRACTION,
    ['e'] = DOUBLE_EXP,
    ['E'] = DOUBLE_EXP,
  },
  [DOUBLE_INIT_FRACTION] = {
    ['0' ... '9'] = DOUBLE_FRACTION,
  },
  [DOUBLE_FRACTION] = {
    ['0' ... '9'] = DOUBLE_FRACTION,
    ['e'] = DOUBLE_EXP,
    ['E'] = DOUBLE_EXP,
  },
  [DOUBLE_EXP] = {
    ['+'] = DOUBLE_EXP_POST_SIGN,
    ['-'] = DOUBLE_EXP_POST_SIGN,
    ['0' ... '9'] = DOUBLE_EXP_POST_SIGN,
  },
  [DOUBLE_EXP_POST_SIGN] = {
    ['0' ... '9'] = DOUBLE_EXP_POST_SIGN,
  },

  [DOUBLE_INIT_HEXDIGITS] = {
    ['0' ... '9'] = DOUBLE_HEXDIGITS,
    ['a' ... 'f'] = DOUBLE_HEXDIGITS,
    ['A' ... 'F'] = DOUBLE_HEXDIGITS,
    ['.'] = DOUBLE_INIT_HEXFRACTION,
  },
  [DOUBLE_HEXDIGITS] = {
    ['0' ... '9'] = DOUBLE_HEXDIGITS,
    ['a' ... 'f'] = DOUBLE_HEXDIGITS,
    ['A' ... 'F'] = DOUBLE_HEXDIGITS,
    ['.'] = DOUBLE_HEXFRACTION,
    ['p'] = DOUBLE_EXP,
    ['P'] = DOUBLE_EXP,
  },
  [DOUBLE_INIT_HEXFRACTION] = {
    ['0' ... '9'] = DOUBLE_HEXFRACTION,
    ['a' ... 'f'] = DOUBLE_HEXFRACTION,
    ['A' ... 'F'] = DOUBLE_HEXFRACTION,
  },
  [DOUBLE_HEXFRACTION] = {
    ['0' ... '9'] = DOUBLE_HEXFRACTION,
    ['a' ... 'f'] = DOUBLE_HEXFRACTION,
    ['A' ... 'F'] = DOUBLE_HEXFRACTION,
    ['p'] = DOUBLE_EXP,
    ['P'] = DOUBLE_EXP,
  },
};

double exp2i(int i) {
  if(i < 0)
    return 1.0/exp2i(-i);
  double base = 2;
  double ret = 1;
  while(i) {
    if(i % 2)
      ret *= base;
    i >>= 1;
    base *= base;
  }
  return ret;
}

int hex_from_chars(char const * buf, char const * end, double * d) {
  int sign = 1;
  if(buf == end)
    return 0;
  if(*buf == '+') {
    buf++;
  } else if(*buf == '-') {
    sign = -1;
    buf++;
  }
  if(buf+2 >= end)
    return 0;
  if(buf[0] != '0' || (buf[1] != 'x' && buf[1] != 'X'))
    return 0;
  buf += 2;

  bool ok = false, fraction = false;
  uint64_t mant = 0;
  uint64_t mant_mask = 0xfull << 60;
  int decimal = 0;
  while(buf != end) {
    char c = buf[0];
    int digit = -1;
    if('0' <= c && c <= '9')
      digit = c - '0';
    else if('a' <= c && c <= 'f')
      digit = c - 'a' + 10;
    else if('A' <= c && c <= 'F')
      digit = c - 'A' + 10;
    else if(c == '.')
      fraction = true;
    else
      break;

    if(digit >= 0 && !(mant & mant_mask)) {
      mant = mant * 16 + digit;
      ok = true;
      if(fraction)
        decimal -= 4;
    }
    buf++;
  }
  int exp = 0;
  if(buf == end)
    goto finish;
  if(buf[0] == 'p' || buf[0] == 'P') {
    buf++;
    if(buf == end)
      goto finish;
    int expsign = 1;
    if(*buf == '+') {
      buf++;
    } else if(*buf == '-') {
      expsign = -1;
      buf++;
    }
    while(buf != end) {
      char c = buf[0];
      if('0' <= c && c <= '9')
        exp = exp * 10 + c - '0';
      buf++;
    }
    exp *= expsign;
  }
finish:
  {
    double mantd = mant;
    double mult = exp2i(decimal + exp);
    *d = sign * mantd * mult;
    return 1;
  }
}
int hex_from_charsf(char const * buf, char const * end, float * f) {
  double d;
  int ret = hex_from_chars(buf, end, &d);
  *f = d;
  return ret;
}

static int scan_str_ci(char * str, char buf[static 4096], int * nchars, int * nchars_left, DFILE * f) {
  bool eof = false;
  while(*str && (*nchars_left)--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    if((c | 32) != *str) {
      d_ungetc(c, f);
      break;
    }
    buf[(*nchars)++] = (c | 32);
    str++;
  }
  return eof ? -1 : 0;
}

static int scan_nan(char buf[static 4096], int * nchars, int * nchars_left, DFILE * f) {
  return scan_str_ci("an", buf, nchars, nchars_left, f);
}
static int scan_inf(char buf[static 4096], int * nchars, int * nchars_left, DFILE * f) {
  return scan_str_ci("nfinite", buf, nchars, nchars_left, f);
}

static int scan_float(DFILE * f, print_specifier specifier, va_list* args, int *fields_scanned, int *chars_scanned) {
  if(skip_whitespace(f, chars_scanned) < 0)
    return -1;

  char buf[4096];
  int nchars = 0;
  int nchars_left = specifier.field_width < 0 ? INT32_MAX : specifier.field_width;

  int state = DOUBLE_INITIAL;
  bool eof = false, hex = false;

  // truncate at 4095 chars. no input that isn't trying to hack
  // is going to have a float more than 4095 chars. the longest
  // valid double is ~1500 chars.
  while(nchars_left-- && nchars < 4095) {
    int c = d_fgetc_unlocked(f);
    if(c < 0) {
      eof = true;
      break;
    }
    if(c > 127)
      state = DOUBLE_DONE;
    else
      state = parse_double_table[state][c];
    if(state == DOUBLE_DONE) {
      d_ungetc(c, f);
      break;
    }
    buf[nchars++] = c;

    if(state == DOUBLE_SPEC_NAN) {
      int ok = scan_nan(buf, &nchars, &nchars_left, f);
      if(ok < 0)
        eof = true;
      break;
    }
    if(state == DOUBLE_SPEC_INF) {
      int ok = scan_inf(buf, &nchars, &nchars_left, f);
      if(ok < 0)
        eof = true;
      break;
    }
    if(state == DOUBLE_INIT_HEXDIGITS)
      hex = true;
  }

  buf[nchars] = '\0';
  void * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  if(specifier.kind_width == PRINT_LONG || specifier.kind_width == PRINT_LONGLONG) {
    double d;
    if(hex) {
      if(!hex_from_chars(buf, buf + nchars, &d))
        return eof ? -1 : 0;
    } else {
      if(!ff_from_chars(buf, buf + nchars, &d))
        return eof ? -1 : 0;
    }
    if(dst) {
      *fields_scanned += 1;
      if(specifier.kind_width == PRINT_LONG)
        *(double*)dst = d;
      else
        *(long double*)dst = d;
    }
  } else {
    float f;
    if(hex) {
      if(!hex_from_charsf(buf, buf + nchars, &f))
        return eof ? -1 : 0;
    } else {
      if(!ff_from_charsf(buf, buf + nchars, &f))
        return eof ? -1 : 0;
    }
    if(dst) {
      *fields_scanned += 1;
      *(float*)dst = f;
    }
  }
  *chars_scanned += nchars;
  return 1;
}

static int scan_tell(DFILE * f, print_specifier specifier, va_list* args, int *chars_scanned) { 
  void * dst = NULL;
  if(!(specifier.flags & SCAN_IGNORE)) {
    dst = va_arg(*args, void*);
  }
  if(dst)
    write_int(dst, +1, *chars_scanned, specifier);
  return 1;
}

static int scan_format(DFILE * f, char const ** pfmt, va_list* args, int *fields_scanned, int *chars_scanned) {
  print_specifier specifier = parse_print_specifier(*pfmt, args, true);
  *pfmt = *pfmt + specifier.chars_consumed;
  switch(specifier.print_kind) {
    case SCAN_PERCENT: {
      int c = d_fgetc_unlocked(f);
      if(c < 0)
        return -1;
      if(c != '%') {
        d_ungetc(c, f);
        return -1;
      }
      (*chars_scanned)++;
      // fields_scanned not modified, %% isn't an assignment
      return 1;
    }
    case SCAN_CHAR:
      return scan_chars(f, specifier, args, fields_scanned, chars_scanned);
    case SCAN_STRING:
      return scan_string(f, specifier, args, fields_scanned, chars_scanned);
    case SCAN_HEX:
    case SCAN_UINT:
    case SCAN_INT:
    case SCAN_OCTAL:
    case SCAN_BINARY:
      return scan_int(f, specifier, args, fields_scanned, chars_scanned);
    case SCAN_POINTER:
      return scan_pointer(f, specifier, args, fields_scanned, chars_scanned);
    case SCAN_DOUBLE:
    case SCAN_GENERAL:
    case SCAN_EXPONENT:
    case SCAN_HEXPONENT:
      return scan_float(f, specifier, args, fields_scanned, chars_scanned);
    case SCAN_TELL:
      // tells don't increment fields scanned cus spec says so.
      return scan_tell(f, specifier, args, chars_scanned);
    case SCAN_SET:
      return scan_set(f, specifier, args, fields_scanned, chars_scanned);
    default:
      return -1;
  }
}

#if defined(_WIN64) || defined(__EMSCRIPTEN__)
#define VA_POINTER(x) (&x)
#else
#define VA_POINTER(x) ((va_list*)x)
#endif
int d_vfscanf_impl(DFILE * f, char const * fmt, va_list* args) {
  d_flockfile(f);
  int fields_scanned = 0;
  int chars_scanned = 0;
  char c;
  while((c = *fmt++)) {
    switch(c) {
      case '%': {
        int ret = scan_format(f, &fmt, args, &fields_scanned, &chars_scanned);
        if(ret < 0) {
          d_funlockfile(f);
          return fields_scanned ? fields_scanned : -1;
        }
        if(ret == 0) {
          d_funlockfile(f);
          return fields_scanned;
        }
        break;
      }
      case ' ':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
      case '\v': {
        if(skip_whitespace(f, &chars_scanned) < 0) {
          d_funlockfile(f);
          return fields_scanned ? fields_scanned : -1;
        }
        break;
      }
      default: {
        int ret = d_fgetc_unlocked(f);
        if(ret < 0) {
          d_funlockfile(f);
          return fields_scanned ? fields_scanned : -1;
        }
        if(ret != c) {
          d_ungetc(ret, f);
          d_funlockfile(f);
          return fields_scanned;
        }
        chars_scanned += 1;
        break;
      }
    }
  }
  d_funlockfile(f);
  return fields_scanned;
}
int d_vfscanf(DFILE * f, char const * fmt, va_list args) {
  return d_vfscanf_impl(f, fmt, VA_POINTER(args));
}

int d_fscanf(DFILE * f, char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = d_vfscanf(f, fmt, args);
  va_end(args);
  return ret;
}
int d_scanf(char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = d_vfscanf(dstdin, fmt, args);
  va_end(args);
  return ret;
}
int d_vscanf(char const * fmt, va_list args) {
  int ret = d_vfscanf(dstdout, fmt, args);
  return ret;
}

static _Thread_local DFILE * sscanf_stream;
int d_vsscanf(char const * buf, char const * fmt, va_list args) {
  if(!sscanf_stream) {
    sscanf_stream = d_open_strstream(buf);
    if(!sscanf_stream)
      return -1;
  } else if(!d_reopen_strstream(buf, sscanf_stream)) {
    return -1;
  }
  int ret = d_vfscanf_impl(sscanf_stream, fmt, VA_POINTER(args));
  // don't need to reseat, nothing happens on close eh
  // d_reopen_strstream("", sscanf_stream);
  return ret;
}
int d_sscanf(char const * buf, char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = d_vsscanf(buf, fmt, args);
  va_end(args);
  return ret;
}
