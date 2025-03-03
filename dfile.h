#ifndef DFILE_H
#define DFILE_H
#include <stdarg.h>
/* Copyright 2025 Richard N Van Natta
 *
 * This file is part of the DFILE stdio alternative.
 *
 * DFILE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 * 
 * DFILE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with DFILE.
 *
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * If not, visit <https://github.com/rnvannatta>
 */

#ifdef _WIN64
typedef long long ssize_t;
typedef long long off64_t;
#else
#include <sys/types.h>
typedef long ssize_t;
typedef long off64_t;
#endif

typedef struct DFILE DFILE;
extern DFILE * dstdin;
extern DFILE * dstdout;
extern DFILE * dstderr;

long long int dftell(DFILE * f);
int dfeof(DFILE * f);
int dferror(DFILE * f);
void dclearerror(DFILE * f);

DFILE * dfdopen(int fd, char const * flags);
DFILE * dfopen(char const * path, char const * mode);
DFILE * dpopen(char const * cmd, char const * mode);
DFILE * dtmpfile();
DFILE * dstrfile();

int dfflush(DFILE * f);
int dfseek(DFILE * f, int offset, int whence);
void drewind(DFILE * f);
int dfclose(DFILE * f);
int dpclose(DFILE * f);

int dfwrite(const void * ptr, int ct, DFILE * f);
int dfread(void * ptr, int ct, DFILE * f);
char * dfgets(char * buf, int ct, DFILE * f);
int dungetc(int c, DFILE * f);

typedef ssize_t d_cookie_read_function_t(void * cookie, char * buf, size_t size);
typedef ssize_t d_cookie_write_function_t(void * cookie, char const * buf, size_t size);
typedef int d_cookie_seek_function_t(void * cookie, off64_t * offset, int whence);
typedef int d_cookie_close_function_t(void * cookie);

typedef struct {
  d_cookie_read_function_t *read;
  d_cookie_write_function_t *write;
  d_cookie_seek_function_t *seek;
  d_cookie_close_function_t *close;
} d_cookie_io_functions_t;

DFILE * d_fopencookie(void * cookie, char const * mode, d_cookie_io_functions_t funcs);

DFILE * dfmemopen(void * buf, size_t size, char const * flags);

//////////////////////////////////////////
//              NICETIES                //
//////////////////////////////////////////

int dfgetc(DFILE * f);
int dgetc(DFILE * f);
int dgetchar();

int dfputc(int c, DFILE * f);
int dputc(int c, DFILE * f);
int dputchar(int c);

int dfputs(char const * str, DFILE * f);
int dputs(char const * str);

//////////////////////////////////////////
//               PRINTF                 //
//////////////////////////////////////////

__attribute__((format(printf, 1, 2))) int d_printf(char const * fmt, ...);
__attribute__((format(printf, 1, 0))) int d_vprintf(char const * fmt, va_list args);
__attribute__((format(printf, 2, 3))) int d_fprintf(DFILE * f, char const * fmt, ...);
__attribute__((format(printf, 2, 0))) int d_vfprintf(DFILE * f, char const * fmt, va_list args);
#endif
