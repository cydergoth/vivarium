/*
  Vivarium Environment Controller based on
  Arduino Yún Bridge example ( http://arduino.cc/en/Tutorial/Bridge )

  LGPL V3
 */
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <OneWire.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <EEPROM.h>
//#include <avr/wdt.h>

// Settings for DallasTemperature library
#define REQUIRESALARMS 0
#define REQUIRESNEW 0
#include <DallasTemperature.h>

#include "controller.h"
#include "settings.h"
#define ONE_WIRE_BUS 3 // FIXME! Shared with I2C due to me reading the diagram incorrectly. Should move to one of pin 14-19
#define LED 13
#define SKT_BASE 5
#include <Process.h>

// FIXME: Eliminate use of "String" in all included libraries and replace with fixed or stack allocation

// Relay power constants
#define POWER_ON 0
#define POWER_OFF 1

#define HI_THRESHOLD 0.25f
#define LO_THRESHOLD 0.25f

// Listen on default port 5555, the webserver on the Yún
// will forward there all the HTTP requests for us.
YunServer server;

// Low level support for the Dallas Semiconductor 1-Wire bus
OneWire ds(ONE_WIRE_BUS);

// High level support for the DS18B20 sensors
DallasTemperature sensors(&ds);

#define MAX_DS1820_SENSORS 8
// Set of discovered sensors
DeviceAddress addr[MAX_DS1820_SENSORS];
unsigned short valid = 0; // Max of 16 sensors

system_flags sys_state;

// Copy of the configuration loaded from eeprom on boot
struct herp_header herp;

// Write a completion message to a REST client
inline void send_ok(YunClient &client) {
  client.println(F("OK"));
  return;
}

// Scan the one wire bus and update the list of valid sensors
// TODO: Report on missing sensors 
// FIXME: Don't exceed max # of sensors we can handle!
int scan() {
  ds.reset_search();
  memset(addr, 0, sizeof(addr));
  valid = 0;
  int i = 0;
  do {
    //wdt_reset();
    if (!ds.search(addr[i])) {
      // No more sensors
      break;
    }
    if (OneWire::crc8(addr[i], 7) != addr[i][7]) {
      // Sensor checksum bad
      continue;
    }
    if (addr[i][0] != 0x28) {
      // Not a DS18B20
      continue;
    }
    valid |= (1 << i);
    i++;
  } while (true);

  if (!valid) {
    // No sensors found, turn off das blinkenlicht
    digitalWrite(LED, LOW);
  }
  return i;
}

// Set all the socket pins controlled by the timer to 'state'
void timerSet(int idx, int state) {
  for (int i = 0; i < MAX_POWER_SKT; i++) {
    if (herp.power[i].ctrl_type == CTRL_TIMER && herp.power[i].controller.timer_idx == idx) digitalWrite(SKT_BASE + i, state);
  }
}

// Set all timer controlled sockets to ON
inline void timerOn(int idx) {
  timerSet(idx, POWER_ON);
}

// Set all timer controlled sockets to OFF
inline void timerOff(int idx) {
  timerSet(idx, POWER_OFF);
}

// Write an error message and abort if the next character in the REST request is not a '/'
// TODO: Investigate longjump here
bool checkSlash(YunClient &client) {
  if (client.read() != '/') {
    client.println('/');
    return false;
  }
  return true;
}

#define MAX(a,b) ((a)>(b))?(a):(b)

#ifdef USE_NAMES
// Format n/<skt>/<new name>
inline void nameCommand(YunClient &client) {
  if (!checkSlash(client)) return;
  int skt = client.parseInt();
  if (!checkSlash(client)) return;
  client.read(); // Bug workaround
  char *ptr = herp.power[skt].name;
  for (int i = 0; i < NAME_LENGTH - 1; i++) {
    char c = client.read();
    if (isspace(c)) break;
    if (c == '+') c = ' '; // Simple URL decode of space
    *ptr++ = c;
  }
  *ptr = 0; // Ensure null terminate short names
  writeEeprom();
  send_ok(client);
  return;
}
#endif

inline void bindSensor(YunClient &client, int skt) {
  if (!checkSlash(client)) return;
  int sensorIdx = client.parseInt(); // FIXME: possible race condition here if sensor order has changed
  if (!(valid & 1 << sensorIdx)) {
    client.println('X');
    return;
  }
  sensors.getAddress(herp.power[skt].controller.sensor.addr, sensorIdx);
  if (!checkSlash(client)) return;
  setTarget(skt, client.parseFloat());
  if (client.read() != ',') {
    client.println(',');
    return;
  }
  setLo(skt, client.parseFloat());
  if (client.read() != ',') {
    client.println(',');
    return;
  }
  setHi(skt, client.parseFloat());
  writeEeprom();
  send_ok(client);
  return;
}

