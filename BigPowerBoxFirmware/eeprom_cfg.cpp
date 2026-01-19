#include "eeprom_cfg.h"

#include <EEPROM.h>
#include <stdio.h>
#include <string.h>

#include "ports.h"

namespace {
bool cfg_differs(const Config& a, const Config& b) {
  if (a.portStatus != b.portStatus)
    return true;
  for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
    if (a.pwmPorts[i] != b.pwmPorts[i])
      return true;
    if (a.pwmPortMode[i] != b.pwmPortMode[i])
      return true;
  }
  if (a.dew_m_on_centi != b.dew_m_on_centi)
    return true;
  if (a.dew_duty_min_pct != b.dew_duty_min_pct)
    return true;
  if (a.dew_duty_max_auto_pct != b.dew_duty_max_auto_pct)
    return true;
  return false;
}
} // namespace

void eeprom_cfg_defaults(Config* cfg) {
  if (!cfg)
    return;
  cfg->currentData = CURRENTCONFIGFLAG;
  cfg->portStatus = 0;
  for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
    cfg->pwmPorts[i] = 0;
    cfg->pwmPortMode[i] = PWM_MODE_VARIABLE;
  }
  cfg->dew_m_on_centi = DEW_M_ON_CENTI;
  cfg->dew_duty_min_pct = DEW_DUTY_MIN_PCT;
  cfg->dew_duty_max_auto_pct = DEW_DUTY_MAX_AUTO_PCT;
}

void eeprom_cfg_init(Config* cfg) {
  if (!cfg)
    return;
  eeprom_cfg_defaults(cfg);

  int addr = EEPROMCONFBASE;
  Config tmp;
  bool found = false;
  bool corrected = false;
  while (addr + (int)sizeof(Config) <= EEPROM.length()) {
    EEPROM.get(addr, tmp);
    if (tmp.currentData == CURRENTCONFIGFLAG) {
      *cfg = tmp;
      found = true;
    }
    addr += sizeof(Config);
  }

  if (found) {
    // Guard against stale EEPROM data from older firmware revisions.
    bool invalid_pwm_mode = false;
    for (uint8_t i = 0; i < PWM_PORT_COUNT; i++) {
      if (cfg->pwmPortMode[i] != PWM_MODE_VARIABLE &&
          cfg->pwmPortMode[i] != PWM_MODE_SWITCHABLE &&
          cfg->pwmPortMode[i] != PWM_MODE_DEW_AMBIENT) {
        invalid_pwm_mode = true;
        break;
      }
    }
    if (invalid_pwm_mode) {
      eeprom_cfg_defaults(cfg);
      corrected = true;
    }
    uint16_t valid_mask = (PORT_COUNT >= 16) ? 0xFFFFu : (uint16_t)((1u << PORT_COUNT) - 1u);
    uint16_t masked = cfg->portStatus & valid_mask;
    if (masked != cfg->portStatus) {
      cfg->portStatus = masked;
      corrected = true;
    }
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
      if (ports_port_type(i) == 'a') {
        uint16_t bit = (uint16_t)(1u << i);
        if ((cfg->portStatus & bit) == 0) {
          cfg->portStatus |= bit;
          corrected = true;
        }
      }
    }
    if (cfg->dew_m_on_centi < 0 || cfg->dew_m_on_centi > 500) {
      cfg->dew_m_on_centi = DEW_M_ON_CENTI;
      corrected = true;
    }
    if (cfg->dew_duty_min_pct > 100 || cfg->dew_duty_max_auto_pct > 100) {
      cfg->dew_duty_min_pct = DEW_DUTY_MIN_PCT;
      cfg->dew_duty_max_auto_pct = DEW_DUTY_MAX_AUTO_PCT;
      corrected = true;
    }
    if (cfg->dew_duty_min_pct > cfg->dew_duty_max_auto_pct) {
      cfg->dew_duty_min_pct = DEW_DUTY_MIN_PCT;
      cfg->dew_duty_max_auto_pct = DEW_DUTY_MAX_AUTO_PCT;
      corrected = true;
    }
  }

  if (!found || corrected) {
    eeprom_cfg_save(cfg);
  }
}

void eeprom_name_init_defaults() {
  char buf[NAMELENGTH];
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    int addr = EEPROMNAMEBASE + (i * NAMELENGTH);
    EEPROM.get(addr, buf);
    bool uninitialized = true;
    for (uint8_t j = 0; j < NAMELENGTH; j++) {
      if ((uint8_t)buf[j] != 0xFF) {
        uninitialized = false;
        break;
      }
    }
    if (uninitialized) {
      char name[NAMELENGTH];
      snprintf(name, sizeof(name), "Port%02u", (unsigned)i);
      eeprom_name_write(i, name);
    }
  }
}

void eeprom_cfg_save(const Config* cfg) {
  if (!cfg)
    return;
  Config saved;
  int addr = EEPROMCONFBASE;
  int last_addr = EEPROMCONFBASE;
  while (addr + (int)sizeof(Config) <= EEPROM.length()) {
    EEPROM.get(addr, saved);
    if (saved.currentData == CURRENTCONFIGFLAG) {
      last_addr = addr;
    }
    addr += sizeof(Config);
  }

  EEPROM.get(last_addr, saved);
  if (!cfg_differs(*cfg, saved))
    return;

  EEPROM.write(last_addr, OLDCONFIGFLAG);
  int next_addr = last_addr + sizeof(Config);
  if (next_addr + (int)sizeof(Config) > EEPROM.length()) {
    next_addr = EEPROMCONFBASE;
  }
  EEPROM.put(next_addr, *cfg);
}

void eeprom_name_read(uint8_t port, char* out) {
  if (!out)
    return;
  int addr = EEPROMNAMEBASE + (port * NAMELENGTH);
  EEPROM.get(addr, out);
  out[NAMELENGTH - 1] = '\0';
}

void eeprom_name_write(uint8_t port, const char* name) {
  if (!name)
    return;
  char buf[NAMELENGTH];
  char old[NAMELENGTH];
  int addr = EEPROMNAMEBASE + (port * NAMELENGTH);
  EEPROM.get(addr, old);
  strncpy(buf, name, NAMELENGTH);
  buf[NAMELENGTH - 1] = '\0';
  if (strcmp(old, buf) == 0)
    return;
  EEPROM.put(addr, buf);
}
