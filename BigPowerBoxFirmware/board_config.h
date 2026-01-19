#pragma once

#include <Arduino.h>

// ---- DEBUG defaults ----
//#define DEBUG 1
#ifdef DEBUG
#define DEBUG_FAKE_TEMP_CENTI 1500
#define DEBUG_FAKE_HUMID_CENTI 5000
#define DEBUG_DEW_LOG_INTERVAL_MS 10000
#define DEBUG_FAKE_PROBE 1
#endif

// ---- Serial / framing ----
#define SERIALPORTSPEED 9600
#define QUEUELENGTH 5
#define MAXCOMMAND 21
#define SOCOMMAND '>'
#define EOCOMMAND '#'
#define NAMELENGTH 16

// ---- FSM timing ----
#define REFRESH 200

// ---- Firmware identity ----
static const char PROGRAM_NAME[] = "BigPowerBox";
static const char PROGRAM_VERSION[] = "013";

// ---- Board signature (must match original hardware) ----
//  m: MCP23017-controlled switch port
//  p: PWM port
//  a: always-on port (not switchable)
//  f: temp + humidity probe
//  g: temp + humidity + pressure probe
#define BOARD_SIGNATURE_BASE "mmmmmmmmppppaa"
static constexpr uint8_t PORT_COUNT = sizeof(BOARD_SIGNATURE_BASE) - 1;
static constexpr uint8_t BOARD_SIGNATURE_MAX_LEN = PORT_COUNT + 2;
extern char g_board_signature[BOARD_SIGNATURE_MAX_LEN];
static constexpr uint8_t PWM_PORT_COUNT = 4;

// ---- PWM modes ----
#define PWM_MODE_VARIABLE 0
#define PWM_MODE_SWITCHABLE 1
#define PWM_MODE_DEW_AMBIENT 2

// ---- MCP23017 pins (GPA0..GPA7) ----
#define PORT1EN 0
#define PORT2EN 1
#define PORT3EN 2
#define PORT4EN 3
#define PORT5EN 4
#define PORT6EN 5
#define PORT7EN 6
#define PORT8EN 7

// ---- PWM pins (Arduino) ----
#define PORT9EN 3
#define PORT10EN 5
#define PORT11EN 6
#define PORT12EN 9

static const uint8_t ports2Pin[12] = {PORT1EN, PORT2EN, PORT3EN, PORT4EN,  PORT5EN,  PORT6EN,
                                      PORT7EN, PORT8EN, PORT9EN, PORT10EN, PORT11EN, PORT12EN};

// ---- Analog inputs (kept for pin compatibility) ----
#define ISIN A2
#define VSIN A0
#define ISOUT A1

// ---- Utility pins (kept for pin compatibility) ----
#define DSEL 4
#define MUX0 10
#define MUX1 11
#define MUX2 12
#define OLEN 2

// ---- MCP23017 address ----
#define MCP23017_ADDR 0x20

// ---- ADC calibration (match original hardware constants) ----
// VCC in millivolts.
#define VCC_MV 4700
// Number of samples in the rolling average for ADC readings.
#define ADC_SMOOTHING_WINDOW 4

// ---- Sensor EMA smoothing (fixed-point alpha, 0..256) ----
#define SENSOR_EMA_ALPHA 32
// Sensor read interval in milliseconds.
#define SENSOR_READ_INTERVAL_MS 1000

// ---- Ambient dew control defaults ----
// Margin thresholds in centi-degC.
#define DEW_M_ON_CENTI 300
#define DEW_M_OFF_CENTI 350
#define DEW_M_FULL_CENTI 50
// Duty cycle bounds in percent.
#define DEW_DUTY_MIN_PCT 20
#define DEW_DUTY_MAX_AUTO_PCT 80
// Slew rate limit per update in percent.
#define DEW_DUTY_SLEW_STEP_PCT 10

// ---- EEPROM config ----
#define EEPROMNAMEBASE 0
#define EEPROMCONFBASE 224
#define CURRENTCONFIGFLAG 99
#define OLDCONFIGFLAG 0

// ---- Voltage shutdown ----
#define MAXINVOLTS 14.7

// Voltage divider resistors for input voltage measurement (ohms).
#define RDIVIN_OHMS 14100
#define RDIVOUT_OHMS 4700
// CC6900-30A current sensor (mV per Amp).
#define KINIS_MV_PER_A 67
// CC6900-10A current sensor (mV per Amp).
#define KOUTIS_MV_PER_A 200
// BTS7008-2EPA sense conversion constants.
#define ROUTIS_OHMS 1126
#define KILIS 5450
