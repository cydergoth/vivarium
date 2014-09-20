
#include <Wire.h>
#include <OneWire.h>
#include <Time.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#ifdef LOGGER
#include <Fat16.h>
#endif
#include "vivarium.h"
#include "lcd.h"
#include "vivtime.h"
#include "settings.h"
#ifdef LOGGER
#include "filestore.h"
#endif
#include "pitches.h"p
#include <avr/wdt.h>
#ifdef BRIDGE
#include <String.h>
#include <Mailbox.h>
#endif

#ifdef LOCAL_DISPLAY

// General purpose input buffer
char inputBuf[16];

// Cursor into intput buffer
int cursor=-1;

// Index of currently selected viv for UI menus
int vivIndex=0;

// Currently active key or 0 if none
char key;

// Last active key
char lastKey=0;

// Time of the last recognised keypress
time_t lastKeyTime=0;

// Is the currently pressed key the same as the previously polled key
bool sameKey=false;

// Is the backlight on/
bool backlight=true;

// Index into the spinner character array
byte spinner=0;

menu_state_t menuState=no_menu;

#endif

// General purpose display buffer
char displayBuf[17];

// Total number of discovered connected sensors
int sensorCount=-1;

// Time the last sensor read attempt was made
time_t lastRead=0;

// Did reading the EEPROM cause a reset?
bool didReset=false;

#ifdef LOGGER

// Is an SD Card present
bool sdPresent=false;

#endif

// Time of the boot or when the clock was set
time_t boot;

boolean silence=false;

struct herp_header herp;

void dot() {
#ifdef LOCAL_DISPLAY
    write_P(PSTR("."));
#endif  
}

void setup() {
#ifdef BRIDGE
    Bridge.begin();
    Mailbox.begin();
#endif

    Wire.begin();    

#ifdef LOCAL_DISPLAY    
    cmd(LCD_CMD_BACKLIGHT_ON);
    cls();
    cursorOff();
    dot();
#endif

    readEeprom();
    dot();

    memset(&sensors,0,sizeof(sensors));
#ifdef LOCAL_DISPLAY    
    memset(&scrollWindow,0,sizeof(scrollWindow));
    scrollWindow.line=0;
    dot();
#endif

#ifdef SERIAL
    Serial.begin(9600);
    dot();
#endif

    // Init relays
    for(int i=0;i<herp.viv_count;i++) {
        byte pin=herp.vivs[i].relay_pin;
        if(pin>0) {
            pinMode(pin,OUTPUT);
        }
    }
    pinMode(SOUNDER_PIN,OUTPUT);    
    dot();

#ifdef LOGGER
    initSD();
    dot();
#endif

    sensorCount=scanbus();
    dot();

#ifdef LOCAL_DISPLAY
    noMenu();
#endif

#ifdef SERIAL
    Serial.println(F("H"));
#else 
    Mailbox.writeMessage("H");
#endif
}

#ifdef LOCAL_DISPLAY
void writeTemp(float temp) {
    dtostrf(temp,3,1,displayBuf);
    displayBuf[5]=0;
    write(displayBuf);
}

void sensorSpinner() {
    at(20,3);
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_CMD);
    Wire.write("|/-\\"[spinner]);
    Wire.endTransmission();
    spinner++;
    if(spinner==4) spinner=0;
}

void mainMenu() {
    menuState=main_menu;
    cls();
    writeAt_P(1 ,1,PSTR("MENU"));
    writeAt_P(1 ,2,PSTR("1 Time"));
    writeAt_P(1 ,3,PSTR("2 Setup"));
    writeAt_P(10,2,PSTR("3 Vivs"));
    showBack();
}


void display() {

    switch(menuState) {
    case set_time:
    case set_temp:
    case setup_menu:
    case viv_menu:
    case pin_edit:
        break;
    case assign:
        {
            if(displaySwitch>0) {
                break;
            }
            scrollSensors();
        }
        break;
    case no_menu:
        timeAt(13,1);
        normalDisplay();
        break;
        //case main_menu: mainMenu(); break;
    }
    if(scrollWindow.line!=0) scroll();
    displaySwitch--;
    if(displaySwitch<0) displaySwitch=6;
}

