/*
 * Config.h
 *
 *
 *   HKC Alarm Panel Arduino Internet Enabled Keypad
 *
 *   For Arduino (UNO or Leonardo) with added Ethernet Shield
 *
 *   See Circuit Diagram for wiring instructions
 *
 *   Author: Ozmo
 *
 *   See: http://www.boards.ie/vbulletin/showthread.php?p=88215184
 *
*/

//--------Configuration Start----------

#pragma once

//Wifi Password (Required)
#define WIFI_SSID "{WIFI NAME HERE}"
#define WIFI_PASSWORD "{WIFI PASS HERE}"


//Email Passwords - Works with gmail
//Emails are optional - comment out line "#define SENDEMAILS" to not send emails
//Important: you must create a password according to this guide: https://support.google.com/accounts/answer/185833
#define SMTP_SERVER "smtp.gmail.com"        
#define SMTP_USER "YourSendEmailAddress@gmail.com"
#define SMTP_PASS "GoogleGeneratedPasscode" //see comment above
#define EMAIL_ADDR "example@example.com"  //Email to send to
#define SMTP_PORT 465 //465 works ok for GMAIL
#define EMAIL_SUBJECT "House Alarm"

//LED_BUILTIN Builtin Red led is Pin 2 on D1 R32 (17 on some esp32)
// #define LED_Stat 17 // this binks when packets are sent to panel
// #define ledFeedback 25 // Blink a Led we can use to show ESP is running
// #define SERIAL1_TXACTIVE -1 //Pin is lit when sending
// //#Serial Port pins - We leave Serial0 for programming and debug - this serial port connects to the HKC Panel via the circuit
// //These are the Pins to use for Serial Connection
// #define SERIAL1_RXPIN 16
// #define SERIAL1_TXPIN 27

#define ledFeedback 14 // Blink a Led we can use to show ESP is running
#define LED_Stat 12 // this binks when packets are sent to panel
#define SERIAL1_RXPIN 13
#define SERIAL1_TXPIN 15
#define SERIAL1_TXACTIVE -1 //Pin is flashed when sending (not used)
//LCD Display Pins
//SDA=18, SCL=19 for my custom boards
//SDA=5,  SCL=4 for ESP board with built in screen (https://www.aliexpress.com/item/33047481007.html)
#define SCREEN_SDA 5
#define SCREEN_SCL 4


//The Arduino IP address and Port (192.168.1.177)
#define IP_ME "192.168.0.177"
#define IP_GW "192.168.0.1"
#define IP_SN "255.255.255.0"
#define IP_DNS IP_GW //This will use your ISP dns - if that doesnt work - try "8.8.8.8" (googles dns)

#define IP_P 80  //The IP Port for the http server


//Alexa Config
//a hex character 0..9 or a..f to force Alexa to see as new device hub 
// its cached by alexa - so increase by 1 if alexa not discovering this as a new device
#define DEVICEHUB_ID '0'
//End Alexa integration


/////////
//These are advanced configuration...

//Switches - Comment out to disable features.. 
//#define SENDEMAILS //Comment line out to disable sending emails (if email fails to send - wifi will reset!)
#define DEBUG_LOG //Comment out to disable all debug comments
#define ENABLE_DISPLAY //Comment out to disable using LCD
//#define HTTPS //uncomment to use HTTPS (much slower and more limited browser support)
//#define DUMP_RAW_LINE_DATA //This will dump all data from Arduino in hex to debug log (DEBUG_LOG required)
//#define DEBUG_SHOW_ALL_TX //This will dump all data sent from ESP to Alarm to terminal
//#define DEBUG_SHOW_ALL_RX //This will dump all data sent from Alarm to terminal

//#define DEBUG_SHOW_DISPLAY ///Only show LCD Display on terminal
//#define ALEXA //comment out to disable Alexa
#define WIFI
#define WEBSERVER

//Build Version (displayed on webpage)
#define sVersion "Castle V6.01"

//Maximum web browsers that can connect at a time
#define MAX_CLIENTS 16

//Symbols as displayed on the HTML page
#define KEY_STAR "&#9650;"  //unicode arrow up "*"
#define KEY_POUND "&#9660;" //unicode arrow down "#"


enum { WIFI_DOWN = 0, WIFI_PENDING = 1, WIFI_OK = 2 };

