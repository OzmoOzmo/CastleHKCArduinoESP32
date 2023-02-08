/*
*   Castle KeyPad
*
*   HKC Alarm Panel ESP32 Internet Enabled Keypad
*
*   For ESP32
*
*   See Circuit Diagram for wiring instructions
*
*   V1.04  March  2014: Arm Disarm Complete and tested as working
*   V1.2	  April  2014: Updated to use ABCD Keypad bus
*   V1.3	  April  2015: Updated to current Arduino IDE
*   V1.4	         2017: Various Updates for New Chrome Support
*   V2.0	         2023: Ported to ESP32
*
*   Author: Ozmo
*
*   See: http://www.boards.ie/vbulletin/showthread.php?p=88215184
*
*
*
*  Compile for ESP32 Dev Module 
*  For best results : Use on a "ESP32 D1 R32" - This has power converter built in so can power from Alarm Panel
*  
*  
*  It will compile within Arduino IDE with correct esp32 board libraries installed - but much easier using Platformio
*
*  ****See Config.h for all the configuration you may need*****
*/


#include "Log.h"
#include "RKP.h"
#include "SMTP.h"
#include "WebSocket.h"

void ShowVersion(){
    #ifdef ESP32
    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.printf("ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    Serial.printf("ESP32 Model: %d, ", chip_info.model); //esp_chip_model_t (1 = ESP32)
    Serial.printf("silicon revision %d, ", chip_info.revision);
    Serial.printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    Serial.printf("CPU Frequency: %d Mhz\n", getCpuFrequencyMhz()); //Get CPU clock
    #endif
    #ifdef ESP8266
    //todo: add useful diagnostics for 8266
    Serial.println("ESP8266");
    #endif
}

void setup()
{
    //--debug logging start
    Log_Init();

    LogLn("\r\n-----[Start]-----");
    ShowVersion();

    LogLn("LCD Display Init..");
    LCD_Init();

    #ifdef DEBUG_LOG
        LogLn("Logging On."); //Enable Silent Mode for full speed
    #else
        LogLn("(Mostly)Silent Mode enabled.");
    #endif

    LogLn("RKP Init..");
    RKPClass::Init();

    LogLn("Server Init..");
    WebSocket::ServerInit();

    //LogLn("SMTP Init..");
    //SMTP::QueueEmail(MSG_START); //Creates a bootup email

    //SMTP::StartEmailMonitor();

    WebSocket::StartWebServerMonitor();

    //Flashing led on Pin 12 show us we are still working ok
    LogLn("If LED not Blinking - Ensure Panel is Connected.");
    pinMode(ledFeedback, OUTPUT);
    pinMode(LED_Stat, OUTPUT);
}

void loop()
{
    LogLn("Main Loop");

    while (true)
    {
        //check for comms activity
        //Poor performance for RKP when in a thread - we will poll on core0
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