void timeAt(int x,int y) {
    switch(timeStatus()) {
    case timeNotSet:
        writeAt_P(x,y,PSTR("TIME"));
        break;
    case timeSet:
        fmtTime(displayBuf);
        displayBuf[8]=0;
        writeAt(x,y,displayBuf);
        break;
    case timeNeedsSync:
        writeAt_P(x,y,PSTR("SYNC"));
        break;
    }
}

int mainDispBase=0;
void normalDisplay() {
    at(1,2);

    for(int i=0;i<2;i++) {
        if(mainDispBase+i<sensorCount) {
            displayTemp(mainDispBase+i);
        }
        if(displaySwitch==0) {
            mainDispBase++;
            if(mainDispBase>=sensorCount) mainDispBase=0;
        }
    }

    // Display keys for debugging
    //char buf[2];
    //buf[0]=key>0?key:' ';
    //buf[1]=0;
    //writeAt(19,2,buf);
    //buf[0]=lastKey;
    //writeAt(19,3,buf);

}

#endif

float cToMaybeF(float temperature) {
    if(herp.flags & (1<<TEMP_FLAG)) {
        return (temperature*9.0)/5.0+32;
    }
    else {
        return temperature;
    }
}

float maybeFToC(float temperature) {
    if(herp.flags & (1<<TEMP_FLAG)) {
        return ((temperature-32.0)*5.0)/9.0;
    }
    else {
        return temperature;
    }
}

#ifdef LOCAL_DISPLAY

void displayTemp(int i) {
    if(!sensors[i].valid) {
        write_P(PSTR("ERR"));
        cmd(LCD_CMD_CR);
        return;
    }
    if(! sensors[i].relay_on) {
        write_P(PSTR("  "));
    }
    else {
        write_P(PSTR("! "));
    }
    float temperature=sensors[i].value;
    float temp=cToMaybeF(temperature);
    writeTemp(temp);
    write_P(PSTR(" "));
    int v=vivForSensor(i);
    if(v>-1) {
        strncpy(displayBuf,herp.vivs[v].name,8);
        displayBuf[8]=0;
        write(displayBuf);
    }
    cmd(LCD_CMD_CR);
}

// If a temperature is being edited, this is it
// It is in the display units
float editingTemp;
enum edit_temp_type temp_type;

int digitCursor=0;

void displayAlphaNum() {
  at(1,2);
  for(int i=0;i<20;i++) {
    cmd((byte)('A'+i));
  }
  at(1,3);
  for(int i=20;i<26;i++) {
    cmd((byte)('A'+i));
  }
  cmd('s');
  at(1,4);
  for(int i=0;i<10;i++) {
    cmd((byte)('0'+i));
  }  
}

void editName() {
  menuState=edit_name;
  cls();

  vivHeader(vivIndex);
  memset(inputBuf,0,9);  
  displayAlphaNum();
  cursor=0;
  setChar(0);
  at(1,2);
  cursorOn();
}

void showDigitCursor() { 
  if(digitCursor<27) {
  int col=digitCursor % 20;
  int line=digitCursor / 20;
  at(col+1,line+2);
  } else {
    at(1+digitCursor-27,4);
  }
}

void setChar(char code) {
  inputBuf[cursor++]=code;
  if(code==0) cursor=0;
  if(cursor>7) cursor=7;
  writeAt(6,1,inputBuf);
  int len=strlen(inputBuf);
  for(int i=len;i<8;i++) cmd('.');
  showDigitCursor();
}

void setName(char *buf) {
  strncpy(herp.vivs[vivIndex].name,buf,8);
}

