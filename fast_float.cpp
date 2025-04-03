#include "fast_float/fast_float.h"
#include "fast_float.h"

extern "C" {
  __attribute__((visibility("default")))
  char const * ff_fromchars(char const * begin, char const * end, double * ret) {
    static auto fmt =
      fast_float::chars_format::general |
      fast_float::chars_format::allow_leading_plus |
      fast_float::chars_format::skip_white_space;
    auto ok = fast_float::from_chars(begin, end, *ret, fmt);
    return ok.ec == std::errc() ? ok.ptr : NULL;
  }
}
