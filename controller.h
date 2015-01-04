#include <Arduino.h>
#include <EEPROM.h>
#include <Time.h>
#define REQUIRESALARMS 0
#define REQUIRESNEW 1
#include <DallasTemperature.h>

struct hilo {
  byte lo; // Offset *10 for high threshold
  byte hi; // Offset *10 for low threshold
  float target;
};
float getTarget(int v);
float getLo(int v);
float getHi(int v);

#define NAME_LENGTH 8

#define CTRL_NONE 0
#define CTRL_ALWAYS_OFF CTRL_NONE
#define CTRL_SENSOR 1
#define CTRL_TIMER 2
#define CTRL_MANUAL 3
#define CTRL_ALWAYS_ON 4
#define CTRL_MAX CTRL_ALWAYS_ON

// Version of the EEPROM layout. Change this if any EEPROM structs below change
#define VER 3
#define MAX_POWER_SKT 8

// Configuration struct which must be serializable to EEPROM
struct power_skt {
#ifdef USE_NAMES
  char name[NAME_LENGTH];
#endif
  byte ctrl_type;
  union {
     struct {
        DeviceAddress addr;
        struct hilo temp;
     } sensor;     
     byte timer_idx;
  } controller;

};

// Struct to record the on and off times for a timer control
struct timer {
  byte on_hour;
  byte on_min;
  byte off_hour;
  byte off_min;
};

#define TIMER_MAX 2
// All fields in this struct will be written to the EEPROM!
struct herp_header {
  byte ver;
  byte unused;
  struct power_skt power[MAX_POWER_SKT];
  struct timer timer[TIMER_MAX];
};

extern struct herp_header herp;

#define RADIX_HEX 16
#define RADIX_DEC 10

struct system_flags {
  byte didReset: 1;
  byte readHeader: 1;
  byte wroteHeader: 1;
  time_t scanTime;
  time_t bootTime;
};

extern system_flags sys_state;
