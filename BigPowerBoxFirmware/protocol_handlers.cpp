#include "protocol_handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_config.h"
#include "eeprom_cfg.h"
#include "protocol_format.h"

namespace {
uint8_t parse_port(const char* s, bool* ok) {
  if (!s || !ok)
    return 0;
  char* end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s || v < 0 || v > 255) {
    *ok = false;
    return 0;
  }
  *ok = true;
  return (uint8_t)v;
}

int16_t parse_int16(const char* s, bool* ok) {
  if (!s || !ok)
    return 0;
  char* end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s || v < -32768 || v > 32767) {
    *ok = false;
    return 0;
  }
  *ok = true;
  return (int16_t)v;
}

// Map a physical port index to the compact PWM port index used in config.
int8_t pwm_index_for_port(uint8_t port) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    if (ports_port_type(i) == 'p') {
      if (i == port)
        return (int8_t)count;
      count++;
    }
  }
  return -1;
}

// Persist on/off state for all ports to EEPROM config.
void save_port_status_config(const Ports* ports) {
  g_config.portStatus = 0;
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    if (ports_get(ports, i))
      g_config.portStatus |= (1u << i);
  }
  eeprom_cfg_save(&g_config);
}

void handle_ping() {
  protocol_send_ok(F("POK"));
}

void handle_discovery() {
  protocol_send_discovery();
}

void handle_status(const Ports* ports) {
  protocol_send_status(ports);
}

void handle_port_on(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 2) {
    protocol_send_err();
    return;
  }
  if (ports_overvoltage(ports)) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  char type = ports_port_type(port);
  if (!ok || !ports_is_controllable(port)) {
    protocol_send_err();
    return;
  }
  if (type == 'p' && ports_get_pwm_mode(ports, port) != PWM_MODE_SWITCHABLE) {
    protocol_send_err();
    return;
  }
  if (!ports_set(ports, port, true)) {
    protocol_send_err();
    return;
  }
  save_port_status_config(ports);
  protocol_send_ok(F("OOK"));
}

void handle_port_off(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 2) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  char type = ports_port_type(port);
  if (!ok || !ports_is_controllable(port)) {
    protocol_send_err();
    return;
  }
  if (type == 'p' && ports_get_pwm_mode(ports, port) != PWM_MODE_SWITCHABLE) {
    protocol_send_err();
    return;
  }
  if (!ports_set(ports, port, false)) {
    protocol_send_err();
    return;
  }
  save_port_status_config(ports);
  protocol_send_ok(F("FOK"));
}

void handle_pwm_level(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 3) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok || ports_port_type(port) != 'p') {
    protocol_send_err();
    return;
  }
  uint8_t current_mode = ports_get_pwm_mode(ports, port);
  if (current_mode != PWM_MODE_VARIABLE) {
    protocol_send_err();
    return;
  }
  bool ok_level = false;
  uint8_t level = parse_port(argv[2], &ok_level);
  if (!ok_level) {
    protocol_send_err();
    return;
  }
  if (level > 0 && ports_overvoltage(ports)) {
    protocol_send_err();
    return;
  }
  if (!ports_set_pwm_level(ports, port, level)) {
    protocol_send_err();
    return;
  }
  int8_t pwm_index = pwm_index_for_port(port);
  if (pwm_index >= 0) {
    g_config.pwmPorts[pwm_index] = level;
    eeprom_cfg_save(&g_config);
  }
  protocol_send_ok(F("WOK"));
}

void handle_pwm_mode(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 3) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok || ports_port_type(port) != 'p') {
    protocol_send_err();
    return;
  }
  bool ok_mode = false;
  uint8_t mode = parse_port(argv[2], &ok_mode);
  if (!ok_mode) {
    protocol_send_err();
    return;
  }
  if (mode == PWM_MODE_DEW_AMBIENT && !ports->have_temp) {
    protocol_send_err();
    return;
  }
  if (!ports_set_pwm_mode(ports, port, mode)) {
    protocol_send_err();
    return;
  }
  int8_t pwm_index = pwm_index_for_port(port);
  if (pwm_index >= 0) {
    g_config.pwmPortMode[pwm_index] = mode;
    eeprom_cfg_save(&g_config);
  }
  protocol_send_ok(F("COK"));
}

void handle_set_name(char* const* argv, uint8_t argc) {
  if (argc < 3) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok) {
    protocol_send_err();
    return;
  }
  eeprom_name_write(port, argv[2]);
  protocol_send_ok(F("MOK"));
}

void handle_get_name(char* const* argv, uint8_t argc) {
  if (argc < 2) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok) {
    protocol_send_err();
    return;
  }
  char name[NAMELENGTH];
  eeprom_name_read(port, name);
  protocol_send_name(port, name);
}

void handle_legacy_temp() {
  // Backwards compatibility: accept command but perform no action.
  protocol_send_ok(F("TOK"));
}

void handle_legacy_dew_margin(char* const* argv, uint8_t argc) {
  // Backwards compatibility: return dew margin (whole degrees C).
  if (argc < 2) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok) {
    protocol_send_err();
    return;
  }
  protocol_send_dew_margin(port);
}

