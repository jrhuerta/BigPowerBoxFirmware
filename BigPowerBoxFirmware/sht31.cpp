#include "sht31.h"

#include "i2c_bus.h"

bool sht31_init(Sht31* s, uint8_t addr) {
  if (!s)
    return false;
  s->addr = addr;
  return i2c_probe(addr);
}

bool sht31_read(Sht31* s, int16_t* t_centi, uint16_t* rh_centi) {
  if (!s || !t_centi || !rh_centi)
    return false;
  uint8_t cmd[2] = {0x24, 0x00}; // single-shot, high repeatability, no stretch
  if (!i2c_write(s->addr, cmd, 2))
    return false;
  delay(15);

  uint8_t buf[6];
  if (!i2c_read(s->addr, buf, 6))
    return false;

  uint16_t raw_t = ((uint16_t)buf[0] << 8) | buf[1];
  uint16_t raw_h = ((uint16_t)buf[3] << 8) | buf[4];

  int32_t t = -4500 + (int32_t)((17500L * raw_t + 32767) / 65535);
  int32_t h = (int32_t)((10000L * raw_h + 32767) / 65535);

  *t_centi = (int16_t)t;
  *rh_centi = (uint16_t)h;
  return true;
}
