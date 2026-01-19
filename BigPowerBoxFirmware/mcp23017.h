#pragma once

#include <Arduino.h>

struct Mcp23017 {
  uint8_t addr;
  uint8_t gpio_a;
  uint8_t gpio_b;
};

bool mcp23017_init(Mcp23017* m);
bool mcp23017_write_pin(Mcp23017* m, uint8_t pin, bool value);
bool mcp23017_write_porta(Mcp23017* m, uint8_t value);
bool mcp23017_read_gpioa(Mcp23017* m, uint8_t* value);
bool mcp23017_read_gpiob(Mcp23017* m, uint8_t* value);