void handleNameKey(char key) {
  switch(key) {
    case '*':  
        setName(inputBuf);
        cursorOff();
        updateEeprom();
        vivEditMenu();
        return;
   case '#': {
     inputBuf[cursor]=0;
     cursor--;
     if(cursor<0) cursor=0;
     break;
   }
   case '5': {
     if(digitCursor<26) {
       setChar((char)('A'+digitCursor));
       return;
     }  
     if(digitCursor==26) { setChar(' '); return; }
     if(digitCursor>26) { setChar((char)('0'+digitCursor-27)); return; }
   }
   case '6': { digitCursor++; if(digitCursor>36) digitCursor=0; showDigitCursor(); break;}
   case '4': { digitCursor--; if(digitCursor<0) digitCursor=36; showDigitCursor(); break; }
}
}

void editTemp(char key) {
    float lo=0;
    float hi=0;
    switch(key) {
    case '*':
        editingTemp=maybeFToC(parseTemp(inputBuf));
        switch(temp_type) {
        case edit_target:
            {
                lo=getLo(vivIndex);
                hi=getHi(vivIndex);
                setTarget(vivIndex,editingTemp);
                setLo(vivIndex,lo);
                setHi(vivIndex,hi);
                break;
            }
        case edit_lo:
            {
                setLo(vivIndex,editingTemp);
                break;
            }
        case edit_hi:
            {
                setHi(vivIndex,editingTemp);
                break;
            }
        }
        cursorOff();
        updateEeprom();
        vivEditMenu();
        return;
    case '#':
        {
            cursor--;
            if(cursor<0) cursor=0;
            if(inputBuf[cursor]=='.') cursor--;
            break;
        }
    default:
        {
            inputBuf[cursor]=key;
            cursor++;
            if(inputBuf[cursor]=='.') cursor++;
            if(cursor>3) cursor=3;
        }
    }
    //writeAt(1,2,inputBuf);
    float temp=parseTemp(inputBuf);
    dtostrf(temp,3,1,inputBuf);
    writeAt(1,2,inputBuf);
    at(1+cursor,2);
}

void handlePinEditKey(char key) {
    switch(key) {
    case '*':
        {
            byte pin=(byte)atoi(inputBuf);
            cursorOff();
            if(pin!=herp.vivs[vivIndex].relay_pin) {
                if(pin==0 || pin>2 && pin<10) {
                    digitalWrite(herp.vivs[vivIndex].relay_pin,0);
                    pinMode(herp.vivs[vivIndex].relay_pin,INPUT);
                    herp.vivs[vivIndex].relay_pin=pin;
                    pinMode(pin,OUTPUT);
                    updateEeprom();
                }
            }

            vivEditMenu();
            return;
        }
    case '#':
        {
            cursor--;
            if(cursor<0) cursor=0;
            //if(inputBuf[cursor]=='.') cursor--;
            break;
        }
    default:
        {
            inputBuf[cursor]=key;
            cursor++;
            //if(inputBuf[cursor]=='.') cursor++;
            if(cursor>1) cursor=1;
        }
    }
    //writeAt(1,2,inputBuf);
    //float temp=parseTemp(inputBuf);
    //dtostrf(temp,3,1,inputBuf);
    writeAt(1,3,inputBuf);
    at(1+cursor,3);
}

float parseTemp(char *buf) {
    float temp=(buf[0]-'0')*10+(buf[1]-'0')+(((float)(buf[3]-'0'))/10);
    return temp;
}

void vivHeader(int v) {
    fmt2Digits(displayBuf,v+1);
    displayBuf[2]=0;
    writeAt_P(1,1,PSTR("#   :"));
    writeAt(2,1,displayBuf);
    strncpy(displayBuf,herp.vivs[v].name,8);
    displayBuf[8]=0;
    writeAt(6,1,displayBuf);
}

void vivEditMenu() {
    menuState=viv_edit_menu;
    cls();
    vivHeader(vivIndex);
    writeAt_P(16,1,PSTR("0 Name"));
    writeAt_P(1,2,PSTR("1 Sensor"));
    writeAt_P(1,3,PSTR("2 Low"));
    writeAt_P(10,2,PSTR("3 Target"));
    writeAt_P(10,3,PSTR("4 High"));
    showBack();
    writeAt_P(8,4,PSTR("#=Pin"));
}

