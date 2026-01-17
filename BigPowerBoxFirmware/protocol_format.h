#pragma once

#include <Arduino.h>

#include "ports.h"

void protocol_send_ok(const __FlashStringHelper* tag);
void protocol_send_err();
void protocol_send_status(const Ports* ports);
void protocol_send_discovery();
void protocol_send_pwm_mode(uint8_t port, uint8_t mode);
void protocol_send_dew_margin(uint8_t port);
void protocol_send_name(uint8_t port, const char* name);