void handle_dew_config(char* const* argv, uint8_t argc) {
  if (argc < 2 || argc > 4) {
    protocol_send_err();
    return;
  }
  bool ok_on = false;
  int16_t dew_on_deg = parse_int16(argv[1], &ok_on);
  if (!ok_on || dew_on_deg < 0 || dew_on_deg > 5) {
    protocol_send_err();
    return;
  }

  uint8_t duty_min_pct = g_config.dew_duty_min_pct;
  uint8_t duty_max_pct = g_config.dew_duty_max_auto_pct;
  if (argc >= 3) {
    bool ok_min = false;
    duty_min_pct = parse_port(argv[2], &ok_min);
    if (!ok_min || duty_min_pct > 100) {
      protocol_send_err();
      return;
    }
  }
  if (argc >= 4) {
    bool ok_max = false;
    duty_max_pct = parse_port(argv[3], &ok_max);
    if (!ok_max || duty_max_pct > 100) {
      protocol_send_err();
      return;
    }
  }
  if (duty_min_pct > duty_max_pct) {
    protocol_send_err();
    return;
  }

  g_config.dew_m_on_centi = (int16_t)(dew_on_deg * 100);
  g_config.dew_duty_min_pct = duty_min_pct;
  g_config.dew_duty_max_auto_pct = duty_max_pct;
  eeprom_cfg_save(&g_config);
  protocol_send_ok(F("KOK"));
}

void reset_port_names() {
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    char name[NAMELENGTH];
    snprintf(name, sizeof(name), "Port%02u", (unsigned)i);
    eeprom_name_write(i, name);
  }
}

void reset_config_and_ports(Ports* ports) {
  ports_all_off(ports);
  eeprom_cfg_defaults(&g_config);
  eeprom_cfg_save(&g_config);
}

void handle_reset(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 2 || !argv[1]) {
    protocol_send_err();
    return;
  }
  if (strcmp(argv[1], "NAMES") == 0) {
    reset_port_names();
  } else if (strcmp(argv[1], "CONF") == 0) {
    reset_config_and_ports(ports);
  } else if (strcmp(argv[1], "ALL") == 0) {
    reset_port_names();
    reset_config_and_ports(ports);
  } else {
    protocol_send_err();
    return;
  }
  protocol_send_ok(F("ROK"));
}

void handle_get_pwm_mode(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc < 2) {
    protocol_send_err();
    return;
  }
  bool ok = false;
  uint8_t port = parse_port(argv[1], &ok);
  if (!ok || ports_port_type(port) != 'p') {
    protocol_send_err();
    return;
  }
  uint8_t mode = ports_get_pwm_mode(ports, port);
  protocol_send_pwm_mode(port, mode);
}

#ifdef DEBUG
void handle_debug_override(char* const* argv, uint8_t argc, Ports* ports) {
  if (argc == 2 && argv[1][0] == '\0') {
    ports->debug_override = false;
    protocol_send_ok(F("XOK"));
    return;
  }
  if (argc < 3) {
    protocol_send_err();
    return;
  }
  bool ok_t = false;
  int16_t temp = parse_int16(argv[1], &ok_t);
  if (!ok_t) {
    protocol_send_err();
    return;
  }
  bool ok_h = false;
  int16_t humid = parse_int16(argv[2], &ok_h);
  if (!ok_h) {
    protocol_send_err();
    return;
  }
  if (humid < 0 || humid > 100) {
    protocol_send_err();
    return;
  }
  ports->debug_temp_centi = (int32_t)temp * 100;
  ports->debug_humid_centi = (int32_t)humid * 100;
  ports->debug_override = true;
  protocol_send_ok(F("XOK"));
}
#endif
} // namespace

void protocol_handle_command(char command, char* const* argv, uint8_t argc, Ports* ports) {
  switch (command) {
  case 'P':
    handle_ping();
    break;
  case 'D':
    handle_discovery();
    break;
  case 'S':
    handle_status(ports);
    break;
  case 'O':
    handle_port_on(argv, argc, ports);
    break;
  case 'F':
    handle_port_off(argv, argc, ports);
    break;
  case 'W':
    handle_pwm_level(argv, argc, ports);
    break;
  case 'C':
    handle_pwm_mode(argv, argc, ports);
    break;
  case 'M':
    handle_set_name(argv, argc);
    break;
  case 'N':
    handle_get_name(argv, argc);
    break;
  case 'T':
    handle_legacy_temp();
    break;
  case 'H':
    handle_legacy_dew_margin(argv, argc);
    break;
  case 'K':
    handle_dew_config(argv, argc);
    break;
  case 'R':
    handle_reset(argv, argc, ports);
    break;
  case 'G':
    handle_get_pwm_mode(argv, argc, ports);
    break;
#ifdef DEBUG
  case 'X':
    handle_debug_override(argv, argc, ports);
    break;
#endif
  default:
    protocol_send_err();
    break;
  }
}
