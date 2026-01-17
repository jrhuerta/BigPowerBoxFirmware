#include "bme280.h"

#include "i2c_bus.h"

namespace {
uint16_t u16le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
int16_t s16le(const uint8_t* p) {
  return (int16_t)u16le(p);
}
} // namespace

static bool bme280_read_cal(Bme280* b) {
  uint8_t buf1[26];
  if (!i2c_read_reg(b->addr, 0x88, buf1, 26))
    return false;
  b->cal.dig_T1 = u16le(&buf1[0]);
  b->cal.dig_T2 = s16le(&buf1[2]);
  b->cal.dig_T3 = s16le(&buf1[4]);
  b->cal.dig_P1 = u16le(&buf1[6]);
  b->cal.dig_P2 = s16le(&buf1[8]);
  b->cal.dig_P3 = s16le(&buf1[10]);
  b->cal.dig_P4 = s16le(&buf1[12]);
  b->cal.dig_P5 = s16le(&buf1[14]);
  b->cal.dig_P6 = s16le(&buf1[16]);
  b->cal.dig_P7 = s16le(&buf1[18]);
  b->cal.dig_P8 = s16le(&buf1[20]);
  b->cal.dig_P9 = s16le(&buf1[22]);
  b->cal.dig_H1 = buf1[25];

  uint8_t buf2[7];
  if (!i2c_read_reg(b->addr, 0xE1, buf2, 7))
    return false;
  b->cal.dig_H2 = s16le(&buf2[0]);
  b->cal.dig_H3 = buf2[2];
  b->cal.dig_H4 = (int16_t)((buf2[3] << 4) | (buf2[4] & 0x0F));
  b->cal.dig_H5 = (int16_t)((buf2[5] << 4) | (buf2[4] >> 4));
  b->cal.dig_H6 = (int8_t)buf2[6];
  return true;
}

bool bme280_init(Bme280* b, uint8_t addr) {
  if (!b)
    return false;
  b->addr = addr;
  uint8_t id = 0;
  if (!i2c_read_reg(addr, 0xD0, &id, 1))
    return false;
  if (id != 0x60)
    return false;
  if (!bme280_read_cal(b))
    return false;
  uint8_t cfg = 0x00;
  uint8_t ctrl_hum = 0x01;  // humidity x1
  uint8_t ctrl_meas = 0x27; // temp/press x1, normal mode
  if (!i2c_write_reg(addr, 0xF5, &cfg, 1))
    return false;
  if (!i2c_write_reg(addr, 0xF2, &ctrl_hum, 1))
    return false;
  if (!i2c_write_reg(addr, 0xF4, &ctrl_meas, 1))
    return false;
  delay(10);
  return true;
}

static int32_t bme280_compensate_temp(Bme280* b, int32_t adc_T) {
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

static uint32_t bme280_compensate_pressure(Bme280* b, int32_t adc_P) {
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

static uint16_t bme280_compensate_humidity(Bme280* b, int32_t adc_H) {
  int32_t v_x1_u32r;
  v_x1_u32r = b->t_fine - 76800;
  v_x1_u32r =
    (((((adc_H << 14) - ((int32_t)b->cal.dig_H4 << 20) - ((int32_t)b->cal.dig_H5 * v_x1_u32r)) +
       16384) >>
      15) *
     (((((((v_x1_u32r * (int32_t)b->cal.dig_H6) >> 10) *
          (((v_x1_u32r * (int32_t)b->cal.dig_H3) >> 11) + 32768)) >>
         10) +
        2097152) *
         (int32_t)b->cal.dig_H2 +
       8192) >>
      14));
  v_x1_u32r =
    (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)b->cal.dig_H1) >> 4));
  if (v_x1_u32r < 0)
    v_x1_u32r = 0;
  if (v_x1_u32r > 419430400)
    v_x1_u32r = 419430400;
  uint32_t h = (uint32_t)(v_x1_u32r >> 12); // 1/1024 %RH
  return (uint16_t)((h * 100 + 512) / 1024);
}

bool bme280_read(Bme280* b, int16_t* t_centi, uint16_t* rh_centi, uint32_t* p_pa) {
  if (!b || !t_centi || !rh_centi || !p_pa)
    return false;
  uint8_t ctrl_hum = 0x01;
  uint8_t ctrl_meas = 0x27;
  if (!i2c_write_reg(b->addr, 0xF2, &ctrl_hum, 1))
    return false;
  if (!i2c_write_reg(b->addr, 0xF4, &ctrl_meas, 1))
    return false;
  delay(10);

  uint8_t buf[8];
  if (!i2c_read_reg(b->addr, 0xF7, buf, 8))
    return false;
  int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
  int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
  int32_t adc_H = ((int32_t)buf[6] << 8) | buf[7];

  int32_t t = bme280_compensate_temp(b, adc_T);
  uint32_t p = bme280_compensate_pressure(b, adc_P);
  uint16_t h = bme280_compensate_humidity(b, adc_H);

  *t_centi = (int16_t)t;
  *p_pa = p;
  *rh_centi = h;
  return true;
}
