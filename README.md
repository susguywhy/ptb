# ptb
Automatic High Voltage Traction Battery Charge Button Pusher Code with Timing

This project will make use of the following Peripherals and Libraries on the Nano 33 IoT board:

Digital Pins:  
Pin 2  - Reset (loopback to RESET on board).  
Pin 3  - LED for status.  
Pin 15 - Servo (3.3V only).

I2C:  
Communication to INA226 IC

Serial UART:  
for Debug

Arduino Software Libraries:  
RTCZero  
NTPClient  
TimeoutCallback  
FlashAsEEPROM  
FlashStorage  
WiFiNINA  
WiFiUdp  
Servo  
INA226  

I have not figured out how to share the sketch/"Thing" via the Arduino Cloud ecosystem; 
you will mostly likely need to setup an Arduino Cloud account and setup a "Thing" (https://docs.arduino.cc/arduino-cloud/cloud-interface/things/) and then copy the code from ptb.c (by means of copy-and-paste) into your empty sketch.

There are Cloud Variables deployed in this project, so you'll need to recreate them locally using the Arduino Cloud web interface

A Dashboard is also recommended so you can visualize the data that arrives from the Nano.

Refer to the following to see what variables are needed and an exmaple of the dashboard used: 
https://susguywhy.github.io/ptb.html#cv

