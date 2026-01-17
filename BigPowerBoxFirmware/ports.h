#pragma once

#include <Arduino.h>

#include "board_config.h"
#include "ema.h"
#include "mcp23017.h"
#include "smoothing.h"

struct Ports {
  bool state[PORT_COUNT];
  uint8_t pwm_mode[PWM_PORT_COUNT];
  uint8_t pwm_level[PWM_PORT_COUNT];
  int32_t input_mv;
  int32_t input_ma;
  int32_t port_ma[PORT_COUNT];
  RollingAverage<ADC_SMOOTHING_WINDOW> input_mv_avg;
  RollingAverage<ADC_SMOOTHING_WINDOW> input_ma_avg;
  RollingAverage<ADC_SMOOTHING_WINDOW> port_ma_avg[PORT_COUNT];
  Mcp23017 mcp;
  bool have_temp;
  bool have_press;
  int32_t temp_centi;
  int32_t humid_centi;
  int32_t pressure_hpa;
  EmaFilter temp_ema;
  EmaFilter humid_ema;
  EmaFilter press_ema;
  bool dew_active;
  uint8_t dew_duty;
  int32_t dewpoint_centi;
#ifdef DEBUG
  bool debug_override;
  int32_t debug_temp_centi;
  int32_t debug_humid_centi;
  bool debug_fake_probe;
#endif
};

void ports_init(Ports* ports);
bool ports_set(Ports* ports, uint8_t port_index, bool on);
bool ports_set_pwm_level(Ports* ports, uint8_t port_index, uint8_t level);
bool ports_set_pwm_mode(Ports* ports, uint8_t port_index, uint8_t mode);
bool ports_get(const Ports* ports, uint8_t port_index);
bool ports_is_controllable(uint8_t port_index);
char ports_port_type(uint8_t port_index);
uint8_t ports_get_pwm_mode(const Ports* ports, uint8_t port_index);
uint8_t ports_get_status_value(const Ports* ports, uint8_t port_index);
void ports_update_input_readings(Ports* ports);
void ports_update_port_current(Ports* ports, uint8_t port_index);
int32_t ports_get_input_mv(const Ports* ports);
int32_t ports_get_input_ma(const Ports* ports);
int32_t ports_get_port_ma(const Ports* ports, uint8_t port_index);
void ports_apply_dew_duty(Ports* ports, uint8_t duty);
void ports_disable_dew_mode(Ports* ports);
void ports_apply_config(Ports* ports);
void ports_all_off(Ports* ports);
bool ports_overvoltage(const Ports* ports);