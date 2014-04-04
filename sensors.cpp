#include <OneWire.h> 
#include <Time.h>
#include <Wire.h>
#include "vivarium.h"
#include "lcd.h"
#include "filestore.h"
#include "pitches.h"


const int DS18S20_Pin = 2; //DS18S20 Signal pin on digital 2

// Temperature chip i/o
OneWire ds(DS18S20_Pin);  // on digital pin 2

struct sensor sensors[4];
int sensorViewBase=0;
int displaySwitch=0;

void sensorIdToBuffer(const int idx,char *buf) {
  for(int i=0;i<8;i++) {
    fmt2XDigits(buf,sensors[idx].addr[i]);
    buf+=2;
  }
  buf[0]=0;
}


void showSensor(int sensor) {
  if(sensor>=sensorCount) return;
  char buf[21];
  fmt2Digits(buf,sensor+1);
  buf[2]=':';
  buf[3]=' ';
  sensorIdToBuffer(sensor,buf+4);
  buf[20]=0;
  write(buf); 
}

// Scroll the sensors up
void scrollSensors() {
  if(sensorCount<0) {
    writeAt_P(1,2,PSTR("NO SENSORS YET"));
  } 
  else {
    at(1,2);
    showSensor(sensorViewBase);
    if(sensorCount>sensorViewBase) {
      at(1,3);
      showSensor(sensorViewBase+1);
    }
    sensorViewBase++;
    if(sensorViewBase==sensorCount-1) {
      sensorViewBase=0;
    }
  }
}

float getCTemp(byte *addr) {
  //returns the temperature from one DS18S20 in DEG Celsius
  byte data[12];
  float temperatureSum=0.0;

  ds.reset();
  ds.select(addr);
  ds.write(0x44,0); // start conversion, without parasite power on at the end

  byte present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); // Read Scratchpad

  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  // ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  temperatureSum = tempRead / 16;
  return temperatureSum;
}

int scanbus() {
  int sensorCount=0;
  ds.reset_search();
  while ( sensorCount<4 && ds.search(sensors[sensorCount].addr)) {

    if ( OneWire::crc8( sensors[sensorCount].addr, 7) != sensors[sensorCount].addr[7]) {
      Serial.println("CRC!");
      continue;
    }

    if ( sensors[sensorCount].addr[0] != 0x10 && sensors[sensorCount].addr[0] != 0x28) {
      Serial.print("Device! ");
      continue;
    }

    sensorCount++;
  }
  return sensorCount;
}

#define ALARM_NOTES 1
int alarmBase=0;
int alarm[] = {
  NOTE_G3, NOTE_A3 };

void readSensors() {

  // Find sensors. Poll each time until we successfully get some
  // TODO: Periodically scan for sensors and report any removed/new sensors
  if(sensorCount==-1) {  
    int sc=scanbus();
    if(sc>0) sensorCount=sc;
    else return;
  }

  for(int i=0;i<sensorCount;i++) {    
    float temperature = getCTemp(sensors[i].addr);
    sensors[i].value=temperature;
    if(temperature<0 || temperature>100) {
      sensors[i].valid=0;
      continue;
    }
    //    if(sensors[i].lastValue>0 && abs(sensors[i].lastValue-sensors[i].value)>5.0) {
    //       sensors[i].valid=0;
    //       sensors[i].lastValue=sensors[i].value;
    //       continue;
    //    }
    sensors[i].lastValue=sensors[i].value;
    sensors[i].valid=1;   
    int v=vivForSensor(i);
    float target=0;

    if(v!=-1 && now()-boot>3*60) { // 3*60 is to give the sensors time to settle
      target=getTarget(v);
      if(herp.vivs[v].relay_pin!=0) {
        if(sensors[i].value<(herp.vivs[v].temp.target-TEMP_DELTA)) {
          digitalWrite(herp.vivs[v].relay_pin,1);         
          sensors[i].relay_on=1;
        } 
        else if(sensors[i].value>(herp.vivs[v].temp.target+TEMP_DELTA)) {
          digitalWrite(herp.vivs[v].relay_pin,0);        
          sensors[i].relay_on=0;
        }
      }
      float hi=getHi(v);
      float lo=getLo(v);
      if(!silence && (sensors[i].value<lo || sensors[i].value>hi)) {
        tone(SOUNDER_PIN, alarm[alarmBase++],1000/4);
        if(alarmBase>ALARM_NOTES) alarmBase=0;
      } else {
        noTone(SOUNDER_PIN);
      }
    }
    logData(i,v,target);
  }
}


