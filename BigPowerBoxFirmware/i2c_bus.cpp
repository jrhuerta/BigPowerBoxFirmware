#include "i2c_bus.h"

#include <Wire.h>

bool i2c_write(uint8_t addr, const uint8_t* data, uint8_t len) {
  Wire.beginTransmission(addr);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission() == 0;
}

bool i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t* data, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission() == 0;
}

bool i2c_read(uint8_t addr, uint8_t* data, uint8_t len) {
  uint8_t got = Wire.requestFrom((int)addr, (int)len);
  if (got != len)
    return false;
  for (uint8_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return true;
}

bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;
  return i2c_read(addr, data, len);
}

bool i2c_probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
