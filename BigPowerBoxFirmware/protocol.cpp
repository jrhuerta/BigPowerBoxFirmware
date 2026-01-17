#include "protocol.h"

#include <string.h>

#include "protocol_format.h"
#include "protocol_handlers.h"

void protocol_handle(char* cmd, Ports* ports) {
  if (!cmd || !ports || cmd[0] == '\0') {
    protocol_send_err();
    return;
  }

  char* argv[3] = {nullptr, nullptr, nullptr};
  uint8_t argc = 0;
  char* token = strtok(cmd, ":");
  while (token && argc < 3) {
    argv[argc++] = token;
    token = strtok(nullptr, ":");
  }

  if (argc == 0 || !argv[0] || argv[0][0] == '\0') {
    protocol_send_err();
    return;
  }

  protocol_handle_command(argv[0][0], argv, argc, ports);
}
