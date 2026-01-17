#pragma once

#include <Arduino.h>

struct Ahtx0 {
  uint8_t addr;
};

bool ahtx0_init(Ahtx0* a, uint8_t addr);
bool ahtx0_read(Ahtx0* a, int16_t* t_centi, uint16_t* rh_centi);
