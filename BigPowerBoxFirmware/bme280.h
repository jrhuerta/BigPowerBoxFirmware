#pragma once

#include <Arduino.h>

struct Bme280Cal {
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
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
};

struct Bme280 {
  uint8_t addr;
  Bme280Cal cal;
  int32_t t_fine;
};

bool bme280_init(Bme280* b, uint8_t addr);
bool bme280_read(Bme280* b, int16_t* t_centi, uint16_t* rh_centi, uint32_t* p_pa);