// Bind a sensor or timer to a socket. This is the master configuration command
// Format: b/<skt>/0 - Always off
//         b/<skt>/1/<sensor idx>/<target temp in C>,<lo offset>*10,<hi offset>*10 - Sensor controlled
//         b/<skt>/2/<timer idx> - Timer Controller
//         b/<skt>/3/[01] - Short manual override
//         b/<skt>/4 - Always on
inline void bindCommand(YunClient &client) {
  if (!checkSlash(client)) return;
  int skt = client.parseInt();
  if (skt < 0 || skt >= MAX_POWER_SKT) {
    client.println('S');
    return;
  }
  if (!checkSlash(client)) return;
  int mode = client.parseInt();
  // Simple range check on mode
  if (mode > CTRL_MAX || mode < CTRL_NONE) {
    client.println('M');
    return;
  }
  if (mode != CTRL_MANUAL) herp.power[skt].ctrl_type = mode;
  switch (mode) {
    case CTRL_NONE: {
        digitalWrite(SKT_BASE + skt, POWER_OFF);
        memset(herp.power[skt].controller.sensor.addr, 0, sizeof(DeviceAddress));
        writeEeprom();
        send_ok(client);
        return;
      }
    case CTRL_ALWAYS_ON: {
        digitalWrite(SKT_BASE + skt, POWER_ON);
        memset(herp.power[skt].controller.sensor.addr, 0, sizeof(DeviceAddress));
        writeEeprom();
        send_ok(client);
        return;
      }
    case CTRL_SENSOR: {
        bindSensor(client, skt);
        return;
      }
    case CTRL_TIMER: {
        if (!checkSlash(client)) return;
        memset(herp.power[skt].controller.sensor.addr, 0, sizeof(DeviceAddress));
        herp.power[skt].controller.timer_idx = client.parseInt();
        writeEeprom();
        resetTimers();
        send_ok(client);
        return;
      }
    case CTRL_MANUAL: {
        if (!checkSlash(client)) return;
        int val = client.parseInt();
        digitalWrite(SKT_BASE + skt, val ? POWER_OFF : POWER_ON);
        // No store to eeprom, temporary manual override
        // Use CTRL_NONE or CTRL_ALWAYS_ON for permenant state
        send_ok(client);
        return;
      }
  }
  client.println(F("?"));
}

// Invoke 'date +%s' on the Linux side to get the seconds from epoch
time_t timeSync() {
  Process date;
  date.begin(F("date"));
  date.addParameter(F("+%s")); // Time in seconds from epoch
  date.run();
  time_t time = 0;
  if (date.available() > 0) {
    // get the result of the date process (should be hh:mm:ss):
    String timeString = date.readString();
    time = (time_t)timeString.toInt(); // FIXME: Need a proper conversion function which can parse and return time_t
  }
  date.close();
  return time;
}

// Curried timer functions as timer alarm library doesn't permit passing data
void timerOn0() {
  timerOn(0);
}
void timerOn1() {
  timerOn(1);
}
typedef void (*TimerPtr)();
TimerPtr timerOnA[] = { timerOn0, timerOn1 };
void timerOff0() {
  timerOff(0);
}
void timerOff1() {
  timerOff(1);
}
TimerPtr timerOffA[] = { timerOff0, timerOff1 };

void resetTimers() {
  for (uint8_t id = 0; id < dtNBR_ALARMS; id++) {
    Alarm.free(id);
  }

  for (int i = 0; i < TIMER_MAX; i++) {
    if (herp.timer[i].on_hour != 0 || herp.timer[i].on_min != 0) {
      Alarm.alarmRepeat(herp.timer[i].on_hour, herp.timer[i].on_min, 0, timerOnA[i]);
      Alarm.alarmRepeat(herp.timer[i].off_hour, herp.timer[i].off_min, 0, timerOffA[i]);
    }
  }
}

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  
  //wdt_enable(WDTO_2S); // Enable watchdog timer to reset MCU after 8 seconds if not reset
  
  readEeprom();

  // Ensure all sockets start off powered down unless specifically set to CTRL_ALWAYS_ON
  for (int i = 0; i < MAX_POWER_SKT; i++) {
    if (herp.power[i].ctrl_type == CTRL_ALWAYS_ON) digitalWrite(SKT_BASE + i, POWER_ON);
    else digitalWrite(SKT_BASE + i, POWER_OFF);
    pinMode(SKT_BASE + i, OUTPUT);
  }

  //wdt_reset();

  // Bridge startup
  Bridge.begin();

  //wdt_reset();
  
  // Initialize clock
  setTime(timeSync());
  // Setup to sync time every 12 hours
  setSyncInterval(60 * 60 * 12);
  setSyncProvider(timeSync);
  sys_state.bootTime = now();

  //wdt_reset();
  
  // Ensure any configured timers are running
  resetTimers();

  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();

  // Start the 1-Wire bus and configure the DS18B20 sensors
  sensors.begin();
  sensors.setResolution(TEMP_10_BIT);
  digitalWrite(LED, HIGH);
  scan();

  Process touch;
  touch.begin(F("touch"));
  touch.addParameter(F("/tmp/mcu_boot")); // Time in seconds from epoch
  touch.run();
  touch.close();
}

