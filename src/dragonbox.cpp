#include "dragonbox.h"
#include "dragonbox.inl"

extern "C" {
  dragonbox dtodragon(double d) {
    auto drag = jkj::dragonbox::to_decimal(d);
    dragonbox ret = {};
    ret.significand = drag.significand;
    ret.exponent = drag.exponent;
    ret.is_negative = drag.is_negative;
    return ret;
  }
  dragonbox ftodragon(float f) {
    auto drag = jkj::dragonbox::to_decimal(f);
    dragonbox ret = {};
    ret.significand = drag.significand;
    ret.exponent = drag.exponent;
    ret.is_negative = drag.is_negative;
    return ret;
  }
}
