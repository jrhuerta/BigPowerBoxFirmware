#include <Arduino.h>
#include <Wire.h>

#include "board_config.h"
#include "eeprom_cfg.h"
#include "ports.h"
#include "probes.h"
#include "protocol.h"
#include "serial_framing.h"
#include "serial_out.h"
#include <math.h>
#include <string.h>

enum FsmState { STATE_IDLE, STATE_READ, STATE_SWAP };

static CommandQueue g_queue;
static Ports g_ports;
static FsmState g_state = STATE_IDLE;
static unsigned long g_last_refresh = 0;
static uint8_t g_port_index = 0;
static uint8_t g_chip = 0;
static bool g_dsel = true;
static unsigned long g_last_sensor_ms = 0;
static Probes g_probes;
Config g_config;
static unsigned long g_last_dew_ms = 0;
static unsigned long g_last_dewpoint_ms = 0;
#ifdef DEBUG
static unsigned long g_last_dew_log_ms = 0;
#endif

char g_board_signature[BOARD_SIGNATURE_MAX_LEN];

static void adc_swap_ports(uint8_t port_max) {
  // Increment port index and wrap.
  g_port_index++;
  if (g_port_index >= port_max)
    g_port_index = 0;

  // Toggle DSEL each swap.
  g_dsel = !g_dsel;
  digitalWrite(DSEL, g_dsel ? HIGH : LOW);

  // Advance mux chip on even port indices.
  if ((g_port_index % 2) == 0)
    g_chip++;
  // Board-specific mapping: skip extra slot before always-on ports.
  if (g_port_index == 13)
    g_chip++;
  if (g_chip > port_max / 2)
    g_chip = 0;

  digitalWrite(MUX0, bitRead(g_chip, 0));
  digitalWrite(MUX1, bitRead(g_chip, 1));
  digitalWrite(MUX2, bitRead(g_chip, 2));
}

static int32_t dew_margin_centi(int32_t t_centi, int32_t rh_centi) {
  float t = t_centi / 100.0f;
  float rh = rh_centi / 100.0f;
  if (rh < 1.0f)
    rh = 1.0f;
  if (rh > 100.0f)
    rh = 100.0f;
  const float a = 17.62f;
  const float b = 243.12f;
  float gamma = (a * t) / (b + t) + logf(rh / 100.0f);
  float t_dew = (b * gamma) / (a - gamma);
  float margin = t - t_dew;
  return (int32_t)(margin * 100.0f);
}

static uint8_t smoothstep_duty(uint8_t duty_min, uint8_t duty_max, int32_t margin_centi,
                               int32_t dew_m_on_centi) {
  if (margin_centi >= dew_m_on_centi)
    return 0;
  if (margin_centi <= DEW_M_FULL_CENTI)
    return duty_max;
  int32_t num = dew_m_on_centi - margin_centi;
  int32_t den = dew_m_on_centi - DEW_M_FULL_CENTI;
  if (den <= 0)
    return duty_min;
  int32_t x_q12 = (num << 12) / den;
  if (x_q12 < 0)
    x_q12 = 0;
  if (x_q12 > 4096)
    x_q12 = 4096;
  int32_t x2_q12 = (x_q12 * x_q12) >> 12;
  int32_t term_q12 = (3 << 12) - (2 * x_q12);
  int32_t smooth_q12 = (x2_q12 * term_q12) >> 12;
  int32_t duty = duty_min + ((smooth_q12 * (duty_max - duty_min)) >> 12);
  if (duty < 0)
    duty = 0;
  if (duty > 255)
    duty = 255;
  return (uint8_t)duty;
}

static uint8_t slew_limit(uint8_t current, uint8_t target) {
  uint8_t step = (uint8_t)((DEW_DUTY_SLEW_STEP_PCT * 255 + 50) / 100);
  if (target > current) {
    uint8_t delta = target - current;
    if (delta > step)
      return current + step;
  } else if (current > target) {
    uint8_t delta = current - target;
    if (delta > step)
      return current - step;
  }
  return target;
}

static void update_dew_control(Ports* ports) {
  if (!ports || !ports->have_temp)
    return;
  int32_t margin = dew_margin_centi(ports->temp_centi, ports->humid_centi);
  ports->dewpoint_centi = ports->temp_centi - margin;
  int32_t dew_m_on_centi = g_config.dew_m_on_centi;
  if (!ports->dew_active) {
    if (margin < dew_m_on_centi)
      ports->dew_active = true;
  } else {
    if (margin > DEW_M_OFF_CENTI)
      ports->dew_active = false;
  }

  uint8_t target_duty = 0;
  if (ports->dew_active) {
    uint8_t duty_min_pct = g_config.dew_duty_min_pct;
    uint8_t duty_max_pct = g_config.dew_duty_max_auto_pct;
    if (duty_min_pct > duty_max_pct) {
      duty_min_pct = duty_max_pct;
    }
    uint8_t duty_min = (uint8_t)((duty_min_pct * 255) / 100);
    uint8_t duty_max = (uint8_t)((duty_max_pct * 255) / 100);
    target_duty = smoothstep_duty(duty_min, duty_max, margin, dew_m_on_centi);
  }
  uint8_t limited = slew_limit(ports->dew_duty, target_duty);
  ports_apply_dew_duty(ports, limited);
}

