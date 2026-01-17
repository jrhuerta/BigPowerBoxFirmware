#include "serial_framing.h"

#include <string.h>

#include "board_config.h"

namespace {
bool in_frame = false;
uint8_t idx = 0;
char rxbuf[MAXCOMMAND];
} // namespace

void framing_init(CommandQueue* q) {
  if (!q)
    return;
  q->head = 0;
  q->tail = 0;
  q->count = 0;
  in_frame = false;
  idx = 0;
  rxbuf[0] = '\0';
}

static void queue_push(CommandQueue* q, const char* src) {
  if (q->count >= QUEUELENGTH) {
    // Drop on overflow
    return;
  }
  strncpy(q->data[q->head], src, MAXCOMMAND);
  q->data[q->head][MAXCOMMAND - 1] = '\0';
  q->head = (q->head + 1) % QUEUELENGTH;
  q->count++;
}

void framing_poll(CommandQueue* q) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == SOCOMMAND) {
      in_frame = true;
      idx = 0;
      rxbuf[0] = '\0';
      continue;
    }
    if (!in_frame) {
      continue;
    }
    if (c == EOCOMMAND) {
      rxbuf[idx] = '\0';
      queue_push(q, rxbuf);
      in_frame = false;
      idx = 0;
      continue;
    }
    if (idx < (MAXCOMMAND - 1)) {
      rxbuf[idx++] = c;
    } else {
      // overflow, drop frame
      in_frame = false;
      idx = 0;
    }
  }
}

bool framing_has_command(const CommandQueue* q) {
  return q && q->count > 0;
}

bool framing_pop(CommandQueue* q, char* out) {
  if (!q || q->count == 0 || !out)
    return false;
  strncpy(out, q->data[q->tail], MAXCOMMAND);
  out[MAXCOMMAND - 1] = '\0';
  q->tail = (q->tail + 1) % QUEUELENGTH;
  q->count--;
  return true;
}
