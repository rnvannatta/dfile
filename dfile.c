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
#ifdef __linux__
#define _GNU_SOURCE
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#endif

#ifdef _WIN64
#include <stdio.h>
#include <io.h>
#include <fileapi.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <synchapi.h>
#include <handleapi.h>
#include <namedpipeapi.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <assert.h>

#include "dfile.h"

// w  DONE
// w+ DONE
// r  DONE
// r+ DONE
// a  DONE
// a+ DONE

// tmpfile    DONE

// fclose     DONE
// fflush     DONE
// fopen      DONE
// freopen    DONE
// setvbuf    DONE
// setbuf     DONE
// setbuffer  DONE
// setlinebuf DONE
// fileno

// fgetc     DONE
// fgets     DONE
// fputc     DONE
// fputs     DONE
// getc      DONE
// getchar   DONE
// putc      DONE
// putchar   DONE
// puts      DONE
// ungetc    DONE

// fread     DONE
// fwrite    DONE

// fgetpos   DONE
// fseek     DONE
// fsetpos   DONE
// ftell     DONE
// rewind    DONE

// clearerr  DONE
// feof      DONE
// ferror    DONE

// popen     DONE
// pclose    DONE

// fprintf   DONE
// fscanf
// printf    DONE
// scanf
// snprintf  DONE
// sprintf   DONE
// sscanf
// vfprintf  DONE
// vfscanf
// vfprintf  DONE
// vscanf
// vsnprintf DONE
// vsprintf  DONE
// vsscanf

// fopencookie      DONE
// fmemopen         DONE
// open_memstream   DONE
// asprintf         DONE
// vasprintf        DONE

// strfile          DONE
// fdreopen         DONE
// freopencookie    DONE
// fmemreopen       DONE
// reopen_memstream DONE
// preopen          DONE
// retmpfile        DONE
// restrfile        DONE

// flockfile        DONE
// ftrylockfile     DONE
// funlockfile      DONE

// getc_unlocked
// getchar_unlocked
// putc_unlocked
// putchar_unlocked

// clearerr_unlocked
// feof_unlocked
// ferror_unlocked
// fileno_unlocked
// fflush_unlocked

// fgetc_unlocked
// fputc_unlocked

// fread_unlocked
// fwrite_unlocked
// fgets_unlocked
// fputs_unlocked

// fputf
// fputg
// fputa
// fpute
// fputb
// fputo
// fputu
// fputx
// fputd
// fputp
// fputm

enum {
  DFILE_ERROR = 1,
  DFILE_READ = 2,
  DFILE_WRITE = 4,
  DFILE_APPEND = 8,
  DFILE_LINE_BUFFERED = 16,
  DFILE_UNBUFFERED = 32,
  DFILE_EOF = 64,
  DFILE_STRFILE = 128,
  DFILE_COOKIE = 256,
  DFILE_PROCESS = 512,
};
enum { DFILE_CANARY = 0xDF11E83 };

typedef struct STRPAGE {
  struct STRPAGE * next;
  char buf[D_BUFSIZ];
} STRPAGE;

enum { DFILE_UNGETS = 2 };
typedef struct DFILE_TAIL {
#ifdef __linux__
  pthread_mutex_t lock;
#else
  CRITICAL_SECTION lock;
#endif
  DFILE * prev;
  DFILE * next;
} DFILE_TAIL;

typedef struct DFILE {
  // invariant:
  // the underlying cursor of the fd is at the buf_cursor
  // and the dirty region is from 0 to dirty_cursor
  // the cursor of the DFILE is at dirty_cursor
  int canary;
  int fd;
  int buf_cursor;
  int dirty_cursor;
  int flags;
  size_t buf_size;
  char * buf;
  char buf_storage[D_BUFSIZ];
  int num_ungets;
  char ungets[DFILE_UNGETS];
  // strfile stuff
  off_t tell;
  off_t len;
  union {
    STRPAGE * strpages;
    void * cookie;
  };
  d_cookie_io_functions_t funcs;
#ifdef __linux__
  pid_t process;
#endif
#ifdef _WIN64
  HANDLE process;
  HANDLE thread;
#endif
  DFILE_TAIL tail[];
} DFILE;

typedef struct DFILE_STORAGE {
  DFILE f;
  DFILE_TAIL tail;
} DFILE_STORAGE;

static DFILE * malloc_dfile() {
  void * ret = malloc(sizeof(DFILE_STORAGE));
  memset(ret, 0, sizeof(DFILE_STORAGE));
  return ret;
}

extern DFILE_STORAGE dstdin_impl;
extern DFILE_STORAGE dstdout_impl;
extern DFILE_STORAGE dstderr_impl;
DFILE * dstdin = &dstdin_impl.f;
DFILE * dstdout = &dstdout_impl.f;
DFILE * dstderr = &dstderr_impl.f;

DFILE_STORAGE dstdin_impl = {
  .f = {
    .canary = DFILE_CANARY,
    .fd = D_STDIN_FILENO,
    .flags = DFILE_READ | DFILE_LINE_BUFFERED,
    .buf_size = D_BUFSIZ,
    .buf = dstdin_impl.f.buf_storage,
  },
  .tail = {
    .next = &dstdout_impl.f,
  },
};
DFILE_STORAGE dstdout_impl = {
  .f = {
    .canary = DFILE_CANARY,
    .fd = D_STDOUT_FILENO,
    .flags = DFILE_WRITE | DFILE_LINE_BUFFERED,
    .buf_size = D_BUFSIZ,
    .buf = dstdout_impl.f.buf_storage,
  },
  .tail = {
    .next = &dstderr_impl.f,
    .prev = &dstdin_impl.f,
  },
};
DFILE_STORAGE dstderr_impl = {
  .f = {
    .canary = DFILE_CANARY,
    .fd = D_STDERR_FILENO,
    .flags = DFILE_WRITE | DFILE_UNBUFFERED,
    .buf_size = D_BUFSIZ,
    .buf = dstderr_impl.f.buf_storage,
  },
  .tail = {
    .next = NULL,
    .prev = &dstdout_impl.f,
  },
};