void setLo(int v, float lo) {
  if (lo > getTarget(v)) lo = getTarget(v);
  herp.power[v].controller.sensor.temp.lo = (byte)((herp.power[v].controller.sensor.temp.target - lo) * 10);
}

void setHi(int v, float hi) {
  if (hi < getTarget(v)) hi = getTarget(v);
  herp.power[v].controller.sensor.temp.hi = (byte)((hi - herp.power[v].controller.sensor.temp.target) * 10);
}

float getLo(int v) {
  return herp.power[v].controller.sensor.temp.target - ((float)herp.power[v].controller.sensor.temp.lo) / 10;
}

float getHi(int v) {
  return herp.power[v].controller.sensor.temp.target + ((float)herp.power[v].controller.sensor.temp.hi) / 10;
}

float getTarget(int v) {
  return herp.power[v].controller.sensor.temp.target;
}

void setTarget(int v, float target) {
  herp.power[v].controller.sensor.temp.target = target;
}

// Scan all the sensors and update any sockets for which the temperature is outside the "ON" zone
void checkTemps(/*YunClient &client*/) {
  // Scan bus for new sensors
  scan();

  // Get all the sensors to read their temperatures
  // and store them in the scratchpad
  sensors.requestTemperatures();

  for (int i = 0; i < MAX_DS1820_SENSORS; i++) {
    if (!(valid & (1 << i))) continue;
    //wdt_reset();
    float cTemp = sensors.getTempC(addr[i]);
    for (int j = 0; j < MAX_POWER_SKT; j++) {
      if (herp.power[j].ctrl_type != CTRL_SENSOR) continue;
      if (memcmp(herp.power[j].controller.sensor.addr, addr[i], sizeof(DeviceAddress))) continue;
      // Found a socket bound to this sensor
      if (cTemp < getLo(j)) {
        digitalWrite(SKT_BASE + j, POWER_ON);
      } else {
        if (cTemp > getHi(j)) {
          digitalWrite(SKT_BASE + j, POWER_OFF);
        }
      }
    }
  }
  sys_state.scanTime = now();
}

void loop() {
  // Get clients coming from server
  YunClient client = server.accept();

  // There is a new client?
  if (client) {
    // Process request
    process(client);

    // Close connection and free resources.
    client.stop();
  } else {
    checkTemps(/*NULL*/);
    // No pending requests. Wait for a while
    Alarm.delay(1000); // Poll every 1000ms
  }

//  wdt_reset(); // Pat the watchdog
}

inline float cToF(float tempC) {
  return DallasTemperature::toFahrenheit(tempC);
}

void fmt2DigitsR(char *buf, int num, int radix) {
  char ibuf[3];
  ibuf[2] = 0;
  itoa(num, ibuf, radix);
  if (ibuf[1] == 0) {
    ibuf[0] = '0';
    itoa(num, ibuf + 1, radix);
  }
  memcpy(buf, ibuf, 2);
}

void fmt2XDigits(char *buf, int num) {
  fmt2DigitsR(buf, num, RADIX_HEX);
}

void fmt2Digits(char *buf, int num) {
  fmt2DigitsR(buf, num, RADIX_DEC);
}

void sensorIdToBuffer(const int idx, char *buf) {
  for (int i = 0; i < 8; i++) {
    fmt2XDigits(buf, addr[idx][i]);
    buf += 2;
  }
  buf[0] = 0;
}

void sensorIdPtrToBuffer(DeviceAddress addr, char *buf) {
  for (int i = 0; i < 8; i++) {
    fmt2XDigits(buf, addr[i]);
    buf += 2;
  }
  buf[0] = 0;
}