void tempEntry(enum edit_temp_type type) {
    menuState=set_temp;
    temp_type=type;
    cls();
    writeAt_P(1,1,PSTR("Set Temp ( )"));
    if(herp.flags & 1<<TEMP_FLAG) {
        writeAt_P(11,1,PSTR("F"));
    }
    else {
        writeAt_P(11,1,PSTR("C"));
    }
    showBack();
    writeAt_P(8,4,PSTR("#=Left"));
    memset(inputBuf,0,sizeof(inputBuf));
    switch(temp_type) {
    case edit_lo:
        editingTemp=getLo(vivIndex);
        break;
    case edit_target:
        editingTemp=getTarget(vivIndex);
        break;
    case edit_hi:
        editingTemp=getHi(vivIndex);
        break;
    }
    editingTemp=cToMaybeF(editingTemp);
    dtostrf(editingTemp,3,1,inputBuf);
    inputBuf[5]=0;
    writeAt(1,2,inputBuf);
    at(1,2);
    cursorOn();
    cursor=0;
}

void timeEntry() {
    menuState=set_time;
    cls();
    writeAt_P(1,1,PSTR("Set Time (24 Hr)"));
    showBack();
    writeAt_P(8,4,PSTR("#=Left"));

    fmtDateTime(inputBuf);
    inputBuf[14]=0;
    writeAt(1,2,inputBuf);
    at(1,2);
    cursorOn();
    cursor=0;
}

#endif

void setLo(int v,float lo) {
    if(lo>getTarget(v)) lo=getTarget(v);
    herp.vivs[v].temp.lo=(byte)((herp.vivs[v].temp.target-lo)*10);
}

void setHi(int v,float hi) {
    if(hi<getTarget(v)) hi=getTarget(v);
    herp.vivs[v].temp.hi=(byte)((hi-herp.vivs[v].temp.target)*10);
}

float getLo(int v) {
    return herp.vivs[v].temp.target-((float)herp.vivs[v].temp.lo)/10;
}

float getHi(int v) {
    return herp.vivs[v].temp.target+((float)herp.vivs[v].temp.hi)/10;
}

float getTarget(int v) {
    return herp.vivs[v].temp.target;
}

void setTarget(int v,float target) {
    herp.vivs[v].temp.target=target;
}

int vivForSensor(int s) {
    for(int i=0;i<herp.viv_count;i++) {
        if(memcmp(sensors[s].addr,herp.vivs[i].sensor_addr,8)==0) {
            return i;
        }
    }
    return -1;
}

int sensorForViv(int v) {
    for(int i=0;i<sensorCount;i++) {
        if(memcmp(sensors[i].addr,herp.vivs[v].sensor_addr,8)==0) {
            return i;
        }
    }
    return -1;
}

#ifdef LOCAL_DISPLAY

void displayViv(int v) {
    vivHeader(v);

    float lo=getLo(v);
    at(1,3);
    writeTemp(cToMaybeF(lo));

    at(7,3);
    writeTemp(cToMaybeF(herp.vivs[v].temp.target));

    float hi=getHi(v);
    at(13,3);
    writeTemp(cToMaybeF(hi));

    fmt2Digits(displayBuf,herp.vivs[v].relay_pin);
    displayBuf[2]=0;
    writeAt(18,3,displayBuf);

    writeAt_P(1,2,PSTR("Low  Target High Pin"));
}

char *scrollbuf=0;

