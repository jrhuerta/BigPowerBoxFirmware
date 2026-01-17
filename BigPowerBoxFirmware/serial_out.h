#pragma once

#include <Arduino.h>

inline void out(char value) {
  Serial.print(value);
}
inline void out(const char* value) {
  Serial.print(value);
}
inline void out(const __FlashStringHelper* value) {
  Serial.print(value);
}
inline void out(int32_t value) {
  Serial.print(value);
}
inline void out(uint32_t value) {
  Serial.print(value);
}
inline void out(uint8_t value) {
  Serial.print(value);
}
