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
#include <sys/wait.h>
#endif

#ifdef _WIN64
// SEEK_SET & FILENO here here on windings
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
// a
// a+

// tmpfile   DONE

// fclose    DONE
// fflush    DONE
// fopen     DONE
// freopen
// setbuf
// setvbuf

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

// fgetpos
// fseek     DONE
// fsetpos
// ftell     DONE
// rewind    DONE

// clearerr  DONE
// feof      DONE
// ferror    DONE

// popen     DONE
// pclose    DONE

// fprintf
// fscanf
// printf
// scanf
// snprintf
// sprintf
// sscanf
// vfprintf
// vfscanf
// fscanf
// vsnprintf
// vsprintf
// vsscanf

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
enum { DEFAULT_BUF_SIZE = 4096 };

enum { DFILE_CANARY = 0xDF11E83 };

typedef struct STRPAGE {
  struct STRPAGE * next;
  char buf[DEFAULT_BUF_SIZE];
} STRPAGE;

enum { DFILE_UNGETS = 2 };
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
  char buf_storage[DEFAULT_BUF_SIZE];
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
} DFILE;

DFILE dstdin_impl = {
  .canary = DFILE_CANARY,
  .fd = STDIN_FILENO,
  .flags = DFILE_READ,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstdin_impl.buf_storage,
};
DFILE dstdout_impl = {
  .canary = DFILE_CANARY,
  .fd = STDOUT_FILENO,
  .flags = DFILE_WRITE | DFILE_LINE_BUFFERED,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstdout_impl.buf_storage,
};
DFILE dstderr_impl = {
  .canary = DFILE_CANARY,
  .fd = STDERR_FILENO,
  .flags = DFILE_WRITE | DFILE_UNBUFFERED,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstderr_impl.buf_storage,
};
DFILE * dstdin = &dstdin_impl;
DFILE * dstdout = &dstdout_impl;
DFILE * dstderr = &dstderr_impl;