void vivMenu() {
    menuState=viv_menu;
    cls();
    displayViv(vivIndex);
    scrollWindow.line=4;
    scrollWindow.offset=1;
    scrollWindow.length=20;
    scrollWindow.start=0;
    if(scrollbuf==0) scrollbuf=(char *)malloc(61);
    int c=0;
    memcpy_P(scrollbuf,PSTR("*=Back 1=Edit "),14);
    c=14;
    if(herp.viv_count<8) {
        memcpy_P(scrollbuf+c,PSTR("2=Add "),6);
        c+=6;
    }
    if(vivIndex!=0) {
        memcpy_P(scrollbuf+c,PSTR("3=Delete "),9);
        c+=9;
    }
    if(vivIndex<herp.viv_count-1) {
        memcpy_P(scrollbuf+c,PSTR("4=Previous "),11);
        c+=11;
    }
    if(vivIndex>0) {
        memcpy_P(scrollbuf+c,PSTR("6=Next "),7);
        c+=7;
    }
    scrollbuf[c]=0;
    scrollWindow.text=scrollbuf;
}

inline void showBack() {
    writeAt_P(1,4,PSTR("*=Back"));
}

void assignSensors() {
    menuState=assign;
    cls();
    writeAt_P(1,1,PSTR("View Sensors"));
    showBack();
}

void setupMenu() {
    menuState=setup_menu;
    cls();
    writeAt_P(1,1,PSTR("Setup (Ver   )"));
    displayBuf[2]=0;
    fmt2Digits(displayBuf,herp.ver);
    writeAt(12,1,displayBuf);
    if(sdPresent) {
        writeAt_P(16,1,PSTR("SD"));
    }
    if(didReset) {
        writeAt_P(19,1,PSTR("RS"));
    }
    writeAt_P(1,2,PSTR("1 Sensors"));
    // writeAt_P(1,3,PSTR("Free: "));
    //  itoa(FreeRam(),displayBuf);
    //  write(displayBuf);
    writeAt_P(13,2,PSTR("3 C/F"));
    if(herp.flags & (1<<TEMP_FLAG)) writeAt(19,2,"F");
    else writeAt(19,2,"C");
    showBack();
}

void noMenu() {
    menuState=no_menu;
    cls();
    strncpy(displayBuf,herp.welcome,16);
    displayBuf[16]=0;
    writeAt(1,1,displayBuf);
    //if(didReset) writeAt(19,1,"!");
    writeAt_P(1,4,PSTR("*=Menu"));
}

void assignVivSensor() {
    menuState=viv_sensor;
    cls();
    vivHeader(vivIndex);
    int sensor=sensorForViv(vivIndex)+1;
    writeAt_P(1,2,PSTR("Currently"));
    if(sensor==0) {
        writeAt_P(11,2,PSTR("None"));
    }
    else {
        displayBuf[0]=sensor+'0';
        displayBuf[1]=0;
        writeAt(11,2,displayBuf);
    }
    writeAt_P(1,3,PSTR("Press Sensor Num"));
    showBack();
}

#endif

void updateEeprom() {
#ifdef LOCAL_DISPLAY  
    cls();
    writeAt_P(6,2,PSTR("Saving..."));
#endif
    writeEeprom();
}

#ifdef LOCAL_DISPLAY
void addViv() {
    scrollWindow.line=0;
    // Fixme: viv index naming on add after delete
    int v=herp.viv_count;
    memset(&herp.vivs[v],0,sizeof(struct viv));
    strcpy_P(herp.vivs[v].name,PSTR("VIV #"));
    herp.vivs[v].name[5]='0'+v+1;
    herp.vivs[v].temp=herp.vivs[v-1].temp;
    herp.viv_count++;
    updateEeprom();
    vivIndex=herp.viv_count-1;
    vivMenu();
}

void deleteViv() {
    scrollWindow.line=0;
    int v=herp.viv_count;
    if(vivIndex<v-1 && vivIndex>0) {
        memcpy(&herp.vivs[vivIndex],&herp.vivs[vivIndex+1],sizeof(struct viv)*(v-vivIndex));
    }
    herp.viv_count--;
    vivIndex=0;
    updateEeprom();
}

