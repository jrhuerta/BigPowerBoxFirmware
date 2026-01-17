#include "protocol_format.h"

#include "board_config.h"
#include "eeprom_cfg.h"
#include "serial_out.h"

namespace {
void print_fixed_from_milli(int32_t milli) {
  int32_t centi = milli >= 0 ? (milli + 5) / 10 : (milli - 5) / 10;
  if (centi < 0) {
    out('-');
    centi = -centi;
  }
  out(centi / 100);
  out('.');
  int32_t frac = centi % 100;
  if (frac < 10)
    out('0');
  out(frac);
}

void print_centi(int32_t centi) {
  if (centi < 0) {
    out('-');
    centi = -centi;
  }
  out(centi / 100);
  out('.');
  int32_t frac = centi % 100;
  if (frac < 10)
    out('0');
  out(frac);
}
} // namespace

void protocol_send_ok(const __FlashStringHelper* tag) {
  out(SOCOMMAND);
  out(tag);
  out(EOCOMMAND);
}

void protocol_send_err() {
  out(SOCOMMAND);
  out(F("ERR"));
  out(EOCOMMAND);
}

void protocol_send_status(const Ports* ports) {
  out(SOCOMMAND);
  out('S');
  out(':');
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    out(ports_get_status_value(ports, i));
    if (i + 1 < PORT_COUNT)
      out(':');
  }
  out(':');
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    print_fixed_from_milli(ports_get_port_ma(ports, i));
    if (i + 1 < PORT_COUNT)
      out(':');
  }
  out(':');
  print_fixed_from_milli(ports_get_input_ma(ports));
  out(':');
  print_fixed_from_milli(ports_get_input_mv(ports));
  if (ports->have_temp) {
    out(':');
    print_centi(ports->temp_centi);
    out(':');
    print_centi(ports->humid_centi);
    out(':');
    print_centi(ports->dewpoint_centi);
    if (ports->have_press) {
      out(':');
      out(ports->pressure_hpa);
    }
  }
  out(EOCOMMAND);
}

void protocol_send_discovery() {
  out(SOCOMMAND);
  out('D');
  out(':');
  out(PROGRAM_NAME);
  out(':');
  out(PROGRAM_VERSION);
  out(':');
  out(g_board_signature);
  out(EOCOMMAND);
}

void protocol_send_pwm_mode(uint8_t port, uint8_t mode) {
  out(SOCOMMAND);
  out('G');
  out(':');
  if (port < 10)
    out('0');
  out(port);
  out(':');
  out(mode);
  out(EOCOMMAND);
}

void protocol_send_dew_margin(uint8_t port) {
  out(SOCOMMAND);
  out('H');
  out(':');
  if (port < 10)
    out('0');
  out(port);
  out(':');
  // Legacy protocol expects whole degrees C for the dew margin.
  out((int32_t)(g_config.dew_m_on_centi / 100));
  out(EOCOMMAND);
}

void protocol_send_name(uint8_t port, const char* name) {
  out(SOCOMMAND);
  out('N');
  out(':');
  if (port < 10)
    out('0');
  out(port);
  out(':');
  out(name);
  out(EOCOMMAND);
}
