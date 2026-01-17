#include "bmp280.h"

#include "i2c_bus.h"

namespace {
uint16_t u16le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
int16_t s16le(const uint8_t* p) {
  return (int16_t)u16le(p);
}
} // namespace

static bool bmp280_read_cal(Bmp280* b) {
  uint8_t buf[24];
  if (!i2c_read_reg(b->addr, 0x88, buf, 24))
    return false;
  b->cal.dig_T1 = u16le(&buf[0]);
  b->cal.dig_T2 = s16le(&buf[2]);
  b->cal.dig_T3 = s16le(&buf[4]);
  b->cal.dig_P1 = u16le(&buf[6]);
  b->cal.dig_P2 = s16le(&buf[8]);
  b->cal.dig_P3 = s16le(&buf[10]);
  b->cal.dig_P4 = s16le(&buf[12]);
  b->cal.dig_P5 = s16le(&buf[14]);
  b->cal.dig_P6 = s16le(&buf[16]);
  b->cal.dig_P7 = s16le(&buf[18]);
  b->cal.dig_P8 = s16le(&buf[20]);
  b->cal.dig_P9 = s16le(&buf[22]);
  return true;
}

bool bmp280_init(Bmp280* b, uint8_t addr) {
  if (!b)
    return false;
  b->addr = addr;
  uint8_t id = 0;
  if (!i2c_read_reg(addr, 0xD0, &id, 1))
    return false;
  if (id != 0x58)
    return false;
  if (!bmp280_read_cal(b))
    return false;
  uint8_t cfg = 0x00;
  uint8_t ctrl = 0x27; // temp/press x1, normal mode
  if (!i2c_write_reg(addr, 0xF5, &cfg, 1))
    return false;
  if (!i2c_write_reg(addr, 0xF4, &ctrl, 1))
    return false;
  delay(10);
  return true;
}

static int32_t bmp280_compensate_temp(Bmp280* b, int32_t adc_T) {
  int32_t var1, var2;
  var1 = ((((adc_T >> 3) - ((int32_t)b->cal.dig_T1 << 1))) * ((int32_t)b->cal.dig_T2)) >> 11;
  var2 =
    (((((adc_T >> 4) - ((int32_t)b->cal.dig_T1)) * ((adc_T >> 4) - ((int32_t)b->cal.dig_T1))) >>
      12) *
     ((int32_t)b->cal.dig_T3)) >>
    14;
  b->t_fine = var1 + var2;
  return (b->t_fine * 5 + 128) >> 8;
}

static uint32_t bmp280_compensate_pressure(Bmp280* b, int32_t adc_P) {
  int64_t var1, var2, p;
  var1 = ((int64_t)b->t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)b->cal.dig_P6;
  var2 = var2 + ((var1 * (int64_t)b->cal.dig_P5) << 17);
  var2 = var2 + (((int64_t)b->cal.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)b->cal.dig_P3) >> 8) + ((var1 * (int64_t)b->cal.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1) * (int64_t)b->cal.dig_P1) >> 33;
  if (var1 == 0)
    return 0;
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)b->cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)b->cal.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)b->cal.dig_P7) << 4);
  return (uint32_t)(p >> 8);
}

bool bmp280_read(Bmp280* b, int16_t* t_centi, uint32_t* p_pa) {
  if (!b || !t_centi || !p_pa)
    return false;
  uint8_t buf[6];
  if (!i2c_read_reg(b->addr, 0xF7, buf, 6))
    return false;
  int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
  int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
  int32_t t = bmp280_compensate_temp(b, adc_T);
  uint32_t p = bmp280_compensate_pressure(b, adc_P);
  *t_centi = (int16_t)t;
  *p_pa = p;
  return true;
}