void pinEdit() {
    menuState=pin_edit;
    cls();
    vivHeader(vivIndex);
    writeAt_P(1,2,PSTR("Edit Relay Pin"));
    fmt2Digits(inputBuf,herp.vivs[vivIndex].relay_pin);
    inputBuf[2]=0;
    writeAt(1,3,inputBuf);
    cursor=0;
    showBack();
}
#endif
#ifdef SERIAL
void dumpSensorsToSerial() {
    for(int i=0;i<sensorCount;i++) {
        Serial.print(i);
        Serial.print(',');
        sensorIdToBuffer(i,displayBuf);
        displayBuf[16]=0;
        Serial.print(displayBuf);
        Serial.print(',');
        Serial.print(sensors[i].value);
        Serial.print(',');
        Serial.println(sensors[i].relay_on);
    }
    Serial.flush();
}
#endif

#ifdef BRIDGE

void dumpSensorsToBridge() {
    Mailbox.writeMessage("S");
    for(int i=0;i<sensorCount;i++) {
        String sensor;
        sensor+=(i);
        sensor+=(',');
        sensorIdToBuffer(i,displayBuf);
        displayBuf[16]=0;
        sensor+=displayBuf;
        sensor+=',';
        sensor+=sensors[i].value;
        sensor+=',';
        sensor+=sensors[i].relay_on;
        Mailbox.writeMessage(sensor);
    }
    Mailbox.writeMessage("Z");
}

void handleMessage() {
     String msg;
     Mailbox.readMessage(msg);
     char cmd=msg[0];
     if(cmd=='S') dumpSensorsToBridge();
#ifdef NOT_YET          
        // Set time
        if(cmd=='t') {
            Serial.readBytes(inputBuf,14);
            inputBuf[14]=0;
            parseDateTime(inputBuf,true);
        }
        // Clock
        if(cmd=='C') {
            fmtDateTime(displayBuf,now());
            displayBuf[14]=0;
            Serial.println(displayBuf);
        }
#endif
        // Set vivarium name
        if(cmd=='N') {
           int vivIndex=(msg[1]-'0')*10+(msg[2]-'0');
           strncpy(herp.vivs[vivIndex].name,buf,8);
        }
        // REBOOT!
        if(cmd=='R') {
            wdt_enable (WDTO_2S);  // reset after two second, if no "pat the dog" received
            while(true);
        }
}

#endif