static DFILE_TAIL dlist_mutex;
static void d_locklist() {
#ifdef __linux__
  pthread_mutex_lock(&dlist_mutex.lock);
#else
  EnterCriticalSection(&dlist_mutex.lock);
#endif
}
static void d_unlocklist() {
#ifdef __linux__
  pthread_mutex_unlock(&dlist_mutex.lock);
#else
  LeaveCriticalSection(&dlist_mutex.lock);
#endif
}

static void insert_dfile_list(DFILE * d) {
  d_locklist();
  DFILE * root;
  if(d->flags & DFILE_LINE_BUFFERED)
    root = dstdin;
  else
    root = dstderr;

  assert(!d->tail->prev);
  assert(!d->tail->next);

  if(root->tail->next)
    root->tail->next->tail->prev = d;

  d->tail->next = root->tail->next;
  d->tail->prev = root;

  root->tail->next = d;

  d_unlocklist();
}
static void remove_dfile_list(DFILE * d) {
  d_locklist();

  assert(d != dstdin);
  assert(d != dstdout);
  assert(d != dstderr);

  d->tail->prev->tail->next = d->tail->next;
  if(d->tail->next)
    d->tail->next->tail->prev = d->tail->prev;

  d->tail->next = NULL;
  d->tail->prev = NULL;

  d_unlocklist();
}
static void reseat_dfile_list(int before_flags, DFILE * d) {
  if(d == dstdin ||
     d == dstdout ||
     d == dstderr)
    return;
  bool before_linebuf = before_flags & DFILE_LINE_BUFFERED;
  bool after_linebuf = d->flags & DFILE_LINE_BUFFERED;
  if(before_linebuf != after_linebuf) {
    d_locklist();
    remove_dfile_list(d);
    insert_dfile_list(d);
    d_unlocklist();
  }
}
static int flush_dfile_list(bool linebuf_only) {
  int test = linebuf_only ? DFILE_LINE_BUFFERED : 0;
  test |= DFILE_WRITE;
  d_locklist();
  DFILE * end = linebuf_only ? dstderr->tail->next : NULL;
  DFILE * cur = dstdin;
  int ret = 0;
  while(cur != end) {
    if((cur->flags & test) == test) {
      d_flockfile(cur);
      ret |= d_fflush_unlocked(cur);
      d_funlockfile(cur);
    }
    cur = cur->tail->next;
  }
  d_unlocklist();
  return ret;
}

#ifdef __linux__
static pthread_mutexattr_t recursive_attr;
#endif
static void init_dfile_lock(DFILE_TAIL * tail) {
#ifdef __linux__
  pthread_mutex_init(&tail->lock, &recursive_attr);
#else
  InitializeCriticalSectionAndSpinCount(&tail->lock, 32);
#endif
}
static int destroy_dfile_lock(DFILE_TAIL * tail) {
#ifdef __linux__
  return pthread_mutex_destroy(&tail->lock);
#else
  DeleteCriticalSection(&tail->lock);
  return 0;
#endif
}

static void init_dfile_tail(DFILE * f) {
  init_dfile_lock(f->tail);
  insert_dfile_list(f);
}
static int destroy_dfile_tail(DFILE * f) {
  remove_dfile_list(f);
  return destroy_dfile_lock(f->tail);
}

__attribute__((constructor(101)))
static void init_stdio() {
#ifdef __linux__
  pthread_mutexattr_init(&recursive_attr);
  pthread_mutexattr_settype(&recursive_attr, PTHREAD_MUTEX_RECURSIVE);
#endif
  
  init_dfile_lock(&dlist_mutex);

  init_dfile_lock(dstdin_impl.f.tail);
  init_dfile_lock(dstdout_impl.f.tail);
  init_dfile_lock(dstderr_impl.f.tail);
}

__attribute__((destructor(101)))
static void flush_stdio() {
#ifdef __linux__
  pthread_mutexattr_destroy(&recursive_attr);
#endif

  //d_fflush_unlocked(dstdin);
  //d_fflush_unlocked(dstdout);
  //d_fflush_unlocked(dstderr);
  flush_dfile_list(false);
  
  destroy_dfile_lock(&dlist_mutex);
}

static off64_t dseek(DFILE * f, off64_t offset, int whence) {
  if(f->flags & DFILE_STRFILE) {
    int newtell;
    switch(whence) {
      case D_SEEK_SET:
        newtell = offset;
        break;
      case D_SEEK_END:
        newtell = f->len + offset;
        break;
      case D_SEEK_CUR:
        newtell = f->tell + offset;
        break;
      default:
        return -1;
    }
    if(newtell < 0 || newtell > f->len)
      return -1;
    f->tell = newtell;
    return f->tell;
  } else if(f->flags & DFILE_COOKIE) {
    if(!f->funcs.seek) {
      return -1;
    } else {
      int ret = f->funcs.seek(f->cookie, &offset, whence);
      if(ret < 0)
        return ret;
      else
        return offset;
    }
  } else {
    return lseek64(f->fd, offset, whence);
  }
}

long long int d_ftell(DFILE * f) {
  d_flockfile(f);
  off_t o = dseek(f, 0, D_SEEK_CUR);
  d_funlockfile(f);
  if(o < 0) return o;
  return o - f->buf_cursor - f->num_ungets + f->dirty_cursor;
}

int d_fgetpos(DFILE * f, off64_t *pos) {
  *pos = d_ftell(f);
  return -(*pos < 0);
}
int d_fsetpos(DFILE * f, off64_t *pos) {
  d_flockfile(f);
  int ret = d_fseek(f, *pos, D_SEEK_SET);
  d_funlockfile(f);
  return ret;
}

int d_feof_unlocked(DFILE * f) {
  return f->flags & DFILE_EOF;
}

int d_ferror_unlocked(DFILE * f) {
  return f->flags & DFILE_ERROR;
}

void d_clearerror_unlocked(DFILE * f) {
  f->flags &= ~(DFILE_EOF | DFILE_ERROR);
}

void d_rewind(DFILE * f) {
  d_flockfile(f);
  d_fseek(f, 0, D_SEEK_SET);
  d_clearerror_unlocked(f);
  d_funlockfile(f);
}

