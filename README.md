vivarium
========

Expandable Vivarium Controller (Arduino Yun Varient)

This project is the driver software for an expandable Vivarium temperature controller using an Arduino Yun as the 
system master controller.

Features
--------

This controller has a number of software features
* Configurable binding for sensor to 110v relay
* Can set target temperature for each sensor
* Can log data via collectd on the OpenWRT side of the Yun
* REST api to set and retrieve temperature control settings and current sensor values

Required Hardware
-----------------

In addition to an Arduino the following other components are required (URLs are provided for example only):
* DS18S/B20 temperature sensors x 1 - 8
  https://www.sparkfun.com/products/11050 
* 5v controlled 110V relay switch module, 8 way
* SD Card for Yun for expanded OpenWRT components like collectd
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
* The latest Time and Alarm library 
  http://www.pjrc.com/teensy/td_libs_Time.html
* Yun Bridge library
* Dallas Semiconductor temperature library

Pin definitions
---------------

* Pin #3 (Digital) - Dallas 1-Wire for the temperature sensors. These are all daisy chained on this one pin.
* Pin #4-12 (Digital) - Relay control
