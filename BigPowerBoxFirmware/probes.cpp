#include "probes.h"

#include <string.h>

#include "board_config.h"

namespace {
bool try_sht31(Probes* p, uint8_t addr) {
  if (sht31_init(&p->sht, addr)) {
    p->ambient = AMBIENT_SHT31;
    return true;
  }
  return false;
}

bool try_ahtx0(Probes* p, uint8_t addr) {
  if (ahtx0_init(&p->aht, addr)) {
    p->ambient = AMBIENT_AHTX0;
    return true;
  }
  return false;
}

bool try_bme280(Probes* p, uint8_t addr) {
  if (bme280_init(&p->bme, addr)) {
    p->ambient = AMBIENT_BME280;
    p->pressure = PRESS_BME280;
    return true;
  }
  return false;
}

bool try_bmp280(Probes* p, uint8_t addr) {
  if (bmp280_init(&p->bmp, addr)) {
    p->pressure = PRESS_BMP280;
    return true;
  }
  return false;
}

void update_signature(bool have_temp, bool have_press) {
  strncpy(g_board_signature, BOARD_SIGNATURE_BASE, BOARD_SIGNATURE_MAX_LEN);
  g_board_signature[BOARD_SIGNATURE_MAX_LEN - 1] = '\0';
  if (!have_temp)
    return;
  size_t len = strlen(g_board_signature);
  if (len + 1 >= BOARD_SIGNATURE_MAX_LEN)
    return;
  g_board_signature[len] = have_press ? 'g' : 'f';
  g_board_signature[len + 1] = '\0';
}
} // namespace

void probes_detect(Probes* p, Ports* ports) {
  if (!p || !ports)
    return;
  p->ambient = AMBIENT_NONE;
  p->pressure = PRESS_NONE;
  ports->have_temp = false;
  ports->have_press = false;

  if (try_sht31(p, 0x44) || try_sht31(p, 0x45)) {
    ports->have_temp = true;
  } else if (try_ahtx0(p, 0x38)) {
    ports->have_temp = true;
  } else if (try_bme280(p, 0x76) || try_bme280(p, 0x77)) {
    ports->have_temp = true;
    ports->have_press = true;
  }

  if (ports->have_temp && !ports->have_press) {
    if (try_bmp280(p, 0x76) || try_bmp280(p, 0x77)) {
      ports->have_press = true;
    }
  }

  if (p->ambient == AMBIENT_BME280) {
    ports->have_press = true;
  }

  update_signature(ports->have_temp, ports->have_press);

#ifdef DEBUG
#if DEBUG_FAKE_PROBE
  if (!ports->have_temp) {
    ports->have_temp = true;
    ports->debug_fake_probe = true;
    ports->temp_centi = DEBUG_FAKE_TEMP_CENTI;
    ports->humid_centi = DEBUG_FAKE_HUMID_CENTI;
  }
#endif
  if (ports->have_temp && !ports->debug_fake_probe) {
    ports->debug_fake_probe = false;
  }
  update_signature(ports->have_temp, ports->have_press);
#endif
}

bool probes_update(Probes* p, Ports* ports) {
  if (!p || !ports)
    return false;
  if (!ports->have_temp)
    return false;

#ifdef DEBUG
  if (ports->debug_override) {
    ports->temp_centi = ports->temp_ema.update(ports->debug_temp_centi, SENSOR_EMA_ALPHA);
    ports->humid_centi = ports->humid_ema.update(ports->debug_humid_centi, SENSOR_EMA_ALPHA);
    return true;
  }
  if (ports->debug_fake_probe) {
    ports->temp_centi = ports->temp_ema.update(DEBUG_FAKE_TEMP_CENTI, SENSOR_EMA_ALPHA);
    ports->humid_centi = ports->humid_ema.update(DEBUG_FAKE_HUMID_CENTI, SENSOR_EMA_ALPHA);
    return true;
  }
#endif

  int16_t t_centi = 0;
  uint16_t rh_centi = 0;
  uint32_t p_pa = 0;

  switch (p->ambient) {
  case AMBIENT_SHT31:
    if (!sht31_read(&p->sht, &t_centi, &rh_centi))
      return false;
    break;
  case AMBIENT_AHTX0:
    if (!ahtx0_read(&p->aht, &t_centi, &rh_centi))
      return false;
    break;
  case AMBIENT_BME280:
    if (!bme280_read(&p->bme, &t_centi, &rh_centi, &p_pa))
      return false;
    break;
  default:
    return false;
  }

  ports->temp_centi = ports->temp_ema.update(t_centi, SENSOR_EMA_ALPHA);
  ports->humid_centi = ports->humid_ema.update(rh_centi, SENSOR_EMA_ALPHA);

  if (ports->have_press) {
    uint32_t press_pa = 0;
    if (p->pressure == PRESS_BME280) {
      press_pa = p_pa;
    } else if (p->pressure == PRESS_BMP280) {
      int16_t tmp = 0;
      if (!bmp280_read(&p->bmp, &tmp, &press_pa)) {
        // Pressure is optional; keep last value if the read fails.
        return true;
      }
    }
    int32_t press_hpa = (int32_t)(press_pa / 100);
    ports->pressure_hpa = ports->press_ema.update(press_hpa, SENSOR_EMA_ALPHA);
  }
  return true;
}
