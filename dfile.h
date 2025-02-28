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

__attribute__((format(printf, 1, 2))) int deeprintf(char const * fmt, ...);
__attribute__((format(printf, 1, 0))) int dvprintf(char const * fmt, va_list args);
__attribute__((format(printf, 2, 3))) int dfprintf(DFILE * f, char const * fmt, ...);
__attribute__((format(printf, 2, 0))) int dvfprintf(DFILE * f, char const * fmt, va_list args);
#endif
