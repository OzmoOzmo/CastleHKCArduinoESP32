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

#ifndef CONFIG_H_  //Dont touch
#define CONFIG_H_ 1  //Dont touch

//Wifi Password (Required)
#define WIFI_SSID "{WIFI NAME HERE}"
#define WIFI_PASSWORD "{WIFI PASS HERE}"

//Email Password (optional) [wont work with gmail smtp servers]
#define SMTP_SERVER "smtp.yourservice.com"        
#define SMTP_PORT 465                   //always 465 for Secure Gmail
#define EMAIL_SUBJECT "House Alarm"
#define SMTP_USER "YOUREMAIL@gmail.com" //create a 
#define SMTP_PASS "PASSWORD"
#define EMAIL_ADDR "example@example.com"  //Email to send to


//LED_BUILTIN Builtin Red led is Pin 2 on D1 R32 (17 on some esp32)
#define LED_Stat 17 // this binks when packets are sent to panel
#define ledFeedback 25 // Blink a Led we can use to show ESP is running
#define SERIAL1_TXACTIVE -1 //Pin is lit when sending

//#Serial Port pins - We leave Serial0 for programming and debug - this serial port connects to the HKC Panel via the circuit
//These are the Pins to use for Serial Connection
#define SERIAL1_RXPIN 16
#define SERIAL1_TXPIN 27 //todo: one wire 27

//LCD Display Pins
#define SCREEN_SDA 23  //5, 4 for ESP board with built in screen
#define SCREEN_SCL 19  //23, 19 for my custom board




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
//#define SENDEMAILS //Comment line out to disable sending emails
#define DEBUG_LOG //Comment out to disable all debug comments
//#define ENABLE_DISPLAY //Comment out to disable using LCD
//#define HTTPS //uncomment to use HTTPS (much slower and more limited browser support)
//#define DUMP_RAW_LINE_DATA //This will dump all data from Arduino in hex to debug log (DEBUG_LOG required)
//#define DEBUG_SHOW_ALL_TX ///This will dump all data from ESP32 to terminal
//#define DISPLAY_ALL_PACKETS //for debugging comms - noramlly comment this line out (DEBUG_LOG required)
//#define DEBUG_SHOW_REPLIES ///This will dump all data from ESP32 to Panel to terminal #todo: remove this or 3 above - dont need all
//#define DEBUG_SHOW_DISPLAY ///Only show LCD Display on terminal
//#define ALEXA //comment out to disable Alexa

//Build Version (displayed on webpage)
#define sVersion "Castle V6.00"

//Maximum web browsers that can connect at a time
#define MAX_CLIENTS 20

//Symbols as displayed on the HTML page
#define KEY_STAR "&#9650;"  //unicode arrow up "*"
#define KEY_POUND "&#9660;" //unicode arrow down "#"


enum { WIFI_DOWN = 0, WIFI_PENDING = 1, WIFI_OK = 2 };


#endif /* CONFIG_H_ */

