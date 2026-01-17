#include "ports.h"

static bool is_pwm_port(uint8_t port_index) {
  return BOARD_SIGNATURE_BASE[port_index] == 'p';
}

static bool is_mcp_port(uint8_t port_index) {
  return BOARD_SIGNATURE_BASE[port_index] == 'm';
}

static bool is_direct_port(uint8_t port_index) {
  return BOARD_SIGNATURE_BASE[port_index] == 's';
}

static bool is_always_on(uint8_t port_index) {
  return BOARD_SIGNATURE_BASE[port_index] == 'a';
}

static int8_t pwm_index_from_port(uint8_t port_index) {
  if (!is_pwm_port(port_index))
    return -1;
  uint8_t count = 0;
  for (uint8_t i = 0; i < port_index; i++) {
    if (is_pwm_port(i))
      count++;
  }
  if (count >= PWM_PORT_COUNT)
    return -1;
  return (int8_t)count;
}

static int32_t adc_read_mv(uint8_t pin) {
  int32_t adc = analogRead(pin);
  return (int32_t)((int64_t)adc * VCC_MV / 1023);
}

bool ports_is_controllable(uint8_t port_index) {
  if (port_index >= PORT_COUNT)
    return false;
  return is_mcp_port(port_index) || is_pwm_port(port_index) || is_direct_port(port_index);
}

char ports_port_type(uint8_t port_index) {
  if (port_index >= PORT_COUNT)
    return '\0';
  return BOARD_SIGNATURE_BASE[port_index];
}

void ports_init(Ports* ports) {
  if (!ports)
    return;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    ports->state[i] = is_always_on(i);
    ports->port_ma[i] = 0;
    ports->port_ma_avg[i].reset();
  }
  for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
    ports->pwm_mode[i] = PWM_MODE_VARIABLE;
    ports->pwm_level[i] = 0;
  }
  ports->input_mv = 0;
  ports->input_ma = 0;
  ports->input_mv_avg.reset();
  ports->input_ma_avg.reset();
  ports->have_temp = false;
  ports->have_press = false;
  ports->temp_centi = 0;
  ports->humid_centi = 0;
  ports->pressure_hpa = 0;
  ports->temp_ema.reset();
  ports->humid_ema.reset();
  ports->press_ema.reset();
  ports->dew_active = false;
  ports->dew_duty = 0;
  ports->dewpoint_centi = 0;
#ifdef DEBUG
  ports->debug_override = false;
  ports->debug_temp_centi = DEBUG_FAKE_TEMP_CENTI;
  ports->debug_humid_centi = DEBUG_FAKE_HUMID_CENTI;
  ports->debug_fake_probe = false;
#endif
  ports->mcp.addr = MCP23017_ADDR;
  ports->mcp.gpio_a = 0x00;
  ports->mcp.gpio_b = 0x00;

  // Initialize MCP and set all port A outputs low.
  mcp23017_init(&ports->mcp);

  // Initialize PWM output pins.
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    if (is_pwm_port(i)) {
      uint8_t pwm_pin = ports2Pin[i];
      pinMode(pwm_pin, OUTPUT);
      analogWrite(pwm_pin, 0);
    }
    if (is_direct_port(i)) {
      uint8_t pin = ports2Pin[i];
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }
}

bool ports_set(Ports* ports, uint8_t port_index, bool on) {
  if (!ports || port_index >= PORT_COUNT)
    return false;
  if (!ports_is_controllable(port_index))
    return false;

  ports->state[port_index] = on;

  if (is_mcp_port(port_index)) {
    uint8_t pin = ports2Pin[port_index];
    return mcp23017_write_pin(&ports->mcp, pin, on);
  }
  if (is_direct_port(port_index)) {
    uint8_t pin = ports2Pin[port_index];
    digitalWrite(pin, on ? HIGH : LOW);
    return true;
  }
  if (is_pwm_port(port_index)) {
    int8_t pwm_index = pwm_index_from_port(port_index);
    if (pwm_index < 0)
      return false;
    uint8_t pin = ports2Pin[port_index];
    if (ports->pwm_mode[pwm_index] == PWM_MODE_SWITCHABLE) {
      digitalWrite(pin, on ? HIGH : LOW);
      ports->pwm_level[pwm_index] = on ? 255 : 0;
      return true;
    }
    return false;
  }
  return false;
}

