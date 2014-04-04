
#define LCD_REG_CMD 0
#define LCD_REG_KEY_LO 1
#define LCD_REG_KEY_HI 2

#define LCD_CMD_CURSOR_OFF 4
#define LCD_CMD_CURSOR_ON 5
#define LCD_CMD_CLEAR_SCR 12
#define LCD_CMD_BACKLIGHT_ON 19
#define LCD_CMD_BACKLIGHT_OFF 20
#define LCD_CMD_CURSOR_LOC 3
#define LCD_CMD_CR 13

#define I2C_LCD_ADDR (0xC6 >> 1)

struct window {
  byte line;
  byte offset;
  byte length;
  byte start;
  char *text;
}; 

extern struct window scrollWindow;

char decodeKey(int code);
void cls();
void cursorOn();
void cursorOff();
void cmd(byte code);
char readKey();
void at(byte x,byte y);
void writeAt(byte x,byte y, const char *buf);
void writeAt_P(byte x,byte y,PGM_P buf);
void fmt2Digits(char *buf,int num);
void fmt2XDigits(char *buf,int num);
void scroll();
void write(const char *buf);
void write_P(PGM_P buf);