static off64_t dseek(DFILE * f, off64_t offset, int whence) {
  if(f->flags & DFILE_STRFILE) {
    int newtell;
    switch(whence) {
      case SEEK_SET:
        newtell = offset;
        break;
      case SEEK_END:
        newtell = f->len + offset;
        break;
      case SEEK_CUR:
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

long long int dftell(DFILE * f) {
  off_t o = dseek(f, 0, SEEK_CUR);
  if(o < 0) return o;
  return o - f->buf_cursor - f->num_ungets + f->dirty_cursor;
}

int dfeof(DFILE * f) {
  return f->flags & DFILE_EOF;
}

int dferror(DFILE * f) {
  return f->flags & DFILE_ERROR;
}

void dclearerror(DFILE * f) {
  f->flags &= ~(DFILE_EOF | DFILE_ERROR);
}

void drewind(DFILE * f) {
  dfseek(f, 0, SEEK_SET);
  dclearerror(f);
}

DFILE * dfdopen(int fd, char const * mode) {
  DFILE * ret = malloc(sizeof(DFILE));
  int bitfield = 0;
  if(!strcmp(mode, "w") || !strcmp(mode, "wb")) // doesn't truncate
    bitfield |= DFILE_WRITE;
  else if(!strcmp(mode, "r") || !strcmp(mode, "rb"))
    bitfield |= DFILE_READ;
  else if(!strcmp(mode, "r+") || !strcmp(mode, "rb+"))
    bitfield |= DFILE_READ | DFILE_WRITE;
  else if(!strcmp(mode, "w+") || !strcmp(mode, "wb+")) // w+ and r+ are  the same in fdopen since no truncation
    bitfield |= DFILE_READ | DFILE_WRITE;
  else
    return NULL;

  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = fd,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield,
    .buf_size = DEFAULT_BUF_SIZE,
    .buf = ret->buf_storage,
  };
  return ret;
}

DFILE * d_fopencookie(void * cookie, char const * mode, d_cookie_io_functions_t funcs) {
  DFILE * ret = malloc(sizeof(DFILE));
  int bitfield = DFILE_COOKIE;
  if(!strcmp(mode, "w") || !strcmp(mode, "wb")) // doesn't truncate
    bitfield |= DFILE_WRITE;
  else if(!strcmp(mode, "r") || !strcmp(mode, "rb"))
    bitfield |= DFILE_READ;
  else if(!strcmp(mode, "r+") || !strcmp(mode, "rb+"))
    bitfield |= DFILE_READ | DFILE_WRITE;
  else if(!strcmp(mode, "w+") || !strcmp(mode, "wb+")) // w+ and r+ are  the same in fdopen since no truncation
    bitfield |= DFILE_READ | DFILE_WRITE;
  else
    return NULL;

  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = -1,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield,
    .buf_size = DEFAULT_BUF_SIZE,
    .buf = ret->buf_storage,
    .cookie = cookie,
    .funcs = funcs,
  };
  return ret;
}

typedef struct memfile_cookie {
  size_t len;
  off64_t tell;
  bool owns_buf;
  char * buf;
} memfile_cookie;
static ssize_t write_memfile(void * _cookie, char const * ptr, size_t nbytes) {
  memfile_cookie * cookie = _cookie;

  if(cookie->len - cookie->tell < nbytes)
    nbytes = cookie->len - cookie->tell;

  memcpy(cookie->buf + cookie->tell, ptr, nbytes);
  cookie->tell += nbytes;

  if(cookie->tell < cookie->len)
    cookie->buf[cookie->tell] = '\0';

  return nbytes;
}
static ssize_t read_memfile(void * _cookie, char * ptr, size_t nbytes) {
  memfile_cookie * cookie = _cookie;

  if(cookie->len - cookie->tell < nbytes)
    nbytes = cookie->len - cookie->tell;

  memcpy(ptr, cookie->buf + cookie->tell, nbytes);
  cookie->tell += nbytes;

  return nbytes;
}
static int seek_memfile(void * _cookie, off64_t * offset, int whence) {
  memfile_cookie * cookie = _cookie;
  off64_t newtell;
  switch(whence) {
    case SEEK_SET:
      newtell = *offset;
      break;
    case SEEK_END:
      newtell = cookie->len + *offset;
      break;
    case SEEK_CUR:
      newtell = cookie->tell + *offset;
      break;
    default:
      return -1;
  }
  if(newtell < 0 || newtell > cookie->len)
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
DFILE * dfmemopen(void * buf, size_t size, char const * mode) {
  if(!strcmp(mode, "w+") || !strcmp(mode, "wb+"))
    ((char*)buf)[0] = 0;
  bool owns_buf = false;
  if(!buf) {
    buf = malloc(size);
    owns_buf = true;
  }

  memfile_cookie * cookie = malloc(sizeof(memfile_cookie));
  *cookie = (memfile_cookie) {
    .buf = buf,
    .tell = 0,
    .len = size,
    .owns_buf = owns_buf,
  };

  d_cookie_io_functions_t funcs = {
    .read = read_memfile,
    .write = write_memfile,
    .seek = seek_memfile,
    .close = close_memfile,
  };

  return d_fopencookie(cookie, mode, funcs);
}

DFILE * dfopen(char const * path, char const * mode) {
  int flags = 0;
  if(!strcmp(mode, "r") || !strcmp(mode, "rb"))
    flags = O_RDONLY;
  else if(!strcmp(mode, "w") || !strcmp(mode, "wb"))
    flags = O_WRONLY | O_CREAT | O_TRUNC;
  else if(!strcmp(mode, "r+") || !strcmp(mode, "rb+"))
    flags = O_RDWR;
  else if(!strcmp(mode, "w+") || !strcmp(mode, "wb+"))
    flags = O_RDWR | O_CREAT | O_TRUNC;
  else
    return NULL;

  int fd = open(path, flags);
  if(fd < 0) return NULL;
  return dfdopen(fd, mode);
}

DFILE * dtmpfile() {
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
  return dfdopen(fd, "r+");
}

DFILE * dstrfile() {
  DFILE * ret = malloc(sizeof(DFILE));
  *ret = (DFILE) {
    .canary = DFILE_CANARY,
    .fd = -1,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = DFILE_READ | DFILE_WRITE | DFILE_STRFILE,
    .buf_size = DEFAULT_BUF_SIZE,
    .buf = ret->buf_storage,
  };
  return ret;
}

static int write_strfile(DFILE * f, char const * ptr, int nbytes) {
  assert(f->canary == DFILE_CANARY);
  STRPAGE ** page_place = &f->strpages;
  STRPAGE * page = f->strpages;
  off_t tell = f->tell;
  int ret = nbytes;
  while(tell >= DEFAULT_BUF_SIZE) {
    page_place = &page->next;
    page = page->next;
    tell -= DEFAULT_BUF_SIZE;
  }
  while(nbytes) {
    if(!page) {
      assert(tell == 0);
      *page_place = page = malloc(sizeof(STRPAGE));
      memset(page, 0, sizeof(STRPAGE));
    }
    int towrite = DEFAULT_BUF_SIZE - tell;
    if(towrite > nbytes) towrite = nbytes;
    memcpy(page->buf + tell, ptr, towrite);
    tell += towrite;
    f->tell += towrite;
    ptr += towrite;
    nbytes -= towrite;
    if(tell >= DEFAULT_BUF_SIZE) {
      page_place = &page->next;
      page = page->next;
      tell -= DEFAULT_BUF_SIZE;
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
  while(tell >= DEFAULT_BUF_SIZE) {
    page = page->next;
    tell -= DEFAULT_BUF_SIZE;
  }
  while(nbytes) {
    assert(page);
    int toread = DEFAULT_BUF_SIZE - tell;
    if(toread > nbytes) toread = nbytes;
    memcpy(ptr, page->buf + tell, toread);
    tell += toread;
    f->tell += toread;
    ptr += toread;
    nbytes -= toread;
    if(tell >= DEFAULT_BUF_SIZE) {
      page = page->next;
      tell -= DEFAULT_BUF_SIZE;
    }
  }
  return ret;
}

static int dfflush_impl(DFILE * f, int flushbytes) {
  assert(f->canary == DFILE_CANARY);
  void * ptr = f->buf;
  int nbytes = flushbytes;
  if(f->buf_cursor) {
    dseek(f, -f->buf_cursor, SEEK_CUR);
    f->buf_cursor = 0;
  }
  if(f->flags & DFILE_STRFILE) {
    write_strfile(f, ptr, nbytes);
  } else {
    while(nbytes) {
      int ret;
      if(f->flags & DFILE_COOKIE) {
        if(!f->funcs.write)
          ret = nbytes;
        else
          ret = f->funcs.write(f->cookie, ptr, nbytes);
      } else {
        ret = write(f->fd, ptr, nbytes);
      }
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

int dfflush(DFILE * f) {
  if(f->dirty_cursor) {
    if(dfflush_impl(f, f->dirty_cursor) < 0)
      return -1;
  }
  if(f->num_ungets) {
    int ret = dseek(f, -f->num_ungets - f->buf_cursor, SEEK_CUR);
    f->num_ungets = 0;
    f->buf_cursor = 0;
    if(ret < 0)
      return -1;
  }
  return 0;
}

__attribute__((destructor))
static void flush_stdio() {
  dfflush(dstdin);
  dfflush(dstdout);
  dfflush(dstderr);
}

int dfseek(DFILE * f, int offset, int whence) {
  assert(f->canary == DFILE_CANARY);
  if(dfflush(f) < 0)
    return -1;
  if(whence == SEEK_CUR) {
    int ret = dseek(f, offset - f->buf_cursor, SEEK_CUR);
    f->buf_cursor = 0;
    return ret;
  }
  if(whence == SEEK_SET || whence == SEEK_END) {
    f->buf_cursor = 0;
    int ret = dseek(f, offset, whence);
    if(ret < 0)
      f->flags |= DFILE_ERROR;
    return ret;
  }
  f->flags |= DFILE_ERROR;
  return -1;
}

int dfclose(DFILE * f) {
  dfflush(f);
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
    DWORD ret;
    if(!GetExitCodeProcess(f->process, &ret))
      ret = -1;
    CloseHandle(f->process);
    CloseHandle(f->thread);
    return ret;
#endif
  }
  free(f);
  return ret;
}
int dpclose(DFILE * f) {
  return dfclose(f);
}

int dfwrite(const void * ptr, int ct, DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  if(!(f->flags & DFILE_WRITE)) {
    f->flags |= DFILE_ERROR;
    return -1;
  }
  if(f->num_ungets) {
    if(dfflush(f) < 0)
      return -1;
  }

  int ret = 0;
  while(ct) {
    if(f->dirty_cursor == f->buf_size) {
      if(dfflush(f) < 0)
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
    int ret = dfflush(f);
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
      int ret = dfflush_impl(f, (char*)ptr - f->buf + 1);
      if(ret < 0) {
        f->flags |= DFILE_ERROR;
        return ret;
      }
    }
  }
  return ret;
}

static int dfbuffer(DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return -1;
  }
  if(dfflush(f) < 0)
    return -1;
  if(f->buf_cursor == f->buf_size)
    return 0;
  int ret = -1;
  if(f->flags & DFILE_STRFILE) {
    ret = read_strfile(f, f->buf + f->buf_cursor, f->buf_size - f->buf_cursor);
  } else {
    while(ret < 0) {
      if(f->flags & DFILE_COOKIE) {
        if(!f->funcs.read)
          ret = 0;
        else
          ret = f->funcs.read(f->cookie, f->buf + f->buf_cursor, f->buf_size - f->buf_cursor);
      } else {
        ret = read(f->fd, f->buf + f->buf_cursor, f->buf_size - f->buf_cursor);
      }
      if(ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return ret;
    }
  }
  f->buf_cursor += ret;
  return ret;
}

int dfread(void * ptr, int ct, DFILE * f) {
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
  if(dfflush(f) < 0)
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
      int bufret = dfbuffer(f);
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

char * dfgets(char * buf, int ct, DFILE * f) {
  assert(f->canary == DFILE_CANARY);
  char * ret = buf;
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
  if(dfflush(f) < 0)
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
      f->buf_cursor -= nbytes;
    }
    if(!satisfied && ct > 1) {
      int bufret = dfbuffer(f);
      if(bufret <= 0) {
        if(bufret == 0)
          f->flags |= DFILE_EOF;
        else
          f->flags |= DFILE_ERROR;
        buf[0] = 0;
        return bufret < 0 ? NULL : ret;
      }
    }
  }
  if(ct)
    buf[0] = 0;
  return ret;
}

DFILE * dpopen(const char * cmd, const char *type) {
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
    DFILE * ret = dfdopen(fds[!is_read], type);
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
#define FORMAT "cmd.exe /C %s"
  char * cmdline = malloc(strlen(cmd)+sizeof FORMAT);
  sprintf(cmdline, FORMAT, cmd);
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
  DFILE * ret = dfdopen(fd, type);
  if(ret) {
    ret->flags |= DFILE_PROCESS;
    ret->process = pi.hProcess;
    ret->thread = pi.hThread;
  }
  return ret;
#endif
  return NULL;
}

int dungetc(int c, DFILE * f) {
  if(c == -1)
    return -1;
  if(f->num_ungets < DFILE_UNGETS) {
    f->ungets[f->num_ungets++] = c;
    f->flags &= ~DFILE_EOF;
    return c;
  }
  return -1;
}

//////////////////////////////////////////
//              NICETIES                //
//////////////////////////////////////////

int dfgetc(DFILE * f) {
  char c;
  int ret = dfread(&c, 1, f);
  return ret < 0 ? -1 : c;
}

int dgetc(DFILE * f) {
  return dgetc(f);
}

int dgetchar() {
  return dgetc(dstdin);
}

int dfputc(int c, DFILE * f) {
  unsigned char cc = c;
  return dfwrite(&cc, 1, f) < 0 ? -1 : cc;
}

int dputc(int c, DFILE * f) {
  return dfputc(c, f);
}

int dputchar(int c) {
  return dputc(c, dstdout);
}

int dfputs(char const * str, DFILE * f) {
 size_t len = strlen(str);
 return dfwrite(str, len, f);
}

int dputs(char const * str) {
  int ret = dfputs(str, dstdout);
  if(ret < 0) return -1;
  int ret2 = dputchar('\n');
  if(ret2 < 0)
    return ret;
  else
    return ret + 1;
}