bool ports_set_pwm_level(Ports* ports, uint8_t port_index, uint8_t level) {
  if (!ports || port_index >= PORT_COUNT)
    return false;
  if (!is_pwm_port(port_index))
    return false;
  int8_t pwm_index = pwm_index_from_port(port_index);
  if (pwm_index < 0)
    return false;
  if (ports->pwm_mode[pwm_index] != PWM_MODE_VARIABLE)
    return false;

  uint8_t pin = ports2Pin[port_index];
  ports->pwm_level[pwm_index] = level;
  ports->state[port_index] = (level > 0);
  analogWrite(pin, level);
  return true;
}

bool ports_set_pwm_mode(Ports* ports, uint8_t port_index, uint8_t mode) {
  if (!ports || port_index >= PORT_COUNT)
    return false;
  if (!is_pwm_port(port_index))
    return false;
  if (mode != PWM_MODE_VARIABLE && mode != PWM_MODE_SWITCHABLE && mode != PWM_MODE_DEW_AMBIENT) {
    return false;
  }

  int8_t pwm_index = pwm_index_from_port(port_index);
  if (pwm_index < 0)
    return false;

  ports->pwm_mode[pwm_index] = mode;
  uint8_t pin = ports2Pin[port_index];
  if (mode == PWM_MODE_SWITCHABLE) {
    bool on = ports->pwm_level[pwm_index] > 0 || ports->state[port_index];
    digitalWrite(pin, on ? HIGH : LOW);
    ports->pwm_level[pwm_index] = on ? 255 : 0;
    ports->state[port_index] = on;
  } else if (mode == PWM_MODE_DEW_AMBIENT) {
    ports->pwm_level[pwm_index] = 0;
    ports->state[port_index] = false;
    analogWrite(pin, 0);
  } else {
    analogWrite(pin, ports->pwm_level[pwm_index]);
    ports->state[port_index] = (ports->pwm_level[pwm_index] > 0);
  }
  return true;
}

bool ports_get(const Ports* ports, uint8_t port_index) {
  if (!ports || port_index >= PORT_COUNT)
    return false;
  return ports->state[port_index];
}

uint8_t ports_get_pwm_mode(const Ports* ports, uint8_t port_index) {
  if (!ports || port_index >= PORT_COUNT)
    return PWM_MODE_VARIABLE;
  if (!is_pwm_port(port_index))
    return PWM_MODE_VARIABLE;
  int8_t pwm_index = pwm_index_from_port(port_index);
  if (pwm_index < 0)
    return PWM_MODE_VARIABLE;
  return ports->pwm_mode[pwm_index];
}

uint8_t ports_get_status_value(const Ports* ports, uint8_t port_index) {
  if (!ports || port_index >= PORT_COUNT)
    return 0;
  if (!is_pwm_port(port_index)) {
    return ports->state[port_index] ? 1 : 0;
  }
  int8_t pwm_index = pwm_index_from_port(port_index);
  if (pwm_index < 0)
    return 0;
  if (ports->pwm_mode[pwm_index] == PWM_MODE_SWITCHABLE) {
    return ports->state[port_index] ? 255 : 0;
  }
  return ports->pwm_level[pwm_index];
}

void ports_update_input_readings(Ports* ports) {
  if (!ports)
    return;
  int32_t vs_mv = adc_read_mv(VSIN);
  int32_t raw_input_mv = (int32_t)((int64_t)vs_mv * RDIVIN_OHMS / RDIVOUT_OHMS);
  ports->input_mv = ports->input_mv_avg.add(raw_input_mv);

  int32_t is_mv = adc_read_mv(ISIN);
  int32_t delta_mv = is_mv - (VCC_MV / 2);
  int32_t raw_input_ma = (int32_t)((int64_t)delta_mv * 1000 / KINIS_MV_PER_A);
  ports->input_ma = ports->input_ma_avg.add(raw_input_ma);
}

void ports_all_off(Ports* ports) {
  if (!ports)
    return;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    char type = ports_port_type(i);
    if (type == 'a')
      continue;
    if (type == 'm') {
      mcp23017_write_pin(&ports->mcp, ports2Pin[i], false);
      ports->state[i] = false;
    } else if (type == 's') {
      digitalWrite(ports2Pin[i], LOW);
      ports->state[i] = false;
    } else if (type == 'p') {
      int8_t pwm_index = pwm_index_from_port(i);
      if (pwm_index < 0)
        continue;
      ports->pwm_level[pwm_index] = 0;
      ports->pwm_mode[pwm_index] = PWM_MODE_VARIABLE;
      ports->state[i] = false;
      analogWrite(ports2Pin[i], 0);
    }
  }
  ports->dew_active = false;
  ports->dew_duty = 0;
}

