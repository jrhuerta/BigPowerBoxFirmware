# BigPowerBox Minimal Firmware
An open-source power distribution switch firmware for 12VDC applications.

Please see the hardware README before using this firmware.

- [BigPowerBox Minimal Firmware](#bigpowerbox-minimal-firmware)
- [Acknowledgements](#acknowledgements)
- [Introduction](#introduction)
- [Key Differences vs Original Firmware](#key-differences-vs-original-firmware)
- [Logic](#logic)
- [Required Libraries](#required-libraries)
- [Modularity](#modularity)
- [Hardware Expansion](#hardware-expansion)
- [Temperature Probes](#temperature-probes)
- [Storage](#storage)
- [Safety and Validation](#safety-and-validation)
- [Command Protocol](#command-protocol)
  - [Available commands](#available-commands)
- [Status Fields](#status-fields)
- [PWM Mode Behavior](#pwm-mode-behavior)
- [Ambient PWM Control](#ambient-pwm-control)
- [Reliability and Resource Use](#reliability-and-resource-use)
- [Pin Compatibility](#pin-compatibility)
- [File Layout](#file-layout)
- [Build / Upload](#build--upload)

# Acknowledgements
This firmware is built on the foundation laid by the original BigPowerBox
author, Michel Moriniaux, and the upstream project at
https://github.com/MichelMoriniaux/BigPowerBox. The protocol, hardware mapping,
and feature set are deeply informed by that work, and this project would not be
possible without their effort and design decisions.

# Introduction
This firmware is a lean rewrite of the BigPowerBox firmware focused on core
state-machine control, port management, and a protocol that remains compatible
with the original command and response formats.

# Key Differences vs Original Firmware
- Status field order changed: **dew point now appears before optional pressure**.
- Ambient dew control is implemented with smoothstep + hysteresis + slew limit.
- Dew settings are configurable via `K` and persisted to EEPROM.
- Reset command `R` supports scoped resets: names, config, or both.
- Port names now initialize to defaults (`Port00`..`PortNN`) on first boot.
- Debug mode can override temp/humidity with `X` and emits periodic dew logs.

# Logic
The firmware runs as a state machine. It stays in an idle loop, periodically
reads ADC values, updates dew control, and cycles the output-current mux. Each
loop also processes at most one command from the serial queue.

# Required Libraries
This firmware avoids external libraries beyond the Arduino core. The only
required dependency is `Wire` for I2C.

# Debug Configuration
The debug build can optionally synthesize a fake ambient probe when no sensor is
connected. Set `DEBUG_FAKE_PROBE` in `board_config.h`:
- `0`: disabled (no temp/humidity/dew in status if no sensor is found).
- `1`: enabled (injects fake temp/humidity readings for testing).

# Modularity
Port order and capabilities are defined by the board signature in
`board_config.h`. The signature controls the port list and the order of status
fields for the protocol.

# Hardware Expansion
The board exposes I2C on the RJ12 connector and supports ambient probes.
Currently supported sensors are SHT31, AHTx0, and BME280.

# Temperature Probes
At boot, the firmware scans for an ambient probe. If present, temperature and
humidity are added to the status string. If a pressure-capable probe is found,
pressure is also reported.

Sensor values are smoothed with an EMA filter, and current readings use a small
rolling average to reduce noise in the status output.

# Storage
Port names and configuration are stored in EEPROM. Port names are fixed-size
slots, and configuration is wear-leveled.

# Safety and Validation
- Commands are validated for argument count, port range, and port type before
  any state change.
- Switchable-only operations reject PWM ports unless in switchable mode.
- PWM level updates are only accepted in mode 0 (variable PWM).
- Ambient PWM mode (mode 2) is only accepted when a temperature probe is present.
- Overvoltage blocks power-enabling commands (`O` and non-zero `W`) and triggers
  a safety shutdown of controllable outputs.
- If probes are absent or partially supported, the status string simply omits
  optional fields (pressure only appears if a pressure-capable probe is present).

# Command Protocol
Every command and reply starts with `>` and ends with `#`. Fields are separated
by `:` and mostly compatible with the original BigPowerBox firmware. Note that
the status field order is a backward-incompatible change: **dew point now appears
before the optional pressure value**.

Example commands:
| send | reply |
| --- | --- |
| `>P#` | `>POK#` |
| `>M:01:PortName#` | `>MOK#` |
| `>N:01#` | `>N:01:PortName#` |

## Available commands
| literal | command | response | description |
| --- | --- | --- | --- |
| `P` | Ping | `POK` | Ping the device |
| `D` | Discover | `D:<Name>:<Version>:<Signature>` | Discover capabilities |
| `S` | Status | `S:<statuses>:<currents>:<Ic>:<Iv>[:<t>:<h>:<dew>[:<p>]]` | Status and measurements |
| `N:<dd>` | Get port name | `N:<dd>:<name>` | Return stored port name |
| `M:<dd>:<name>` | Set port name | `MOK` | Store a new port name |
| `O:<dd>` | On | `OOK` | Turn port on (switchable mode only) |
| `F:<dd>` | Off | `FOK` | Turn port off (switchable mode only) |
| `W:<dd>:<level>` | Set PWM level | `WOK` | PWM level 0-255 (mode 0 only) |
| `C:<dd>:<mode>` | Set PWM mode | `COK` | Set port mode (0,1,2) |
| `G:<dd>` | Get PWM mode | `G:<dd>:<mode>` | Query PWM mode |
| `T` | Legacy temp offset | `TOK` | Accepted for compatibility, no action |
| `H:<dd>` | Legacy dew margin | `H:<dd>:<temp>` | Returns whole-degree dew margin |
| `K:<deg>[:<min>[:<max>]]` | Set dew config | `KOK` | Set dew margin on (deg, max 5), optional duty min/max (0-100, min <= max) |
| `R:<scope>` | Reset | `ROK` | `NAMES` resets names to defaults (`Port00`..), `CONF` resets config/ports, `ALL` resets names+config |
| `X:<tempC>:<hum>` | Debug override | `XOK` | When `DEBUG` is enabled: overrides ambient readings (`tempC` can be negative, `hum` must be 0..100), `X::` clears override |
| `J` | Debug MCP dump | `J:<addr>:<probe_ok>:<read_a_ok>:<read_b_ok>:<cached_a>:<cached_b>:<gpio_a>:<gpio_b>` | When `DEBUG` is enabled: dump MCP23017 state and I2C health |
| `L:<0|1>` | Debug OLEN | `LOK` | When `DEBUG` is enabled: set OLEN low/high (0 disables open-load diagnostics) |

# Status Fields
The `S` response includes:
- `<statuses>`: one field per port (0/1 for switchable, 0-255 for PWM)
- `<currents>`: one field per port in amps with 2 decimals
- `<Ic>`: input current in amps with 2 decimals
- `<Iv>`: input voltage in volts with 2 decimals
- Optional when a probe is present: `<t>` (C), `<h>` (%), `<dew>` (C), and optional `<p>` (hPa)

Example (with temp/humidity/dew/pressure):
`>S:0:1:0:1:0:1:0:1:0:128:0:64:1:1:0.00:0.12:0.00:0.00:0.00:0.00:0.00:0.00:0.50:0.75:0.00:0.00:0.00:0.00:1.23:12.40:21.50:45.00:12.30:1013#`

# PWM Mode Behavior
- Mode 0 (variable): `W` sets 0..255, `O/F` is rejected.
- Mode 1 (switchable): `O/F` toggles `digitalWrite(HIGH/LOW)`, `W` is rejected.
- Mode 2 (ambient dew): automatic duty based on ambient dew margin; `W` is rejected.

# Ambient PWM Control
PWM mode 2 enables ambient dew control. When an ambient probe is present, the
firmware automatically adjusts duty based on the dew margin. The duty ramps
between a minimum and maximum as the margin approaches the configured
thresholds, applies hysteresis, and rate-limits changes to avoid sudden jumps.
This mode rejects manual PWM level commands.

# Reliability and Resource Use
This rewrite focuses on stability by keeping memory usage small and avoiding
heap-heavy patterns that can fragment memory on AVR-class devices. The code
path and protocol are intentionally minimal, which reduces flash and RAM usage
and leaves room for future features.

# Pin Compatibility
Pin assignments are copied from the original firmware in
`BigPowerBox/Arduino/BigPowerBox/board.h`. This includes:
- MCP23017 port mapping for ports 1-8
- PWM pins for ports 9-12
- Utility pins (DSEL, MUX0-2, OLEN)
- Analog inputs (ISIN, VSIN, ISOUT)

# File Layout
```
BigPowerBoxFirmware/
  BigPowerBoxFirmware.ino
  board_config.h
  serial_framing.{h,cpp}
  protocol.{h,cpp}
  protocol_handlers.{h,cpp}
  protocol_format.{h,cpp}
  ports.{h,cpp}
  i2c_bus.{h,cpp}
  mcp23017.{h,cpp}
```

# Build / Upload
You can build and upload using the Arduino IDE
(https://www.arduino.cc/en/software) or `arduino-cli`
(https://github.com/arduino/arduino-cli).

Example (Nano ATmega328P, adjust as needed):
```
arduino-cli compile \
  --fqbn arduino:avr:nano \
  /home/jrhuerta/Code/PowerBoxFirmware/BigPowerBoxFirmware

arduino-cli upload \
  --fqbn arduino:avr:nano \
  -p /dev/ttyUSB0 \
  ./BigPowerBoxFirmware
```
