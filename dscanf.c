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
#include "dragonbox.h"

__attribute__((visibility("hidden")))
print_specifier parse_print_specifier(char const * fmt, va_list* args);

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
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0)
      return -1;
    (*chars_scanned)++;
    if(dst)
      *dst++ = c;
  }
  if(dst) {
    (*fields_scanned)++;
  }
  return 1;
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
  bool any = false;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0)
      break;
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
  return any;
}

static uint64_t write_signed_int(void * dst, int sign, uint64_t u, print_specifier specifier) {
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
static uint64_t write_unsigned_int(void * dst, int sign, uint64_t u, print_specifier specifier) {
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
  bool any = false;
  int base_min_10 = base < 10 ? base : 10;
  while(nchars--) {
    int c = d_fgetc_unlocked(f);
    if(c < 0)
      break;
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
  return any;
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
  print_specifier specifier = parse_print_specifier(*pfmt, args);
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
    case SCAN_TELL:
      // tells don't increment fields scanned cus spec says so.
      return scan_tell(f, specifier, args, chars_scanned);
    default:
      return -1;
  }
}

#ifdef _WIN64
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
