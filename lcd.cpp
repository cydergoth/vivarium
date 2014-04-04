#include <Arduino.h>
#include <Wire.h>
#include <Time.h>
#include "vivarium.h"
#include "lcd.h"

struct window scrollWindow;

void cmd(byte code) {
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_CMD);
    Wire.write(code);
    Wire.endTransmission();
}

void cls() {
    cmd(LCD_CMD_CLEAR_SCR);
}

void cursorOff() {
    cmd(LCD_CMD_CURSOR_OFF);
}

void cursorOn() {
    cmd(LCD_CMD_CURSOR_ON);
}

char decodeKey(int code)
{
    switch(code) {
    case 1:
        return '1';
    case 2:
        return '2';
    case 4:
        return '3';
    case 8:
        return '4';
    case 16:
        return '5';
    case 32:
        return '6';
    case 64:
        return '7';
    case 128:
        return '8';
    case 256:
        return '9';
    case 512:
        return '*';
    case 1024:
        return '0';
    case 2048:
        return '#';
    default:
        return 0;
    }
}

char readKey() {
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_KEY_LO);
    Wire.endTransmission();
    Wire.requestFrom(I2C_LCD_ADDR,1);
    byte loByte=(byte)Wire.read();
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_KEY_HI);
    Wire.endTransmission();
    Wire.requestFrom(I2C_LCD_ADDR,1);
    byte hiByte=(byte)Wire.read();
    return decodeKey(hiByte << 8 | loByte);
}

void at(byte x,byte y) {
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_CMD);
    Wire.write(LCD_CMD_CURSOR_LOC);
    Wire.write(y);
    Wire.write(x);
    Wire.endTransmission();
}

void writeAt(byte x,byte y,const char *msg) {
    at(x,y);
    write(msg);
}

void writeAt_P(byte x,byte y,PGM_P msg) {
    at(x,y);
    write_P(msg);
}

void write(const char *buf) {
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_CMD);
    Wire.write(buf);
    Wire.endTransmission();
}

void write_P(PGM_P msg) {
    Wire.beginTransmission(I2C_LCD_ADDR);
    Wire.write(LCD_REG_CMD);
    char c;
    while((c=pgm_read_byte(msg++))!=0) {
        Wire.write(c);
    }
    Wire.endTransmission();
}

void fmt2DigitsR(char *buf,int num,int radix) {
    char ibuf[3];
    ibuf[2]=0;
    itoa(num,ibuf,radix);
    if(ibuf[1]==0) {
        ibuf[0]='0';
        itoa(num,ibuf+1,radix);
    }
    memcpy(buf,ibuf,2);
}

void fmt2XDigits(char *buf,int num) {
    fmt2DigitsR(buf,num,RADIX_HEX);
}

void fmt2Digits(char *buf,int num) {
    fmt2DigitsR(buf,num,RADIX_DEC);
}

int scrollCountDown=0;

void scroll() {
    if(scrollCountDown-->0) return;
    char visible[scrollWindow.length+1];
    memset(visible,' ',scrollWindow.length);
    visible[scrollWindow.length]=0;
    int textLen=strlen(scrollWindow.text);
    int textLeft=textLen-scrollWindow.start;
    memcpy(visible,&scrollWindow.text[scrollWindow.start],min(textLeft,scrollWindow.length));
    if(textLeft<scrollWindow.length-1) {
        memcpy(&visible[textLeft+1],scrollWindow.text,scrollWindow.length-textLeft-1);
    }
    writeAt(scrollWindow.offset,scrollWindow.line,visible);
    scrollWindow.start++;
    if(scrollWindow.start==textLen) {
        scrollWindow.start=0;
    }
    scrollCountDown=6;
}
