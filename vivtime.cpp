#include <Arduino.h>
#include <Time.h>
#include "vivarium.h"
#include "lcd.h"
#include "vivtime.h"

#ifdef LOCAL_DISPLAY
void fmtTime(char *buf) {
  fmtTime(buf,now());
}

void fmtTime(char *buf,time_t current) {
    fmt2Digits(buf,hour(current));
    buf[2]=':';
    fmt2Digits(buf+3,minute(current));
    buf[5]=':';
    fmt2Digits(buf+6,second(current));
}

void fmtDateTime(char *buf) {
  fmtDateTime(buf,now());
}

void fmtDateTime(char *buf,time_t current) {
    fmt2Digits(buf,month(current));
    buf[2]='/';
    fmt2Digits(buf+3,day(current));
    buf[5]='/';
    fmt2Digits(buf+6,year(current) % 100);
    buf[8]=' ';
    fmtTime(buf+9,current);
    buf[14]=0;
}

void editTime(char key) {
    switch(key) {
    case '*':
        parseDateTime(inputBuf,true);
        cursorOff();
        mainMenu();
        return;
    case '#':
        {
            //   inputBuf[cursor]='0';
            cursor--;
            if(cursor<0) cursor=0;
            if(inputBuf[cursor]==':' || inputBuf[cursor]==' ' || inputBuf[cursor]=='/') cursor--;
            break;
        }
    default:
        {
            inputBuf[cursor]=key;
            cursor++;
            if(inputBuf[cursor]==':' || inputBuf[cursor]==' ' || inputBuf[cursor]=='/') cursor++;
            if(cursor>13) cursor=13;
        }
    }
    writeAt(1,2,inputBuf);
    if(!parseDateTime(inputBuf,false)) {
        writeAt_P(1,3,PSTR("Not Valid"));
    }
    else {
        writeAt_P(1,3,PSTR("Time Ok  "));
    }
    at(1+cursor,2);
}


bool parseDateTime(char *buf,bool set) {
    time_t current=now();
    int cmonth=month(current);
    int cday=day(current);
    int cyear=year(current);
    char ibuf[3];
    ibuf[2]=0;

    if(buf[2]=='/') {
        ibuf[0]=buf[0];
        ibuf[1]=buf[1];
        cmonth=atoi(ibuf);
        ibuf[0]=buf[3];
        ibuf[1]=buf[4];
        cday=atoi(ibuf);
        ibuf[0]=buf[6];
        ibuf[1]=buf[7];
        cyear=atoi(ibuf)+2000;
        buf+=9;
    }

    int hrs=0;
    int mins=0;
    int secs=0;
    ibuf[0]=buf[0];
    ibuf[1]=buf[1];
    hrs=atoi(ibuf);
    ibuf[0]=buf[3];
    ibuf[1]=buf[4];
    mins=atoi(ibuf);
    if(hrs>0 && hrs<24 && mins>-1 && mins<60 && cday>0 && cday<32 && cmonth>0 && cmonth<13 && cyear>0 && cyear<2099) {
        if(set) {          
            setTime(hrs,mins,secs,cday,cmonth,cyear);
            boot=now();
            lastKeyTime=boot;
            lastRead=lastKeyTime;
        }
        return true;
    }
    else {
        return false;
    }

}

#endif
