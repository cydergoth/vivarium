vivarium
========

Expandable Vivarium Controller (Arduino)

This project is the driver software for an expandable Vivarium temperature controller using an Arduino as the 
system master controller.

Features
--------

This controller has a number of software features
* Menu driven via 12 digit telephone keypad
* Scrolling LCD temperature display
* Real Time Clock enabled
* Configurable names for vivarium sensors
* Configurable binding for sensor to 110v relay
* Can set minimum, maximum warning threshold temperatures which trigger an alarm
* Can set target temperature for each sensor
* Can log data to a micro SD-Card
* Serial interface (via USB) for external communication (I use an old WRT54SLGS flashed with OpenWRT)
* Configurable F/C display
* Backlight Off "Sleep" mode for LCD, press and hold '*' to wakeup

Required Hardware
-----------------

In addition to an Arduino the following other components are required (URLs are provided for example only):
* I2C based LCD controller with Telephone Keypad capability
  http://www.robot-electronics.co.uk/htm/Lcd03tech.htm
* DS18S/B20 temperature sensors x 1 - 8
  https://www.sparkfun.com/products/11050 
* Real time clock compatible with the Arudino "Time" library
  https://www.sparkfun.com/products/10160
* 5v controlled 110V relay switch module
  https://www.sparkfun.com/products/11042
* Optional, for data logging, an SD Card Shield
  https://www.sparkfun.com/products/9802 
* Prototyping shield for mounting connectors and resistors to
  https://www.sparkfun.com/products/7914

Miscellaneous hardware, resistors, sockets, plugs etc

Note: When using low voltage (<110v) controlled 100v relays, always observe mains level precautions and ensure
isolation between the low voltage and high voltage sides. Use a non-conductive filler compound, resin, or a non-condudting shield between the two voltage zones

Required Libraries
------------------
In addition to the software in this repository a number of external libraries are needed

* 1-Wire library
  http://playground.arduino.cc/Learning/OneWire
* The latest Time library
  http://www.pjrc.com/teensy/td_libs_Time.html
  (Note: This project requires the RTC module to already be initialized and running! )
* DS1307RTC library (Real Time Clock)
  http://www.pjrc.com/teensy/td_libs_DS1307RTC.html
* Fat16 library
  https://code.google.com/p/fat16lib/

Pin definitions
---------------

* I2C - Display and keypad, Optional DS1307RTC Real Time Clock
* Pin #2 (Digital) - Dallas 1-Wire for the temperature sensors. These are all daisy chained on this one pin.
* Pin #3 (Digital) - Relay #1. Additional relays may be added as required
* Pin #8 (PCM) - Sounder for alarm 

