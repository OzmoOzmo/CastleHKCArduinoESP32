/*
*   Castle KeyPad
*
*   HKC Alarm Panel ESP32 Internet Enabled Keypad
*
*   For Arduino (UNO or Leonardo) with added Ethernet Shield
*
*   See Circuit Diagram for wiring instructions
*
*   V1.04  March  2014: Arm Disarm Complete and tested as working
*   V1.2	  April  2014: Updated to use ABCD Keypad bus
*   V1.3	  April  2015: Updated to current Arduino IDE
*
*   Author: Ozmo
*
*   See: http://www.boards.ie/vbulletin/showthread.php?p=88215184
*
*
*
*  Compile for ESP32 Dev Module 
*  For best results : Use on a "D1 R32" or "WEMOS LOLIN32" - a ESP32 module with a built in LCD display 
*  For any other bord or kit without display - Select board to be any of the ESP32 Boards "EXP32 Dev Module" is ok.
*  
*  You will need install the espressif\esp32 board support from within Arduino IDE
*  Use this url in Arduino IDE preferences
*     ESP32 V1->  https://dl.espressif.com/dl/package_esp32_index.json 
*
*
* Platformio will auto install library "esp32_https_server"
*     https://github.com/fhessel/esp32_https_server  - for https, wss sockets
* 
*   Which contains a bug - you will need modify line 9 of this file...
*    C:\Projects\GitHub\CastleHKC_ESP32_RKP\221223-042156-wemos_d1_mini32\.pio\libdeps\wemos_d1_mini32\esp32_https_server\src\HTTPConnection.hpp
*       #include <esp32/sha.h>  //#include <hwcrypto/sha.h>
*
*   esp8266-oled-ssd1306
*     https://github.com/ThingPulse/esp8266-oled-ssd1306 - for LCD Support- works well on ESP32
*
*   Preferences 
*     https://github.com/vshymanskyy/Preferences - version 2
*      
*      
*/

//See Config.h for all the configuration you may need

//required for Visual Micro - the ino file needs to include every header we will use in other .cpp files
#include <WiFi.h>
#include <Preferences.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
//#include <HTTPRequest.hpp>
#include <WiFiUdp.h>
#include "oleddisplay.h"

#ifdef HTTPS
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#endif
//end required for Visual Micro

#include "Log.h"
#include "RKP.h"
#include "SMTP.h"
#include "WebSocket.h"

//#include <SoftwareSerial.h>

Preferences prefs;

/*
SoftwareSerial swSer;

void setup()
{
    #define SERIAL1_RXPIN 27
    #define SERIAL1_TXPIN 16

    Serial.begin(115200);
    Serial.println("Start...");

    swSer.begin(1658, SWSERIAL_8S1, SERIAL1_RXPIN, SERIAL1_TXPIN, false); //9 bit Mark/Space
    //swSer.begin(115200, SWSERIAL_8N1, 12, 12, false, 256);

    if (!swSer) { // If the object did not initialize, then its configuration is invalid
        Serial.println("Invalid SoftwareSerial pin configuration, check config"); 
        while (1) { // Don't continue with invalid configuration
            delay (1000);
        }
    } 
}

void CheckSwSerial(SoftwareSerial* ss) {
	byte ch;

    //write with mark: write(ch, SWSERIAL_PARITY_MARK)

	if (ss->available()) {
		//Serial.print(PSTR("\nResult:"));
		while (ss->available()) {
            bool bBit9 = ss->readParity();
			ch = (byte)ss->read();

            if (bBit9)
                Serial.println();
			Serial.print(ch < 0x10 ? PSTR(" 0") : PSTR(" "));
			Serial.print(ch, HEX);
		}
	}
}

void send(){
    byte buf[10] ={0,1,2,3,4};
	byte sum = 0;
	for (int i = 2; i < 8; i++) 
		sum += buf[i];
    buf[8] = sum;
    swSer.flush();
	swSer.enableTx(true);
	swSer.write(buf, 10);
	swSer.enableTx(false);
}*/

void setup()
{
    //--debug logging start
    Log_Init();

    LogLn("\r\n-----[Start]-----");

    //Logging... Reboot counter.
    int result = prefs.begin("castle", false);
    Logf("PrefInit: %u\n", result);
    unsigned int counter = prefs.getUInt("counter", 0)+1;
    Logf("Reboot counter: %u\n", counter);
    prefs.putUInt("counter", counter);

    LogLn("LCD Display Init..");
    LCD_Init();

    LogLn("RKP Init..");
    RKPClass::Init();

    LogLn("Server Init..");
    WebSocket::ServerInit();

    //]LogLn("SMTP Init..");
    //]SMTP::QueueEmail(MSG_START); //Creates a bootup email

    //]SMTP::StartEmailMonitor();

    WebSocket::StartWebServerMonitor();

    //Flashing led on Pin 12 show us we are still working ok
    LogLn("If LED not Blinking - Ensure Panel is Connected.");
    pinMode(ledFeedback, OUTPUT); digitalWrite(ledFeedback, LOW);

    //Terrible performance for RKP when in a thread - we will run on core0

    #ifndef DEBUG_LOG
        LogLn("(Mostly)Silent Mode enabled.");
    #else
        //Serial.print("Enable Silent Mode for full speed.");
    #endif
}

void loop()
{
    LogLn("Main Loop");
    while (true)
    {
        //CheckSwSerial(&swSer);

        RKPClass::Poll();

        //Flash status led
        static int tiLast = 0;
        int tiNow = millis();
        if (tiLast < tiNow - 500){
            tiLast = tiNow;
            digitalWrite(ledFeedback, !digitalRead(ledFeedback));
        }
    }
}