static DFILE * d_fdopen_impl(int fd, char const * mode, DFILE * ret) {
  int bitfield = 0;
  switch(mode[0]) {
  case 'r':
    bitfield |= DFILE_READ;
    break;
  case 'w':
    bitfield |= DFILE_WRITE;
    break;
  case 'a':
    bitfield |= DFILE_WRITE | DFILE_APPEND;
    break;
  }
  if(strchr(mode, '+'))
    bitfield |= DFILE_READ | DFILE_WRITE;

  if(isatty(fd))
    bitfield |= DFILE_LINE_BUFFERED;

  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = fd,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield,
    .buf_size = D_BUFSIZ,
    .buf = ret->buf_storage,
  };
  return ret;
}
DFILE * d_fdopen(int fd, char const * mode) {
  DFILE * ret = malloc_dfile();
  if(!d_fdopen_impl(fd, mode, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

static DFILE * d_fopencookie_impl(void * cookie, char const * mode, d_cookie_io_functions_t funcs, DFILE * ret) {
  int bitfield = DFILE_COOKIE;
  switch(mode[0]) {
  case 'r':
    bitfield |= DFILE_READ;
    break;
  case 'w':
    bitfield |= DFILE_WRITE;
    break;
  case 'a':
    bitfield |= DFILE_WRITE | DFILE_APPEND;
    break;
  }
  if(strchr(mode, '+'))
    bitfield |= DFILE_READ | DFILE_WRITE;

  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = -1,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield,
    .buf_size = D_BUFSIZ,
    .buf = ret->buf_storage,
    .cookie = cookie,
    .funcs = funcs,
  };
  return ret;
}
DFILE * d_fopencookie(void * cookie, char const * mode, d_cookie_io_functions_t funcs) {
  DFILE * ret = malloc_dfile();
  if(!d_fopencookie_impl(cookie, mode, funcs, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

int d_setvbuf(DFILE * f, char * buf, int mode, size_t size) {
  d_flockfile(f);
  d_fflush_unlocked(f);
  int oldflags = f->flags;
  f->flags &= ~(DFILE_LINE_BUFFERED | DFILE_UNBUFFERED);
  switch(mode) {
  case D_IONBF:
    f->flags |= DFILE_UNBUFFERED;
    break;
  case D_IOLBF:
    f->flags |= DFILE_LINE_BUFFERED;
    break;
  case D_IOFBF:
    /* absense of flags means fully buffered */
    break;
  }
  if(buf) {
    f->buf = buf;
    f->buf_size = size;
  } else {
    f->buf = f->buf_storage;
    f->buf_size = D_BUFSIZ;
  }
  reseat_dfile_list(oldflags, f);
  d_funlockfile(f);
  return 0;
}
void d_setbuf(DFILE * f, char buf[D_BUFSIZ]) {
  d_setvbuf(f, buf, buf ? D_IOFBF : D_IONBF, D_BUFSIZ);
}
void d_setbuffer(DFILE * f, char * buf, size_t size) {
  d_setvbuf(f, buf, buf ? D_IOFBF : D_IONBF, size);
}
void d_setlinebuf(DFILE * f) {
  d_setvbuf(f, NULL, D_IOLBF, 0);
}

int d_fileno_unlocked(DFILE * f) {
  d_flockfile(f);
  int ret = f->fd;
  d_funlockfile(f);
  return ret;
}

//////////////////////////////////////////
//               MEMFILE                //
//////////////////////////////////////////

typedef struct memfile_cookie {
  size_t len;
  off64_t tell;
  off64_t maxtell;
  bool owns_buf;
  bool ignore_overflow;
  char * buf;
} memfile_cookie;
static ssize_t write_memfile(void * _cookie, char const * ptr, size_t nbytes) {
  size_t nbytes_original = nbytes;
  memfile_cookie * cookie = _cookie;

  if(cookie->len - cookie->tell < nbytes)
    nbytes = cookie->len - cookie->tell;

  memcpy(cookie->buf + cookie->tell, ptr, nbytes);
  cookie->tell += nbytes;

  cookie->maxtell = cookie->maxtell > cookie->tell ? cookie->maxtell : cookie->tell;
  if(cookie->maxtell < cookie->len)
    cookie->buf[cookie->maxtell] = '\0';

  return cookie->ignore_overflow ? nbytes_original : nbytes;
}
static ssize_t read_memfile(void * _cookie, char * ptr, size_t nbytes) {
  size_t nbytes_original = nbytes;
  memfile_cookie * cookie = _cookie;

  if(cookie->len - cookie->tell < nbytes)
    nbytes = cookie->len - cookie->tell;

  memcpy(ptr, cookie->buf + cookie->tell, nbytes);
  cookie->tell += nbytes;

  if(cookie->ignore_overflow)
    memset(ptr + nbytes, 0, nbytes_original - nbytes);
  return cookie->ignore_overflow ? nbytes_original : nbytes;
}
static int seek_memfile(void * _cookie, off64_t * offset, int whence) {
  memfile_cookie * cookie = _cookie;
  off64_t newtell;
  switch(whence) {
    case D_SEEK_SET:
      newtell = *offset;
      break;
    case D_SEEK_END:
      newtell = cookie->maxtell + *offset;
      break;
    case D_SEEK_CUR:
      newtell = cookie->tell + *offset;
      break;
    default:
      return -1;
  }
  if(newtell < 0 || (!cookie->ignore_overflow && newtell > cookie->len))
    return -1;
  cookie->tell = newtell;
  *offset = newtell;
  return 0;
}
static int close_memfile(void * _cookie) {
  memfile_cookie * cookie = _cookie;
  if(cookie->owns_buf)
    free(cookie->buf);
  free(_cookie);
  return 0;
}
static DFILE * d_fmemopen_impl(void * buf, size_t size, char const * _mode, DFILE * ret) {
  char mode[4];
  int idx = 0;
  mode[idx++] = _mode[0];
  if(strchr(_mode, 'b'))
    mode[idx++] = 'b';
  if(strchr(_mode, '+'))
    mode[idx++] = '+';
  mode[idx] = '\0';
  bool ignore_overflow = strchr(_mode, '0');

  bool owns_buf = false;
  if(!buf && size) {
    buf = malloc(size);
    owns_buf = true;
  }

  if(owns_buf || (mode[0] == 'w' && size))
    ((char*)buf)[0] = 0;

  size_t maxtell = 0;
  switch(mode[0]) {
  case 'r':
    maxtell = size;
    break;
  case 'w':
    maxtell = 0;
    break;
  case 'a':
    {
      void * end = memchr(buf, '\0', size);
      if(end)
        maxtell = end - buf;
      else
        maxtell = size;
      break;
    }
  }

  memfile_cookie * cookie = malloc(sizeof(memfile_cookie));
  *cookie = (memfile_cookie) {
    .buf = buf,
    .tell = mode[0] == 'a' ? maxtell : 0,
    .maxtell = maxtell,
    .len = size,
    .owns_buf = owns_buf,
    .ignore_overflow = ignore_overflow,
  };

  d_cookie_io_functions_t funcs = {
    .read = read_memfile,
    .write = write_memfile,
    .seek = seek_memfile,
    .close = close_memfile,
  };

  return d_fopencookie_impl(cookie, mode, funcs, ret);
}
DFILE * d_fmemopen(void * buf, size_t size, char const * mode) {
  DFILE * ret = malloc_dfile();
  if(!d_fmemopen_impl(buf, size, mode, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

//////////////////////////////////////////
//              MEMSTREAM               //
//////////////////////////////////////////

typedef struct memstream_cookie {
  size_t len;
  size_t *maxtell;
  size_t tell;
  char ** buf;
} memstream_cookie;
static ssize_t write_memstream(void * _cookie, char const * ptr, size_t nbytes) {
  memstream_cookie * cookie = _cookie;

  if(cookie->len - cookie->tell <= nbytes) {
    size_t newlen = cookie->len ? 2* cookie->len : 1;
    while(newlen - cookie->tell <= nbytes)
      newlen *= 2;
    *cookie->buf = realloc(*cookie->buf, newlen);
    cookie->len = newlen;
  }

  memcpy(*cookie->buf + cookie->tell, ptr, nbytes);
  cookie->tell += nbytes;
  *cookie->maxtell = *cookie->maxtell > cookie->tell ? *cookie->maxtell : cookie->tell;
  (*cookie->buf)[*cookie->maxtell] = '\0';

  return nbytes;
}
static int seek_memstream(void * _cookie, off64_t * offset, int whence) {
  memstream_cookie * cookie = _cookie;
  off64_t newtell;
  switch(whence) {
    case D_SEEK_SET:
      newtell = *offset;
      break;
    case D_SEEK_END:
      newtell = *cookie->maxtell + *offset;
      break;
    case D_SEEK_CUR:
      newtell = cookie->tell + *offset;
      break;
    default:
      return -1;
  }
  if(newtell < 0)
    return -1;
  if(newtell >= cookie->len) {
    size_t newlen = cookie->len ? 2* cookie->len : 1;
    while(newlen <= cookie->tell)
      newlen *= 2;
    *cookie->buf = realloc(*cookie->buf, newlen);
    cookie->len = newlen;
  }
  if(newtell > *cookie->maxtell) {
    memset(*cookie->buf + *cookie->maxtell, 0, newtell + 1 - *cookie->maxtell);
    *cookie->maxtell = newtell;
  }
  cookie->tell = newtell;
  *offset = newtell;
  return 0;
}
static int close_memstream(void * _cookie) {
  memstream_cookie * cookie = _cookie;
  *cookie->buf = realloc(*cookie->buf, *cookie->maxtell+1);
  free(_cookie);
  return 0;
}
static DFILE * d_open_memstream_impl(char ** buf, size_t * tell, DFILE * ret) {
  *buf = malloc(1);
  **buf = '\0';
  *tell = 0;
  memstream_cookie * cookie = malloc(sizeof(memstream_cookie));
  *cookie = (memstream_cookie) {
    .buf = buf,
    .maxtell = tell,
    .tell = 0,
    .len = 0,
  };

  d_cookie_io_functions_t funcs = {
    .read = NULL,
    .write = write_memstream,
    .seek = seek_memstream,
    .close = close_memstream,
  };

  return d_fopencookie_impl(cookie, "w", funcs, ret);
}
DFILE * d_open_memstream(char ** buf, size_t * tell) {
  DFILE * ret = malloc_dfile();
  if(!d_open_memstream_impl(buf, tell, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

//////////////////////////////////////////
//                 MISC                 //
//////////////////////////////////////////

DFILE * d_fopen_impl(char const * path, char const * mode, DFILE * f) {
  int flags = 0;
  bool plus = strchr(mode, '+');
  switch(mode[0]) {
  case 'r':
    if(plus)
      flags = O_RDWR;
    else
      flags = O_RDONLY;
    break;
  case 'w':
    if(plus)
      flags = O_RDWR | O_CREAT | O_TRUNC;
    else
      flags = O_WRONLY | O_CREAT | O_TRUNC;
    break;
  case 'a':
    if(plus)
      flags = O_RDWR | O_CREAT | O_APPEND;
    else
      flags = O_WRONLY | O_CREAT | O_APPEND;
    break;
  }

  int fd = open(path, flags);
  if(fd < 0) return NULL;
  return d_fdopen_impl(fd, mode, f);
}
DFILE * d_fopen(char const * path, char const * mode) {
  DFILE * ret = malloc_dfile();
  if(!d_fopen_impl(path, mode, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

static DFILE * d_tmpfile_impl(DFILE * f) {
#ifdef _WIN64
  // because tmpfile() creates its files in the root folder of the volume
  // and the root C: was changed in Vista to readonly
  // and tmpfile() was left hanging.
  char buf[PATH_MAX];
  GetTempPathA(sizeof buf, buf);
  char * name = _tempnam(buf, "tmp");
  if(!name)
    return NULL;
  enum { FILE_FLAG_DELETE_ON_CLOSE = 0x04000000 };
  HANDLE h = CreateFileA( name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL );
  free(name);
  if((intptr_t)h == (intptr_t)-1)
    return NULL;
  int fd = _open_osfhandle((intptr_t)h, O_RDWR);
  if(fd == -1)
    return NULL;
#endif
#ifdef __linux__
  int fd = open("/tmp", O_TMPFILE | O_RDWR, 0666);
  if(fd < 0) return NULL;
#endif
  return d_fdopen_impl(fd, "r+", f);
}

DFILE * d_tmpfile() {
  DFILE * ret = malloc_dfile();
  if(!d_tmpfile_impl(ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

static DFILE * d_strfile_impl(DFILE * ret) {
  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = -1,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = DFILE_READ | DFILE_WRITE | DFILE_STRFILE,
    .buf_size = D_BUFSIZ,
    .buf = ret->buf_storage,
  };
  return ret;
}

DFILE * d_strfile() {
  DFILE * ret = malloc_dfile();
  if(!d_strfile_impl(ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}

static int write_strfile(DFILE * f, char const * ptr, int nbytes) {
  assert(f->canary == DFILE_CANARY);
  STRPAGE ** page_place = &f->strpages;
  STRPAGE * page = f->strpages;
  off_t tell = f->tell;
  int ret = nbytes;
  while(tell >= D_BUFSIZ) {
    page_place = &page->next;
    page = page->next;
    tell -= D_BUFSIZ;
  }
  while(nbytes) {
    if(!page) {
      assert(tell == 0);
      *page_place = page = malloc(sizeof(STRPAGE));
      memset(page, 0, sizeof(STRPAGE));
    }
    int towrite = D_BUFSIZ - tell;
    if(towrite > nbytes) towrite = nbytes;
    memcpy(page->buf + tell, ptr, towrite);
    tell += towrite;
    f->tell += towrite;
    ptr += towrite;
    nbytes -= towrite;
    if(tell >= D_BUFSIZ) {
      page_place = &page->next;
      page = page->next;
      tell -= D_BUFSIZ;
    }
  }
  if(f->len < f->tell)
    f->len = f->tell;
  return ret;
}

static int read_strfile(DFILE * f, char * ptr, int nbytes) {
  assert(f->canary == DFILE_CANARY);
  off_t tell = f->tell;
  if(f->len - tell < nbytes)
    nbytes = f->len - tell;
  int ret = nbytes;
  STRPAGE * page = f->strpages;
  while(tell >= D_BUFSIZ) {
    page = page->next;
    tell -= D_BUFSIZ;
  }
  while(nbytes) {
    assert(page);
    int toread = D_BUFSIZ - tell;
    if(toread > nbytes) toread = nbytes;
    memcpy(ptr, page->buf + tell, toread);
    tell += toread;
    f->tell += toread;
    ptr += toread;
    nbytes -= toread;
    if(tell >= D_BUFSIZ) {
      page = page->next;
      tell -= D_BUFSIZ;
    }
  }
  return ret;
}

static int d_fflush_unlocked_impl(DFILE * f, int flushbytes) {
  assert(f->canary == DFILE_CANARY);
  void * ptr = f->buf;
  int nbytes = flushbytes;
  if(f->buf_cursor) {
    dseek(f, -f->buf_cursor, D_SEEK_CUR);
    f->buf_cursor = 0;
  }
  if(f->flags & DFILE_STRFILE) {
    write_strfile(f, ptr, nbytes);
  } else if(f->flags & DFILE_COOKIE) {
    if(f->funcs.write) {
      int ret = f->funcs.write(f->cookie, ptr, nbytes);
      if(ret < 0)
        return -1;
    }
  } else {
    while(nbytes) {
      int ret = write(f->fd, ptr, nbytes);
      if(ret < 0) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
          f->flags |= DFILE_ERROR;
          return -1;
        }
        errno = 0;
      } else {
        nbytes -= ret;
        ptr += ret;
      }
    }
  }
  memmove(f->buf, f->buf + flushbytes, f->dirty_cursor - flushbytes);
  f->dirty_cursor -= flushbytes;
  return 0;
}

int d_fflush_unlocked(DFILE * f) {
  if(!f)
    return flush_dfile_list(false);

  if(f->dirty_cursor) {
    if(d_fflush_unlocked_impl(f, f->dirty_cursor) < 0)
      return -1;
  }
  if(f->num_ungets) {
    int ret = dseek(f, -f->num_ungets - f->buf_cursor, D_SEEK_CUR);
    f->num_ungets = 0;
    f->buf_cursor = 0;
    if(ret < 0)
      return -1;
  }
  return 0;
}

int d_fseek(DFILE * f, int offset, int whence) {
  assert(f->canary == DFILE_CANARY);
  d_flockfile(f);
  if(d_fflush_unlocked(f) < 0)
    goto failure;
  if(whence == D_SEEK_CUR) {
    int ret = dseek(f, offset - f->buf_cursor, D_SEEK_CUR);
    f->buf_cursor = 0;
    if(ret >= 0)
      goto success;
  }
  if(whence == D_SEEK_SET || whence == D_SEEK_END) {
    f->buf_cursor = 0;
    int ret = dseek(f, offset, whence);
    if(ret >= 0)
      goto success;
  }
  goto failure;
success:
  d_funlockfile(f);
  return 0;
failure:
  f->flags |= DFILE_ERROR;
  d_funlockfile(f);
  return -1;
}

static int d_fclose_impl(DFILE * f) {
  d_fflush_unlocked(f);
  int ret;
  if(f->flags & DFILE_STRFILE) {
    STRPAGE * page = f->strpages;
    while(page) {
      STRPAGE * next = page->next;
      free(page);
      page = next;
    }
    ret = 0;
  } else if(f->flags & DFILE_COOKIE) {
    if(f->funcs.close)
      ret = f->funcs.close(f->cookie);
  } else {
    ret = close(f->fd);
  }

  if(f->flags & DFILE_PROCESS) {
#ifdef __linux__
    if(waitpid(f->process, &ret, 0) < 0)
      errno = ECHILD;
#endif
#ifdef _WIN64
    WaitForSingleObject(f->process, -1);
    DWORD ret2;
    if(!GetExitCodeProcess(f->process, &ret2))
      ret2 = -1;
    CloseHandle(f->process);
    CloseHandle(f->thread);
    ret = ret2;
#endif
  }
  return ret;
}

int d_fclose(DFILE * f) {
  int ret = d_fclose_impl(f);
  if(destroy_dfile_tail(f))
    ret = -1;
  free(f);
  return ret;
}
int d_pclose(DFILE * f) {
  return d_fclose(f);
}

DFILE * d_freopen(char const * path, char const * mode, DFILE * stream) {
  // permitted to just fail here, so will for cross platform
  if(!path)
    goto fail;
    
  int oldflags = stream->flags;
  d_fclose_impl(stream);

  if(!d_fopen_impl(path, mode, stream))
    goto fail;
  reseat_dfile_list(oldflags, stream);

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr) {
    remove_dfile_list(stream);
    free(stream);
  }
  return NULL;
}

DFILE * d_fdreopen(int fd, char const * mode, DFILE * stream) {
  int oldflags = stream->flags;
  d_fclose_impl(stream);

  if(!d_fdopen_impl(fd, mode, stream))
    goto fail;
  reseat_dfile_list(oldflags, stream);

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr) {
    remove_dfile_list(stream);
    free(stream);
  }
  return NULL;
}

DFILE * d_retmpfile(DFILE * stream) {
  int oldflags = stream->flags;
  d_fclose_impl(stream);

  if(!d_tmpfile_impl(stream))
    goto fail;
  reseat_dfile_list(oldflags, stream);

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr) {
    remove_dfile_list(stream);
    free(stream);
  }
  return NULL;
}
DFILE * d_restrfile(DFILE * stream) {
  int oldflags = stream->flags;
  d_fclose_impl(stream);

  if(!d_strfile_impl(stream))
    goto fail;
  reseat_dfile_list(oldflags, stream);

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr) {
    remove_dfile_list(stream);
    free(stream);
  }
  return NULL;
}

DFILE * d_freopencookie(void * cookie, char const * mode, d_cookie_io_functions_t funcs, DFILE * stream) {
  int oldflags = stream->flags;
  d_fclose_impl(stream);

  if(!d_fopencookie_impl(cookie, mode, funcs, stream))
    goto fail;
  reseat_dfile_list(oldflags, stream);

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr) {
    remove_dfile_list(stream);
    free(stream);
  }
  return NULL;
}

DFILE * d_fmemreopen(void * buf, size_t size, char const * _mode, DFILE * f) {
  int oldflags = f->flags;
  if(f->flags & DFILE_COOKIE &&
     f->funcs.read == read_memfile &&
     f->funcs.write == write_memfile &&
     f->funcs.seek == seek_memfile &&
     f->funcs.close == close_memfile) {
    d_fflush_unlocked(f);
    memfile_cookie * cookie = f->cookie;
    if(cookie->owns_buf)
      free(cookie->buf);
    char mode[4];
    int idx = 0;
    mode[idx++] = _mode[0];
    if(strchr(_mode, 'b'))
      mode[idx++] = 'b';
    if(strchr(_mode, '+'))
      mode[idx++] = '+';
    mode[idx] = '\0';
    bool ignore_overflow = strchr(_mode, '0');

    bool owns_buf = false;
    if(!buf && size) {
      buf = malloc(size);
      owns_buf = true;
    }

    if(owns_buf || (mode[0] == 'w' && size))
      ((char*)buf)[0] = 0;

    size_t maxtell = 0;
    switch(mode[0]) {
    case 'r':
      maxtell = size;
      break;
    case 'w':
      maxtell = 0;
      break;
    case 'a':
      {
        void * end = memchr(buf, '\0', size);
        if(end)
          maxtell = end - buf;
        else
          maxtell = size;
        break;
      }
    }

    *cookie = (memfile_cookie) {
      .buf = buf,
      .tell = mode[0] == 'a' ? maxtell : 0,
      .maxtell = maxtell,
      .len = size,
      .owns_buf = owns_buf,
    };

    if(!d_fopencookie_impl(cookie, mode, f->funcs, f))
      goto fail;
  } else {
    d_fclose_impl(f);

    if(!d_fmemopen_impl(buf, size, _mode, f))
      goto fail;
  }
  reseat_dfile_list(oldflags, f);
  return f;

fail:
  if(f != dstdout && f != dstdin && f != dstderr) {
    remove_dfile_list(f);
    free(f);
  }
  return NULL;
}

DFILE * d_reopen_memstream(char ** buf, size_t * tell, DFILE * f) {
  int oldflags = f->flags;
  if(f->flags & DFILE_COOKIE &&
     f->funcs.read == NULL &&
     f->funcs.write == write_memstream &&
     f->funcs.seek == seek_memstream &&
     f->funcs.close == close_memstream) {
    d_fflush_unlocked(f);
    memstream_cookie * cookie = f->cookie;

    *buf = malloc(1);
    **buf = '\0';
    *tell = 0;
    *cookie = (memstream_cookie) {
      .buf = buf,
      .maxtell = tell,
      .tell = 0,
      .len = 0,
    };

    if(!d_fopencookie_impl(cookie, "w", f->funcs, f))
      goto fail;
  } else {
    d_fclose_impl(f);

    if(!d_open_memstream_impl(buf, tell, f))
      goto fail;
  }
  reseat_dfile_list(oldflags, f);
  return f;

fail:
  if(f != dstdout && f != dstdin && f != dstderr) {
    remove_dfile_list(f);
    free(f);
  }
  return NULL;
}

int d_fwrite_unlocked(const void * ptr, int ct, DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  if(!(f->flags & DFILE_WRITE)) {
    f->flags |= DFILE_ERROR;
    return -1;
  }
  if(f->num_ungets) {
    if(d_fflush_unlocked(f) < 0)
      return -1;
  }
  if(f->flags & DFILE_APPEND) {
    if(d_fseek(f, 0, D_SEEK_END) < 0)
      return -1;
  }

  int ret = 0;
  while(ct) {
    if(f->dirty_cursor == f->buf_size) {
      if(d_fflush_unlocked(f) < 0)
        break;
    }
    int nbytes = f->buf_size - f->dirty_cursor;
    if(ct < nbytes) nbytes = ct;

    memcpy(f->buf + f->dirty_cursor, ptr, nbytes);
    ret += nbytes;
    ct -= nbytes;
    ptr += nbytes;
    f->dirty_cursor += nbytes;
  }
  if(f->flags & DFILE_UNBUFFERED) {
    int ret = d_fflush_unlocked(f);
    if(ret < 0) {
      f->flags |= DFILE_ERROR;
      return ret;
    }
  }
  else if(f->flags & DFILE_LINE_BUFFERED) {
    bool found = false;
    char * ptr = f->buf + f->dirty_cursor;
    while(ptr --> f->buf) {
      if(*ptr == '\n') {
        found = true;
        break;
      }
    }
    if(found) {
      int ret = d_fflush_unlocked_impl(f, (char*)ptr - f->buf + 1);
      if(ret < 0) {
        f->flags |= DFILE_ERROR;
        return ret;
      }
    }
  }
  return ret;
}

static int dfbuffer(DFILE * f, int ct) {
  assert(f->canary == DFILE_CANARY);
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return -1;
  }
  if(d_fflush_unlocked(f) < 0)
    return -1;
  if(f->buf_cursor == f->buf_size)
    return 0;
  int ret = -1;
  if(f->flags & (DFILE_LINE_BUFFERED))
    flush_dfile_list(true);

  if(f->flags & DFILE_UNBUFFERED) {
    if(ct > f->buf_size - f->buf_cursor)
      ct = f->buf_size - f->buf_cursor;
  } else {
    ct = f->buf_size - f->buf_cursor;
  }

  if(f->flags & DFILE_STRFILE) {
    ret = read_strfile(f, f->buf + f->buf_cursor, ct);
  } else if(f->flags & DFILE_COOKIE) {
    if(!f->funcs.read)
      ret = 0;
    else
      ret = f->funcs.read(f->cookie, f->buf + f->buf_cursor, ct);
  } else {
    while(ret < 0) {
      // relying on termios to not be retarded
      ret = read(f->fd, f->buf + f->buf_cursor, ct);
      if(ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return ret;
    }
  }
  f->buf_cursor += ret;
  return ret;
}

int d_fread_unlocked(void * ptr, int ct, DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return 0;
  }

  int nread = 0;
  while(ct && f->num_ungets) {
    *(char*)ptr = f->ungets[--f->num_ungets];
    ct -= 1;
    ptr += 1;
    nread += 1;
  }
  if(!ct)
    return nread;
  // dirty_cursor is now 0
  if(d_fflush_unlocked(f) < 0)
    return 0;
  while(ct) {
    if(f->buf_cursor) {
      int nbytes = ct < f->buf_cursor ? ct : f->buf_cursor;
      memcpy(ptr, f->buf, nbytes);
      memmove(f->buf, f->buf + nbytes, f->buf_cursor - nbytes);
      ct -= nbytes;
      ptr += nbytes;
      nread += nbytes;
      f->buf_cursor -= nbytes;
    }
    if(ct) {
      int bufret = dfbuffer(f, ct);
      if(bufret <= 0) {
        if(bufret == 0)
          f->flags |= DFILE_EOF;
        else
          f->flags |= DFILE_ERROR;
        return nread;
      }
    }
  }
  return nread;
}

char * d_fgets_unlocked(char * buf, int ct, DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  char * ret = buf;
  bool any_read = false;
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return NULL;
  }
  int nread = 0;
  bool satisfied = false;
  while(!satisfied && ct && f->num_ungets) {
    char c = *buf = f->ungets[--f->num_ungets];
    ct -= 1;
    buf += 1;
    nread += 1;
    any_read = true;
    if(c == '\n') {
      satisfied = true;
      break;
    }
  }
  // dirty_cursor is now 0
  if(satisfied) {
    if(ct)
      buf[0] = 0;
    return ret;
  }
  if(d_fflush_unlocked(f) < 0)
    return NULL;
  while(!satisfied && ct > 1) {
    if(f->buf_cursor) {
      int nbytes = 0;
      while(nbytes < f->buf_cursor) {
        if(f->buf[nbytes++] == '\n') {
          satisfied = true;
          break;
        }
      }
      if(nbytes > ct - 1)
        nbytes = ct - 1;
      memcpy(buf, f->buf, nbytes);
      memmove(f->buf, f->buf + nbytes, f->buf_cursor - nbytes);
      ct -= nbytes;
      buf += nbytes;
      nread += nbytes;
      any_read = true;
      f->buf_cursor -= nbytes;
    }
    if(!satisfied && ct > 1) {
      int bufret = dfbuffer(f, 1);
      if(bufret <= 0) {
        if(bufret == 0)
          f->flags |= DFILE_EOF;
        else
          f->flags |= DFILE_ERROR;
        buf[0] = 0;
        return bufret < 0 ? NULL :
               any_read ? ret : NULL;
      }
    }
  }
  if(ct)
    buf[0] = 0;
  return ret;
}

static DFILE * d_popen_impl(const char * cmd, const char *type, DFILE * f) {
  bool is_read = !strcmp(type, "r");
#ifdef __linux__
  // 0 is read end 1 is write end
  int fds[2];
  if(pipe(fds)) return NULL;
  pid_t pid = vfork();
  if(pid < 0) return NULL;
  if(pid) {
    // parent
    close(fds[is_read]);
    DFILE * ret = d_fdopen_impl(fds[!is_read], type, f);
    if(ret) {
      ret->flags |= DFILE_PROCESS;
      ret->process = pid;
    }
    return ret;
  } else {
    // child
    close(fds[!is_read]);
    if(dup2(fds[is_read], is_read) < 0) // 0 is stdin and 1 is stdout
      _exit(1);
    execl("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(1);
    return NULL;
  }
#endif
#ifdef _WIN64
  SECURITY_ATTRIBUTES sa = {
    .nLength = sizeof sa,
    .bInheritHandle = TRUE,
  };

  HANDLE hReadEnd = NULL, hWriteEnd = NULL;
  if(!CreatePipe(&hReadEnd, &hWriteEnd, &sa, 0))
    return NULL;

  STARTUPINFO si = {
    .cb = sizeof si,
    .dwFlags = 0x00000100, // STARTF_USESTDHANDLES
  };
  if(is_read) {
    si.hStdOutput = hWriteEnd;
    SetHandleInformation(hReadEnd, 0x00000001, 0);
  } else {
    si.hStdInput = hReadEnd;
    SetHandleInformation(hWriteEnd, 0x00000001, 0);
  }
  PROCESS_INFORMATION pi = {0};
//#define FORMAT "cmd.exe /C %s"
#define FORMAT "cmd.exe /C "
  char * cmdline = malloc(strlen(cmd)+sizeof FORMAT);
//  sprintf(cmdline, FORMAT, cmd);
  strcpy(cmdline, FORMAT);
  strcat(cmdline, cmd);
  if(!CreateProcess(
       NULL,
       cmdline,
       NULL,
       NULL,
       TRUE,
       0,
       NULL,
       NULL,
       &si,
       &pi)) {
    free(cmdline);
    return NULL;
  }
  free(cmdline);

  int fd;
  if(is_read) {
    fd = _open_osfhandle((intptr_t)hReadEnd, 0);
    CloseHandle(hWriteEnd);
  } else {
    CloseHandle(hReadEnd);
    fd = _open_osfhandle((intptr_t)hWriteEnd, 0);
  }
  DFILE * ret = d_fdopen_impl(fd, type, f);
  if(ret) {
    ret->flags |= DFILE_PROCESS;
    ret->process = pi.hProcess;
    ret->thread = pi.hThread;
  }
  return ret;
#endif
  return NULL;
}
DFILE * d_popen(const char * cmd, const char *type) {
  DFILE * ret = malloc_dfile();
  if(!d_popen_impl(cmd, type, ret)) {
    free(ret);
    return NULL;
  }
  init_dfile_tail(ret);
  return ret;
}
DFILE * d_preopen(char const * cmd, char const * mode, DFILE * stream) {
  d_fclose_impl(stream);

  if(!d_popen_impl(cmd, mode, stream))
    goto fail;

  return stream;
  
fail:
  if(stream != dstdout && stream != dstdin && stream != dstderr)
    free(stream);
  return NULL;
}

int d_ungetc(int c, DFILE * f) {
  d_flockfile(f);
  if(c == -1)
    goto failure;
  if(f->num_ungets < DFILE_UNGETS) {
    f->ungets[f->num_ungets++] = c;
    f->flags &= ~DFILE_EOF;
    goto success;
  }
  goto failure;
success:
  d_funlockfile(f);
  return c;
failure:
  d_funlockfile(f);
  return -1;
}

void d_flockfile(DFILE * f) {
#ifdef __linux__
  pthread_mutex_lock(&f->tail[0].lock);
#else
  EnterCriticalSection(&f->tail[0].lock);
#endif
}
int d_ftrylockfile(DFILE * f) {
#ifdef __linux__
  return pthread_mutex_trylock(&f->tail[0].lock) ? -1 : 0;
#else
  // wat.
  return TryEnterCriticalSection(&f->tail[0].lock) ? 0 : -1;
#endif
}
void d_funlockfile(DFILE * f) {
#ifdef __linux__
  pthread_mutex_unlock(&f->tail[0].lock);
#else
  LeaveCriticalSection(&f->tail[0].lock);
#endif
}

//////////////////////////////////////////
//              NICETIES                //
//////////////////////////////////////////

int d_fgetc_unlocked(DFILE * f) {
  char c;
  int ret = d_fread_unlocked(&c, 1, f);
  return ret <= 0 ? -1 : c;
}

int d_getc_unlocked(DFILE * f) {
  return d_fgetc_unlocked(f);
}

int d_getchar_unlocked() {
  return d_fgetc_unlocked(dstdin);
}

int d_fputc_unlocked(int c, DFILE * f) {
  unsigned char cc = c;
  return d_fwrite_unlocked(&cc, 1, f) < 0 ? -1 : cc;
}

int d_putc_unlocked(int c, DFILE * f) {
  return d_fputc_unlocked(c, f);
}

int d_putchar_unlocked(int c) {
  return d_fputc_unlocked(c, dstdout);
}

int d_fputs_unlocked(char const * str, DFILE * f) {
 size_t len = strlen(str);
 return d_fwrite_unlocked(str, len, f);
}

int d_puts(char const * str) {
  d_flockfile(dstdout);
  int ret = d_fputs_unlocked(str, dstdout);
  if(ret < 0)
    goto failure;
  int ret2 = d_putchar_unlocked('\n');
  if(ret2 < 0)
    goto failure;

  d_funlockfile(dstdout);
  return ret + 1;
failure:
  d_funlockfile(dstdout);
  return -1;
}

//////////////////////////////////////////
//               LOCKED                 //
//////////////////////////////////////////

#define IMPL_LOCKED_BASIC(T, name) \
  T d_ ## name(DFILE * f) { \
    d_flockfile(f); \
    T ret = d_ ## name ## _unlocked(f); \
    d_funlockfile(f); \
    return ret; \
  }
IMPL_LOCKED_BASIC(int, feof)
IMPL_LOCKED_BASIC(int, ferror)
void d_clearerror(DFILE * f) {
  d_flockfile(f);
  d_clearerror_unlocked(f);
  d_funlockfile(f);
}

IMPL_LOCKED_BASIC(int, fileno)
IMPL_LOCKED_BASIC(int, fflush)

int d_fwrite(const void * ptr, int ct, DFILE * f) {
  d_flockfile(f);
  int ret = d_fwrite_unlocked(ptr, ct, f);
  d_funlockfile(f);
  return ret;
}
int d_fread(void * ptr, int ct, DFILE * f) {
  d_flockfile(f);
  int ret = d_fread_unlocked(ptr, ct, f);
  d_funlockfile(f);
  return ret;
}
char * d_fgets(char * ptr, int ct, DFILE * f) {
  d_flockfile(f);
  char * ret = d_fgets_unlocked(ptr, ct, f);
  d_funlockfile(f);
  return ret;
}

IMPL_LOCKED_BASIC(int, fgetc)
IMPL_LOCKED_BASIC(int, getc)
int d_getchar() {
  return d_fgetc(dstdin);
}

int d_fputc(int c, DFILE * f) {
  d_flockfile(f);
  int ret = d_fputc_unlocked(c, f);
  d_funlockfile(f);
  return ret;
}
int d_putc(int c, DFILE * f) {
  d_flockfile(f);
  int ret = d_putc_unlocked(c, f);
  d_funlockfile(f);
  return ret;
}
int d_putchar(int c) {
  return d_fputc(c, dstdout);
}

int d_fputs(char const * ptr, DFILE * f) {
  d_flockfile(f);
  int ret = d_fputs_unlocked(ptr, f);
  d_funlockfile(f);
  return ret;
}