// Return the full state of all the sensors, sockets and their bound sensors/timers (if any)
inline void stateCommand(YunClient &client) {
  unsigned long start=millis();
  getTempCommand(client);
  client.println(F("SKT,ON,CTRL,SENS,LC,TC,HC,TI"));
  char buf[17];
  for (int i = 0; i < MAX_POWER_SKT; i++) {
    client.print(i);
    client.print(',');
    client.print(digitalRead(SKT_BASE + i)); // This reports true logic. With the relays I use, 0 = ON, 1 = OFF
    client.print(',');
#ifdef USE_NAMES
    strncpy(buf, herp.power[i].name, NAME_LENGTH);
    buf[NAME_LENGTH] = 0;
    client.print(buf);
    client.print(',');
#endif
    client.print(herp.power[i].ctrl_type);
    client.print(',');
    sensorIdPtrToBuffer(herp.power[i].controller.sensor.addr, buf);
    client.print(buf);
    client.print(',');
    if (herp.power[i].ctrl_type == CTRL_SENSOR) {
      client.print(getLo(i));
      client.print(',');
      client.print(getTarget(i));
      client.print(',');
      client.print(getHi(i));
    } else {
      client.print(F("0.0,0.0,0.0"));
    }
    client.print(',');
    client.print(herp.power[i].controller.timer_idx);
    client.println();
  }
#ifdef MORE_STATS  
  client.print(F("S,"));
  client.println(start);
  sys_state.reportDuration=millis()-start;
  client.print(F("D,"));
  client.println(sys_state.reportDuration);
#endif
}

// Get the IDs of all connected sensors and their temperature irrespective of whether they are controlling a socket
inline void getTempCommand(YunClient &client) {
  char buf[17];

  digitalWrite(LED, LOW);

  // Scan bus for new sensors
  scan();

  // TODO: Report removed sensors so we can detect failures

  // Get all the sensors to read their temperatures
  // and store them in the scratchpad
  sensors.requestTemperatures();

  client.println(F("SENS,C,F"));
  for (int i = 0; i < MAX_DS1820_SENSORS; i++) {
    if (!(valid & (1 << i))) continue;
    sensorIdToBuffer(i, buf);
    client.print(buf);
    float cTemp = sensors.getTempC(addr[i]);
    client.print(F(","));
    client.print(cTemp);
    client.print(F(","));
    client.println(cToF(cTemp));
  }
  digitalWrite(LED, HIGH);
}

// Set  timer on/off times
//Format : T/<idx>/h1:m1,h2:m2
inline void timeCommand(YunClient &client) {
  if (!checkSlash(client)) return;
  int idx = client.parseInt();
  if (!checkSlash(client)) return;
  herp.timer[idx].on_hour = client.parseInt();
  if (client.read() != ':') {
    client.println(':');
    return;
  }
  herp.timer[idx].on_min = client.parseInt();
  if (client.read() != ',') {
    client.println('C');
    return;
  }
  herp.timer[idx].off_hour = client.parseInt();
  if (client.read() != ':') {
    client.println(':');
    return;
  }
  herp.timer[idx].off_min = client.parseInt();
  writeEeprom();
  resetTimers();
  send_ok(client);
}

inline void systemCommand(YunClient &client) {
  // System Flag state
  client.print(sys_state.didReset);
  client.print(',');
  client.print(sys_state.wroteHeader);
  client.print(',');
  client.print(sys_state.readHeader);
  client.print(',');
  client.print(sizeof(herp));
  client.print(',');
  client.print(sys_state.bootTime);
  client.print(',');
  client.print(sys_state.scanTime);
#ifdef MORE_STATS
  client.print(',');
  client.println(sys_state.reportDuration);
#else
  client.println();
#endif
  for (int i = 0; i < TIMER_MAX; i++) {
    client.print(herp.timer[i].on_hour);
    client.print(':');
    client.print(herp.timer[i].on_min);
    client.print(',');
    client.print(herp.timer[i].off_hour);
    client.print(':');
    client.print(herp.timer[i].off_min);
    client.println();
  }
}

void process(YunClient &client) {
  char command;
  // read the command
  command = client.read();

  switch (command) {
    case 'T': {
        timeCommand(client);
        return;
      }
    case 't': {
        getTempCommand(client);
        return;
      }
    case 'b': {
        bindCommand(client);
        return;
      }
    case 's': {
        stateCommand(client);
        return;
      }

#ifdef USE_NAMES
    case 'n': {
        nameCommand(client);
        return;
      }
#endif

    case 'S': {
        systemCommand(client);
        return;
      }
    default: {
        client.println('?');
        return;
      }
  }
}



