#pragma once

#include <stdint.h>

template <uint8_t N> struct RollingAverage {
  int32_t sum = 0;
  int32_t buf[N] = {};
  uint8_t index = 0;
  uint8_t count = 0;

  void reset() {
    sum = 0;
    index = 0;
    count = 0;
    for (uint8_t i = 0; i < N; i++) {
      buf[i] = 0;
    }
  }

  int32_t add(int32_t value) {
    if (count < N) {
      buf[index] = value;
      sum += value;
      count++;
    } else {
      sum -= buf[index];
      buf[index] = value;
      sum += value;
    }
    index++;
    if (index >= N)
      index = 0;
    return average();
  }

  int32_t average() const {
    if (count == 0)
      return 0;
    return sum / count;
  }
};
