#pragma once

#include <Arduino.h>

#include "board_config.h"

struct CommandQueue {
  char data[QUEUELENGTH][MAXCOMMAND];
  uint8_t head;
  uint8_t tail;
  uint8_t count;
};

void framing_init(CommandQueue* q);
void framing_poll(CommandQueue* q);
bool framing_has_command(const CommandQueue* q);
bool framing_pop(CommandQueue* q, char* out);
