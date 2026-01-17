#pragma once

#include <Arduino.h>

struct Bmp280Cal {
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
};

struct Bmp280 {
  uint8_t addr;
  Bmp280Cal cal;
  int32_t t_fine;
};

bool bmp280_init(Bmp280* b, uint8_t addr);
bool bmp280_read(Bmp280* b, int16_t* t_centi, uint32_t* p_pa);
