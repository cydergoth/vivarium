#include <Arduino.h>
#include <EEPROM.h>
#include "controller.h"
#include "settings.h"

// All internal temperatures are stored as Celcius as that is what the sensors report natively
void clearSettings() {
  memset(&herp, 0, sizeof(herp));
  herp.ver = VER;
#ifdef USE_NAMES
  memcpy_P(&herp.power[0].name, PSTR("SKT #1"), 6);
#endif
  writeEeprom();
  sys_state.didReset = true;
}

void readEeprom() {
  if (EEPROM.read(0) != 'c' ||
      EEPROM.read(1) != 't' ||
      EEPROM.read(2) != 'r' ||
      EEPROM.read(3) != 'l' || EEPROM.read(4) != VER) {
    clearSettings();
    return;
  }
  // Header matched
  byte *ptr = (byte*)&herp;
  for (int i = 0; i < sizeof(herp); i++) {
    *ptr++ = EEPROM.read(i + 4);
  }
  sys_state.readHeader = 1;
}

void writeEeprom() {
  EEPROM.write(0, 'c');
  EEPROM.write(1, 't');
  EEPROM.write(2, 'r');
  EEPROM.write(3, 'l');
  byte *ptr = (byte*)&herp;
  for (int i = 0; i < sizeof(herp); i++) {
    EEPROM.write(i + 4, *ptr++);
  }
  sys_state.wroteHeader = 1;
}


