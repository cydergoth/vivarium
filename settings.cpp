#include <Arduino.h>
#include <EEPROM.h>
#include <Time.h>
#include "vivarium.h"
#include "settings.h"

void clearSettings() {
  memset(&herp,0,sizeof(herp));
  herp.ver=VER;
  memcpy_P(herp.welcome,PSTR("Welcome"),7);
  herp.viv_count=1;
  memcpy_P(&herp.vivs[0].name,PSTR("VIV #1"),6);
  herp.vivs[0].temp.target=28.9;
  herp.vivs[0].temp.hi=(byte)((32.0f-herp.vivs[0].temp.target)*10);
  herp.vivs[0].temp.lo=(byte)abs((26.5f-herp.vivs[0].temp.target)*10);
  herp.vivs[0].relay_pin=3;
  herp.flags|=(TEMP_F << TEMP_FLAG);
  writeEeprom();
  didReset=true;
}

void readEeprom() {
  if(EEPROM.read(0)!='h' || 
    EEPROM.read(1)!='e' ||
    EEPROM.read(2)!='r' ||
    EEPROM.read(3)!='p' || EEPROM.read(20)!=VER) {
    clearSettings();
    return;
  }
  // Header matched
  byte *ptr=(byte*)&herp;
  for(int i=0;i<sizeof(herp);i++) {
    *ptr++=EEPROM.read(i+4);
  }   
}

void writeEeprom() {
  EEPROM.write(0,'h');
  EEPROM.write(1,'e');
  EEPROM.write(2,'r');
  EEPROM.write(3,'p');
  byte *ptr=(byte*)&herp;
  for(int i=0;i<sizeof(herp);i++) {
    EEPROM.write(i+4,*ptr++);
  }   
}


