#include <Arduino.h>
#include <Fat16.h>
#include <Time.h>
#include "vivarium.h"
#include "filestore.h"
#include "vivtime.h"
#include "lcd.h"

SdCard card;
Fat16 file;

void initSD() {
    if(card.init() && Fat16::init(&card)) {
    sdPresent=true;
  }
}

// Sensor ID's are 16 characters in Hex.
// We just use a sub-set, skipping the first two characters
// This will change when we get the persistent registration of the
// sensor Ids implemented
bool selectSensorFile(char *sensorIdStr) {
  memcpy(displayBuf,sensorIdStr+2,8);
  strcpy_P(displayBuf+8,PSTR(".dat"));
  displayBuf[12]=0;

  if(file.open(displayBuf,O_RDWR | O_CREAT | O_APPEND)==0) return false;
}

void truncateFile(int idx) {
  char sensorId[17];
  sensorIdToBuffer(idx,sensorId);
  if(!selectSensorFile(sensorId)) return;
  file.truncate(0);
  file.close();
}

void dumpFile(int idx) {
  char sensorId[17];
  sensorIdToBuffer(idx,sensorId);
  if(!selectSensorFile(sensorId)) return;
  file.rewind();
  int d;
  while((d=file.read())!=-1) {
    Serial.write((byte)d);
  }
  Serial.println("");
  file.close();
}

void logData(int idx,int viv,float target) {
  if(!sdPresent || timeStatus()==timeNotSet) return;
  time_t current=now();
  if(sensors[idx].valid==0 || (sensors[idx].lastWrite!=0 && (current-sensors[idx].lastWrite)<60)) return;
  sensors[idx].lastWrite=current;
  char sensorId[17];
  sensorIdToBuffer(idx,sensorId);
  if(!selectSensorFile(sensorId)) return;
  dtostrf(sensors[idx].value,3,1,displayBuf);
  displayBuf[5]=0;
  file.write(displayBuf);
  file.write(',');
  fmt2Digits(displayBuf,viv);
  displayBuf[2]=0;
  file.write(displayBuf);
  file.write(',');
  dtostrf(target,3,1,displayBuf);
  displayBuf[5]=0;
  file.write(displayBuf);
  file.write(',');
  file.write(sensors[idx].relay_on+'0');
  file.write(',');
  displayBuf[14]=0;
  time_t tval=lastRead;
  fmtDateTime(displayBuf,tval);
  file.write(displayBuf);
  file.write("\n");
  file.sync();
  file.close();
}
