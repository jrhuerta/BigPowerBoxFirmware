#pragma once

#include <Arduino.h>

struct Sht31 {
  uint8_t addr;
};

bool sht31_init(Sht31* s, uint8_t addr);
bool sht31_read(Sht31* s, int16_t* t_centi, uint16_t* rh_centi);
