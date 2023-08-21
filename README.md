# CastleHKCArduinoESP32
=================================

Any questions I can be found via: https://www.boards.ie/discussion/2057113381

This is a HKC / SecureWatch Alarm compatible Remote Keypad that makes the Alarms functions and menus
accessible via any desktop or modern mobile phone browser.

It does this by emulating a real keypad - and making a virtual keypad available to your phone.

Features a WIFI connection to your router, Built in Webserver to present the virtual keypad - websockets to provide two way connection from your phone. Sends Email when an alarm is triggered. Also has an optional OLED screen you can add.

Tested on HKC SW8/12 - but should work on many of the older HKC models.

![animation demo](https://github.com/OzmoOzmo/CastleAritechArduinoESP32/blob/master/HowTo/ArduinoAritechInternetKeypadLoop.gif)

Allows you to remote arm/disarm the panel as well as view logs and enter Engineer Menu etc.

Connection to the Alarm Panel requires connecting an ESP32 to the panels standard 3 wire Remote Keypad bus using the circuit described.

To compile use Platformio. Its possible to compile using Arduino IDE as well.

The circuit required is as follows.

It can be soldered up to your requirments and parts available - I used an Adafruit Proto Shield clone mounted on an ESP D1 R32 board.
Both available online from amazon/aliexpress and other sources for as low as $5 each.

Note: I have added a 10uF capacitor between GND and Reset on the ESP32 - this is a common fix to make it more reliable to flash new software to the ESP.

Circuit on a shield:  

![Circuit as shield](https://github.com/OzmoOzmo/CastleHKCArduinoESP32/blob/main/Docs/HkcESP32ProtoShield.png)
BLACK - goes to Panel GND  
BLUE - goes to RKP data  
RED - goes to Panels +12V  
Power Plug - Connect the shield over the ESP32 and Connect the power plug shown on the right to the ESP32 board.  

Or - on a breadboard:

![Circuit](https://raw.githubusercontent.com/OzmoOzmo/CastleHKCArduinoESP32/main/Docs/IMG_5684.jpg)
A goes to ESP32 Receive (Esp Pin 16)  
B goes to Panel RKP GND and also ESp32 GND  
C goes to ESP32 Transmit (Esp Pin 27)  
D goes to Panels RKP port +12V  
E goes to Panels RKP data  
  
![HKC Connection](https://github.com/OzmoOzmo/CastleHKCArduinoESP32/blob/main/Docs/HKCPinOut.JPG?raw=true)


![Wiring Diagram](https://github.com/OzmoOzmo/CastleHKCArduinoESP32/blob/main/Docs/CircuitDiagram_ESP32.png)


*Note: This is the latest code for an ESP32 - supporting Wifi, Chrome, most phones and Gmail - An easier to make, but less capable, Arduino UNO version using an Ethernet Shield is also available on my other GitHub repositories*
Feb 2023
