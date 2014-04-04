struct hilo {
  byte lo;
  byte hi;
  float target;
};

struct viv {
  char name[8];
  byte flags;
  byte relay_pin;
  uint8_t sensor_addr[8];
  struct hilo temp;
};

#define VER 7

struct herp_header {
  char welcome[16];
  byte ver;
  byte flags;
  byte unused;
  byte viv_count;
  struct viv vivs[8];
};

enum menu_state_t {
  no_menu,
  main_menu,
  set_time,
  manual_ctrl,
  setup_menu,
  viv_menu,
  assign,
  set_temp,
  viv_edit_menu,
  viv_sensor,
  pin_edit,
  edit_name
};

#define RADIX_HEX 16
#define RADIX_DEC 10

#define SOUNDER_PIN 8

extern struct herp_header herp;

// static_assert(sizeof(struct herp_header) > 1024, "herp structure too big for EEPROM");

enum edit_temp_type {
  edit_lo,
  edit_target,
  edit_hi
};

extern boolean silence;
extern int sensorCount;

struct sensor {
  byte addr[8];
  float value;
  float lastValue;
  time_t lastWrite;
  byte valid:1;
  byte relay_on:1;
};

// Hysteresis curve +/- 1.0 celsius
#define TEMP_DELTA 1.0

extern time_t boot;
extern struct sensor sensors[4];
extern char displayBuf[17];
float getCTemp(byte *addr);
int scanbus();
void sensorIdToBuffer(int idx,char *buf);
void mainMenu();
void readSensors();
void scrollSensors();
void dumpFile(int fileIdx);
void truncateFile(int fileIdx);
int vivForSensor(int s);
float getTarget(int v);
float getLo(int v);
float getHi(int v);
extern int displaySwitch;
extern char inputBuf[16];
extern int cursor;
extern time_t lastRead;
extern bool didReset;
extern time_t lastKeyTime;
extern bool backlight;

#define TEMP_C 0
#define TEMP_F 1
#define TEMP_FLAG 0
