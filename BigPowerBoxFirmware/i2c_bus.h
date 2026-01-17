#pragma once

#include <Arduino.h>

bool i2c_write(uint8_t addr, const uint8_t* data, uint8_t len);
bool i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t* data, uint8_t len);
bool i2c_read(uint8_t addr, uint8_t* data, uint8_t len);
bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len);
bool i2c_probe(uint8_t addr);