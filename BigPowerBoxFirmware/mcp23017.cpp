#include "mcp23017.h"

#include "i2c_bus.h"

namespace {
constexpr uint8_t REG_IODIRA = 0x00;
constexpr uint8_t REG_IODIRB = 0x01;
constexpr uint8_t REG_GPIOA = 0x12;
constexpr uint8_t REG_GPIOB = 0x13;
} // namespace

bool mcp23017_init(Mcp23017* m) {
  if (!m)
    return false;
  m->gpio_a = 0x00;
  m->gpio_b = 0x00;

  uint8_t out = 0x00; // all outputs
  if (!i2c_write_reg(m->addr, REG_IODIRA, &out, 1))
    return false;
  if (!i2c_write_reg(m->addr, REG_IODIRB, &out, 1))
    return false;

  if (!i2c_write_reg(m->addr, REG_GPIOA, &m->gpio_a, 1))
    return false;
  if (!i2c_write_reg(m->addr, REG_GPIOB, &m->gpio_b, 1))
    return false;

  return true;
}

bool mcp23017_write_pin(Mcp23017* m, uint8_t pin, bool value) {
  if (!m)
    return false;
  if (pin > 7)
    return false;
  if (value) {
    m->gpio_a |= (1u << pin);
  } else {
    m->gpio_a &= ~(1u << pin);
  }
  return i2c_write_reg(m->addr, REG_GPIOA, &m->gpio_a, 1);
}

bool mcp23017_write_porta(Mcp23017* m, uint8_t value) {
  if (!m)
    return false;
  m->gpio_a = value;
  return i2c_write_reg(m->addr, REG_GPIOA, &m->gpio_a, 1);
}

bool mcp23017_read_gpioa(Mcp23017* m, uint8_t* value) {
  if (!m || !value)
    return false;
  if (!i2c_read_reg(m->addr, REG_GPIOA, value, 1))
    return false;
  return true;
}

bool mcp23017_read_gpiob(Mcp23017* m, uint8_t* value) {
  if (!m || !value)
    return false;
  if (!i2c_read_reg(m->addr, REG_GPIOB, value, 1))
    return false;
  return true;
}