#ifdef DEBUG
static void log_dew_debug(int32_t margin_centi, uint8_t duty) {
  out('!');
  out(F("dew margin="));
  out(margin_centi / 100);
  out('.');
  int32_t frac = margin_centi % 100;
  if (frac < 0)
    frac = -frac;
  if (frac < 10)
    out('0');
  out(frac);
  out(F("C duty="));
  out(duty);
  out(F(" ("));
  out((int32_t)((duty * 100L + 127) / 255));
  out(F("%)"));
  out('\n');
}
#endif
static void init_board_pins() {
  pinMode(ISIN, INPUT);
  pinMode(VSIN, INPUT);
  pinMode(ISOUT, INPUT);

  pinMode(DSEL, OUTPUT);
  pinMode(MUX0, OUTPUT);
  pinMode(MUX1, OUTPUT);
  pinMode(MUX2, OUTPUT);
  pinMode(OLEN, OUTPUT);

  digitalWrite(OLEN, HIGH);
  digitalWrite(MUX0, LOW);
  digitalWrite(MUX1, LOW);
  digitalWrite(MUX2, LOW);
  digitalWrite(DSEL, HIGH);
}

void setup() {
  Serial.begin(SERIALPORTSPEED);
  Wire.begin();
  Wire.setClock(100000);

  init_board_pins();

  framing_init(&g_queue);
  ports_init(&g_ports);
  eeprom_cfg_init(&g_config);
  eeprom_name_init_defaults();
  ports_update_input_readings(&g_ports);
  // Apply config to runtime state
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    bool on = (g_config.portStatus >> i) & 0x01;
    g_ports.state[i] = on;
  }
  for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
    g_ports.pwm_level[i] = g_config.pwmPorts[i];
    g_ports.pwm_mode[i] = g_config.pwmPortMode[i];
  }
  if (!ports_overvoltage(&g_ports)) {
    ports_apply_config(&g_ports);
  } else {
    ports_all_off(&g_ports);
  }
  g_last_refresh = millis();
  g_port_index = 0;
  g_chip = 0;
  g_dsel = true;
  g_last_sensor_ms = millis();
  g_last_dew_ms = millis();
  g_last_dewpoint_ms = millis();
#ifdef DEBUG
  g_last_dew_log_ms = millis();
#endif

  strncpy(g_board_signature, BOARD_SIGNATURE_BASE, BOARD_SIGNATURE_MAX_LEN);
  g_board_signature[BOARD_SIGNATURE_MAX_LEN - 1] = '\0';
  probes_detect(&g_probes, &g_ports);
}

void loop() {
  framing_poll(&g_queue);

  unsigned long now = millis();
  if ((now - g_last_refresh) >= REFRESH) {
    g_state = STATE_READ;
    g_last_refresh = now;
  }

  switch (g_state) {
  case STATE_IDLE: {
    if (framing_has_command(&g_queue)) {
      char cmd[MAXCOMMAND];
      if (framing_pop(&g_queue, cmd)) {
        protocol_handle(cmd, &g_ports);
      }
    }
    break;
  }
  case STATE_READ:
    ports_update_input_readings(&g_ports);
    if (ports_overvoltage(&g_ports)) {
      ports_all_off(&g_ports);
      g_config.portStatus = 0;
      for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
        g_config.pwmPorts[i] = g_ports.pwm_level[i];
        g_config.pwmPortMode[i] = g_ports.pwm_mode[i];
      }
      eeprom_cfg_save(&g_config);
    }
    ports_update_port_current(&g_ports, g_port_index);
    if ((now - g_last_sensor_ms) >= SENSOR_READ_INTERVAL_MS) {
      bool ok = probes_update(&g_probes, &g_ports);
      if (!ok && g_ports.have_temp) {
        ports_disable_dew_mode(&g_ports);
        // Persist config after disabling dew mode.
        for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
          g_config.pwmPortMode[i] = g_ports.pwm_mode[i];
          g_config.pwmPorts[i] = g_ports.pwm_level[i];
        }
        eeprom_cfg_save(&g_config);
      }
      g_last_sensor_ms = now;
    }

    if ((now - g_last_dew_ms) >= 10000) {
      if (g_ports.have_temp) {
        update_dew_control(&g_ports);
      }
      g_last_dew_ms = now;
    }

    if ((now - g_last_dewpoint_ms) >= 10000) {
      if (g_ports.have_temp) {
        int32_t margin = dew_margin_centi(g_ports.temp_centi, g_ports.humid_centi);
        g_ports.dewpoint_centi = g_ports.temp_centi - margin;
      }
      g_last_dewpoint_ms = now;
    }

#ifdef DEBUG
    if ((now - g_last_dew_log_ms) >= DEBUG_DEW_LOG_INTERVAL_MS) {
      int32_t margin = dew_margin_centi(g_ports.temp_centi, g_ports.humid_centi);
      log_dew_debug(margin, g_ports.dew_duty);
      g_last_dew_log_ms = now;
    }
#endif
    g_state = STATE_SWAP;
    break;
  case STATE_SWAP:
    adc_swap_ports(PORT_COUNT);
    g_state = STATE_IDLE;
    break;
  }
}