void ports_update_port_current(Ports* ports, uint8_t port_index) {
  if (!ports || port_index >= PORT_COUNT)
    return;
  char type = ports_port_type(port_index);
  int32_t is_mv = adc_read_mv(ISOUT);

  if (type == 'a') {
    int32_t delta_mv = is_mv - (VCC_MV / 2);
    int32_t raw_port_ma = (int32_t)((int64_t)delta_mv * 1000 / KOUTIS_MV_PER_A);
    ports->port_ma[port_index] = ports->port_ma_avg[port_index].add(raw_port_ma);
  } else if (type == 's' || type == 'm' || type == 'p') {
    int32_t raw_port_ma = (int32_t)((int64_t)is_mv * KILIS / ROUTIS_OHMS);
    ports->port_ma[port_index] = ports->port_ma_avg[port_index].add(raw_port_ma);
  } else {
    ports->port_ma[port_index] = 0;
  }
}

int32_t ports_get_input_mv(const Ports* ports) {
  if (!ports)
    return 0;
  return ports->input_mv;
}

int32_t ports_get_input_ma(const Ports* ports) {
  if (!ports)
    return 0;
  return ports->input_ma;
}

int32_t ports_get_port_ma(const Ports* ports, uint8_t port_index) {
  if (!ports || port_index >= PORT_COUNT)
    return 0;
  return ports->port_ma[port_index];
}

bool ports_overvoltage(const Ports* ports) {
  if (!ports)
    return false;
  return (ports->input_mv / 1000.0f) > MAXINVOLTS;
}

void ports_apply_dew_duty(Ports* ports, uint8_t duty) {
  if (!ports)
    return;
  ports->dew_duty = duty;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    if (ports_port_type(i) != 'p')
      continue;
    int8_t pwm_index = pwm_index_from_port(i);
    if (pwm_index < 0)
      continue;
    if (ports->pwm_mode[pwm_index] != PWM_MODE_DEW_AMBIENT)
      continue;
    ports->pwm_level[pwm_index] = duty;
    ports->state[i] = (duty > 0);
    analogWrite(ports2Pin[i], duty);
  }
}

void ports_disable_dew_mode(Ports* ports) {
  if (!ports)
    return;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    if (ports_port_type(i) != 'p')
      continue;
    int8_t pwm_index = pwm_index_from_port(i);
    if (pwm_index < 0)
      continue;
    if (ports->pwm_mode[pwm_index] != PWM_MODE_DEW_AMBIENT)
      continue;
    ports->pwm_mode[pwm_index] = PWM_MODE_VARIABLE;
    ports->pwm_level[pwm_index] = 0;
    ports->state[i] = false;
    analogWrite(ports2Pin[i], 0);
  }
  ports->dew_active = false;
  ports->dew_duty = 0;
}

void ports_apply_config(Ports* ports) {
  if (!ports)
    return;
  // MCP ports and direct ports use state[].
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    char type = ports_port_type(i);
    if (type == 'm') {
      mcp23017_write_pin(&ports->mcp, ports2Pin[i], ports->state[i]);
    } else if (type == 's') {
      digitalWrite(ports2Pin[i], ports->state[i] ? HIGH : LOW);
    } else if (type == 'p') {
      int8_t pwm_index = pwm_index_from_port(i);
      if (pwm_index < 0)
        continue;
      if (ports->pwm_mode[pwm_index] == PWM_MODE_SWITCHABLE) {
        digitalWrite(ports2Pin[i], ports->state[i] ? HIGH : LOW);
      } else if (ports->pwm_mode[pwm_index] == PWM_MODE_DEW_AMBIENT) {
        analogWrite(ports2Pin[i], 0);
        ports->pwm_level[pwm_index] = 0;
        ports->state[i] = false;
      } else {
        analogWrite(ports2Pin[i], ports->pwm_level[pwm_index]);
        ports->state[i] = (ports->pwm_level[pwm_index] > 0);
      }
    }
  }
}
