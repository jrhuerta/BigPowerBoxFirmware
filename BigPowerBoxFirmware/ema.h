#pragma once

#include <stdint.h>

struct EmaFilter {
  int32_t value = 0;
  bool initialized = false;

  void reset() {
    value = 0;
    initialized = false;
  }

  int32_t update(int32_t sample, uint16_t alpha_q8) {
    if (!initialized) {
      value = sample;
      initialized = true;
      return value;
    }
    int32_t diff = sample - value;
    value += (diff * alpha_q8) >> 8;
    return value;
  }
};
