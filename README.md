# CastleHKCArduinoESP32
=================================

Any questions I can be found via: https://www.boards.ie/discussion/2057113381

An HKC / SecureWatch Alarm compatible Remote Keypad that makes the Alarms functions and menus
accessible via any desktop or modern mobile phone browser.

It does this by emulating a real keypad - and making a virtual keypad available to your phone.

Features a WIFI connection to your router, Built in Webserver to present the virtual keypad - websockets to provide two way connection from your phone. Sends Email when an alarm is triggered. Also has a built in OLED screen.

Tested on HKC SW8/12 - but should work on many of the older HKC models.

![animation demo](https://github.com/OzmoOzmo/CastleAritechArduinoESP32/blob/master/HowTo/ArduinoAritechInternetKeypadLoop.gif)

Allows you to remote arm/disarm the panel as well as view logs and enter Engineer Menu etc.

Also Emails you when an Alarm happens.

Connection to the Alarm Panel requires connecting an ESP32 to the panels standard 3 wire Remote Keypad bus.

To compile use Platformio.

The circuit required is as follows.

It can be soldered up to yuor requirments and parts available - I used an Adafruit Proto Shield clone mounted on an ESP D1 R32 board.
Both available online from aliexpress and other sources from about $5 each.

Note: I have added a 10uF capacitor between GND and Reset - this is a recommended fix to make it more reliable to flash new software to the ESP.

![Wiring Diagram](https://raw.githubusercontent.com/OzmoOzmo/CastleHKCArduinoESP32/blob/main/Docs/CircuitDiagram.png)



*Note: This is the latest code for an ESP32 - supporting Wifi, Chrome, most phones and Gmail - An Arduino UNO version using Ethernet is also available*
Feb 2023
