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
// ungetc

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
  DFILE_MEMFILE = 256,
  DFILE_PROCESS = 512,
};
enum { DEFAULT_BUF_SIZE = 4096 };

enum { DFILE_COOKIE = 0xDF11E83 };

typedef struct STRPAGE {
  struct STRPAGE * next;
  char buf[DEFAULT_BUF_SIZE];
} STRPAGE;

typedef struct DFILE {
  // invariant:
  // the underlying cursor of the fd is at the buf_cursor
  // and the dirty region is from 0 to dirty_cursor
  // the cursor of the DFILE is at dirty_cursor
  int cookie;
  int fd;
  int buf_cursor;
  int dirty_cursor;
  int flags;
  size_t buf_size;
  char * buf;
  char buf_storage[DEFAULT_BUF_SIZE];
  // strfile stuff
  off_t tell;
  off_t len;
  union {
    STRPAGE * strpages;
    char * membuf;
  };
#ifdef __linux__
  pid_t process;
#endif
#ifdef _WIN64
  HANDLE process;
  HANDLE thread;
#endif
} DFILE;

DFILE dstdin_impl = {
  .cookie = DFILE_COOKIE,
  .fd = STDIN_FILENO,
  .flags = DFILE_READ,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstdin_impl.buf_storage,
};
DFILE dstdout_impl = {
  .cookie = DFILE_COOKIE,
  .fd = STDOUT_FILENO,
  .flags = DFILE_WRITE | DFILE_LINE_BUFFERED,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstdout_impl.buf_storage,
};
DFILE dstderr_impl = {
  .cookie = DFILE_COOKIE,
  .fd = STDERR_FILENO,
  .flags = DFILE_WRITE | DFILE_UNBUFFERED,
  .buf_size = DEFAULT_BUF_SIZE,
  .buf = dstderr_impl.buf_storage,
};
DFILE * dstdin = &dstdin_impl;
DFILE * dstdout = &dstdout_impl;
DFILE * dstderr = &dstderr_impl;

static off_t dseek(DFILE * f, off_t offset, int whence) {
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
  } else {
    return lseek(f->fd, offset, whence);
  }
}

off_t dftell(DFILE * f) {
  off_t o = dseek(f, 0, SEEK_CUR);
  if(o < 0) return o;
  return o - f->buf_cursor + f->dirty_cursor;
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

DFILE * dfdopen(int fd, char const * flags) {
  DFILE * ret = malloc(sizeof(DFILE));
  int bitfield = 0;
  if(!strcmp(flags, "w") || !strcmp(flags, "wb")) // doesn't truncate
    bitfield |= DFILE_WRITE;
  else if(!strcmp(flags, "r") || !strcmp(flags, "rb"))
    bitfield |= DFILE_READ;
  else if(!strcmp(flags, "r+") || !strcmp(flags, "rb+"))
    bitfield |= DFILE_READ | DFILE_WRITE;
  else if(!strcmp(flags, "w+") || !strcmp(flags, "wb+")) // w+ and r+ are  the same in fdopen since no truncation
    bitfield |= DFILE_READ | DFILE_WRITE;
  else
    return NULL;

  *ret = (DFILE) {
    .cookie = DFILE_COOKIE,
    .fd = fd,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield,
    .buf_size = DEFAULT_BUF_SIZE,
    .buf = ret->buf_storage,
  };
  return ret;
}

DFILE * dfmemopen(void * buf, int size, char const * flags) {
  DFILE * ret = malloc(sizeof(DFILE));
  int bitfield = 0;
  if(!strcmp(flags, "w") || !strcmp(flags, "wb"))
    bitfield |= DFILE_WRITE;
  else if(!strcmp(flags, "r") || !strcmp(flags, "rb"))
    bitfield |= DFILE_READ;
  else if(!strcmp(flags, "r+") || !strcmp(flags, "rb+"))
    bitfield |= DFILE_READ | DFILE_WRITE;
  else if(!strcmp(flags, "w+") || !strcmp(flags, "wb+"))
    bitfield |= DFILE_READ | DFILE_WRITE;
  else
    return NULL;

  *ret = (DFILE) {
    .cookie = DFILE_COOKIE,
    .fd = -1,
    .buf_cursor = 0,
    .dirty_cursor = 0,
    .flags = bitfield | DFILE_MEMFILE,
    .buf_size = DEFAULT_BUF_SIZE,
    .buf = ret->buf_storage,
    .len = size,
    .membuf = buf,
  };
  return ret;
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
    .cookie = DFILE_COOKIE,
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
  assert(f->cookie == DFILE_COOKIE);
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
  assert(f->cookie == DFILE_COOKIE);
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

#if 0
static int write_memfile(DFILE * f, char const * ptr, int nbytes) {
  assert(f->cookie == DFILE_COOKIE);
  off_t tell = f->tell;
  if(f->len - tell < nbytes)
    nbytes = f->len - tell;

  memcpy(f->membuf + tell, ptr, nbytes);
  f->tell += nbytes;

  if(f->tell < f->len)
    f->membuf[f->len - 1] = '\0';

  return nbytes;
}
static int read_memfile(DFILE * f, char const * ptr, int nbytes) {
  assert(f->cookie == DFILE_COOKIE);
  off_t tell = f->tell;
  if(f->len - tell < nbytes)
    nbytes = f->len - tell;

  memcpy(ptr, f->membuf + tell, nbytes);
  f->tell += nbytes;

  return nbytes;
}
#endif

static int dfflush_impl(DFILE * f, int flushbytes) {
  assert(f->cookie == DFILE_COOKIE);
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

int dfflush(DFILE * f) {
  if(f->dirty_cursor)
    return dfflush_impl(f, f->dirty_cursor);
  return 0;
}

__attribute__((destructor))
static void flush_stdio() {
  dfflush(dstdin);
  dfflush(dstdout);
  dfflush(dstderr);
}

int dfseek(DFILE * f, int offset, int whence) {
  assert(f->cookie == DFILE_COOKIE);
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
  assert(f->cookie == DFILE_COOKIE);
  if(!(f->flags & DFILE_WRITE)) {
    f->flags |= DFILE_ERROR;
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
  assert(f->cookie == DFILE_COOKIE);
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
      ret = read(f->fd, f->buf + f->buf_cursor, f->buf_size - f->buf_cursor);
      if(ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return ret;
    }
  }
  f->buf_cursor += ret;
  return ret;
}

int dfread(void * ptr, int ct, DFILE * f) {
  assert(f->cookie == DFILE_COOKIE);
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return 0;
  }
  if(dfflush(f) < 0)
    return 0;
  // dirty_cursor is now 0
  int nread = 0;
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
  assert(f->cookie == DFILE_COOKIE);
  char * ret = buf;
  if(!(f->flags & DFILE_READ)) {
    f->flags |= DFILE_ERROR;
    return NULL;
  }
  if(dfflush(f) < 0)
    return NULL;
  // dirty_cursor is now 0
  int nread = 0;
  bool satisfied = false;
  while(!satisfied && ct > 1) {
    if(f->buf_cursor) {
      int nbytes = 0;
      while(nbytes < f->buf_cursor) {
        if(f->buf[nbytes++] == '\n')
        {
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