void loop() {
    time_t current=now();

    if(current-lastRead>10) {
        readSensors();
#ifdef LOCAL_DISPLAY        
        if(menuState==no_menu) sensorSpinner();
#endif        
        lastRead=current;
    }

#ifdef LOCAL_DISPLAY
    key=readKey();
    sameKey=key==lastKey;
    lastKey=key;
    if(key!=0 && !sameKey) {
        lastKeyTime=current;
        if(!backlight) {
            cmd(LCD_CMD_BACKLIGHT_ON);
            backlight=true;
            writeAt_P(1,4,PSTR("*=Menu  "));
            return;
        }

        switch(menuState) {
        case set_temp:
            editTemp(key);
            break;
        case no_menu:
            {
                if (key=='*') {
                    mainMenu();
                    break;
                }
                if (key=='1') {
                    sensorCount=scanbus();
                    break;
                }
                if (key=='2') {
                    readSensors();
                    lastRead=current;
                    break;
                }
                if(key=='#') {
                    silence=!silence;
                }
            }
            break;
        case main_menu:
            {
                if(key=='*') {
                    noMenu();
                    break;
                }
                if(key=='1') {
                    timeEntry();
                    break;
                }
                if(key=='2') {
                    setupMenu();
                    break;
                }
                if(key=='3') {
                    vivMenu();
                    break;
                }
                break;
            }
        case set_time:
            editTime(key);
            current=lastKeyTime;
            break;
        case setup_menu:
            {
                if(key=='*') {
                    scrollWindow.line=0;
                    noMenu();
                    break;
                };
                if(key=='3') {
                    // Toggle display c/f for main temperature display
                    herp.flags ^= (1<<TEMP_FLAG);
                    setupMenu();
                    writeEeprom();
                    break;
                }
                if(key=='1') {
                    assignSensors();
                    break;
                }
                break;
            }
        case viv_edit_menu:
            {
              switch(key) {
                case '0': { editName(); break; }
                case '1': {
                    assignVivSensor();
                    break;
                }
                case '*': {
                    vivMenu();
                    break;
                }
                case '2': {
                    tempEntry(edit_lo);
                    break;
                }
                case '3': {
                    tempEntry(edit_target);
                    break;
                }
                case '4': {
                    tempEntry(edit_hi);
                    break;
                }
                case '#': {
                    pinEdit();
                    break;
                }
              }
                break;
            }
        case pin_edit:
            handlePinEditKey(key);
            break;
        case viv_menu:
            {
                if(key=='*') {
                    scrollWindow.line=0;
                    noMenu();
                    break;
                };
                if(key=='1') {
                    scrollWindow.line=0;
                    vivEditMenu();
                    break;
                }
                if(key=='2' && herp.viv_count<8) {
                    addViv();
                    break;
                }
                if(key=='6' && vivIndex<herp.viv_count-1) {
                    vivIndex++;
                    vivMenu();
                    break;
                }
                if(key=='4' && vivIndex>0) {
                    vivIndex--;
                    vivMenu();
                    break;
                }
                if(key=='3' && herp.viv_count>1) {
                    deleteViv();
                    vivMenu();
                    break;
                }
                break;
            }
        case edit_name:
        {
          handleNameKey(key); break;
        }
        case assign:
            {
                if(key=='*') {
                    scrollWindow.line=0;
                    setupMenu();
                    break;
                };
                break;
            }
        case viv_sensor:
            {
                if(key=='*') {
                    vivEditMenu();
                    break;
                }
                if(key=='0') {
                    memset(herp.vivs[vivIndex].sensor_addr,0,8);
                    updateEeprom();
                    assignVivSensor();
                    break;
                }
                if(key>'0' && key<('0'+sensorCount+1)) {
                    memcpy(herp.vivs[vivIndex].sensor_addr,sensors[key-'0'-1].addr,8);
                    updateEeprom();
                    assignVivSensor();
                    break;
                }
                break;
            }
        default:
            menuState=no_menu;
            cls();
            break;
        }
    }
#endif

#ifdef BRIDGE
    if(Mailbox.messageAvailable()) {
      handleMessage();
    }
#endif

#ifdef LOCAL_DISPLAY
    display();

    if(menuState!=set_time && backlight && (current - lastKeyTime ) > 5*60) {
        cmd(LCD_CMD_BACKLIGHT_OFF);
        backlight=false;
        writeAt_P(1,4,PSTR("*=Wakeup"));
    }

    // Change response time if backlight is on
    if(!backlight) delay(500);
    else delay(50);
#else
   delay(500);
#endif

}

#ifdef SERIAL
void serialEvent() {
    while(Serial.available()) {
        int cmd=Serial.read();
        if(cmd=='S') dumpSensorsToSerial();
#ifdef LOGGER        
        // List files human readable
        if(cmd=='L') Fat16::ls(LS_DATE | LS_SIZE);
        // list files just names
        if(cmd=='l') Fat16::ls();
        // Dump file TODO: Add time threshold
        if(cmd=='F') {
            int fileIdx=Serial.read()-'0';
            dumpFile(fileIdx);
        }
        // Truncate File
        if(cmd=='T') {
            int fileIdx=Serial.read()-'0';
            truncateFile(fileIdx);
        }
#endif       
        // Set time
        if(cmd=='t') {
            Serial.readBytes(inputBuf,14);
            inputBuf[14]=0;
            parseDateTime(inputBuf,true);
        }
        // Clock
        if(cmd=='C') {
            fmtDateTime(displayBuf,now());
            displayBuf[14]=0;
            Serial.println(displayBuf);
        }
        // change baud rate for serial port
        if(cmd=='B') {
            Serial.flush();
            delay(2);
            Serial.end();
            Serial.begin(115200);
        }
        // REBOOT!
        if(cmd=='R') {
            wdt_enable (WDTO_2S);  // reset after two second, if no "pat the dog" received
            while(true);
        }
    }
#endif
}
