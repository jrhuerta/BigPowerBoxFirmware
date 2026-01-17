#pragma once

#include <Arduino.h>

#include "board_config.h"

struct Config {
  uint8_t currentData;
  uint16_t portStatus;
  uint8_t pwmPorts[PWM_PORT_COUNT];
  uint8_t pwmPortMode[PWM_PORT_COUNT];
  int16_t dew_m_on_centi;
  uint8_t dew_duty_min_pct;
  uint8_t dew_duty_max_auto_pct;
};

extern Config g_config;

void eeprom_cfg_init(Config* cfg);
void eeprom_cfg_save(const Config* cfg);
void eeprom_cfg_defaults(Config* cfg);
void eeprom_name_init_defaults();
void eeprom_name_read(uint8_t port, char* out);
void eeprom_name_write(uint8_t port, const char* name);
