#include "dfile.h"
#include <stdio.h>
#if defined(__linux__) || defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

_Static_assert(D_SEEK_SET == SEEK_SET, "");
_Static_assert(D_SEEK_CUR == SEEK_CUR, "");
_Static_assert(D_SEEK_END == SEEK_END, "");

_Static_assert(D_IONBF == _IONBF, "");
_Static_assert(D_IOLBF == _IOLBF, "");
_Static_assert(D_IOFBF == _IOFBF, "");

_Static_assert(D_STDIN_FILENO == STDIN_FILENO, "");
_Static_assert(D_STDOUT_FILENO == STDOUT_FILENO, "");
_Static_assert(D_STDERR_FILENO == STDERR_FILENO, "");
