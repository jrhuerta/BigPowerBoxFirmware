#pragma once

#include <Arduino.h>

#include "ahtx0.h"
#include "bme280.h"
#include "bmp280.h"
#include "ports.h"
#include "sht31.h"

enum AmbientType : uint8_t { AMBIENT_NONE = 0, AMBIENT_SHT31, AMBIENT_AHTX0, AMBIENT_BME280 };

enum PressureType : uint8_t { PRESS_NONE = 0, PRESS_BME280, PRESS_BMP280 };

struct Probes {
  AmbientType ambient;
  PressureType pressure;
  Sht31 sht;
  Ahtx0 aht;
  Bme280 bme;
  Bmp280 bmp;
};

void probes_detect(Probes* p, Ports* ports);
bool probes_update(Probes* p, Ports* ports);
