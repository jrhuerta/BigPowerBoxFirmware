#include "ahtx0.h"

#include "i2c_bus.h"

bool ahtx0_init(Ahtx0* a, uint8_t addr) {
  if (!a)
    return false;
  a->addr = addr;
  delay(50);
  uint8_t cmd[3] = {0xBE, 0x08, 0x00};
  if (!i2c_write(a->addr, cmd, 3))
    return false;
  delay(10);
  return true;
}

bool ahtx0_read(Ahtx0* a, int16_t* t_centi, uint16_t* rh_centi) {
  if (!a || !t_centi || !rh_centi)
    return false;
  uint8_t trig[3] = {0xAC, 0x33, 0x00};
  if (!i2c_write(a->addr, trig, 3))
    return false;

  const uint32_t t0 = millis();
  uint8_t buf[6];
  while (true) {
    delay(10);
    if (!i2c_read(a->addr, buf, 6))
      return false;
    if ((buf[0] & 0x80) == 0)
      break;
    if (millis() - t0 > 200)
      return false;
  }

  uint32_t raw_h =
    ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)(buf[3] >> 4) & 0x0F);

  uint32_t raw_t = ((uint32_t)(buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

  uint32_t rh = (uint32_t)(((uint64_t)raw_h * 10000ULL) >> 20);
  if (rh > 10000)
    rh = 10000;
  *rh_centi = (uint16_t)rh;

  int32_t tc = (int32_t)(((uint64_t)raw_t * 20000ULL) >> 20) - 5000;
  *t_centi = (int16_t)tc;

  return true;
}
