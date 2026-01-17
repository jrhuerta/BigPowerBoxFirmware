#pragma once

#include <Arduino.h>

#include "ports.h"

void protocol_handle_command(char command, char* const* argv, uint8_t argc, Ports* ports);
