#ifndef DRAGONBOX_H
#define DRAGONBOX_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dragonbox {
  uint64_t significand;
  int exponent;
  bool is_negative;
} dragonbox;

dragonbox dtodragon(double d);
dragonbox ftodragon(float f);

#ifdef __cplusplus
}
#endif
#endif
