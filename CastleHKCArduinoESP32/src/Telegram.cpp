/*
  Telegram.cpp

  Created: 22/2/2023
  Author: Ambrose

  ENABLE_TELEGRAM defined in config.h determines if Telegram messages will be sent

  Sends alerts to your phone.

  To use - fill out details of your private bot in config.h
*/
#include "Config.h"
#ifdef ENABLE_TELEGRAM

#include <Arduino.h>
#include "Telegram.h"
#include "Log.h"
#include "WebSocket.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "TelegramAPI.h"
#include "RKP.h"

WiFiClientSecure secured_client;
TelegramAPI bot;
unsigned long bot_lasttime; // last time messages' scan has been done
const int ledPin = 12; //Enable turning On and Off an Output via telegram

Telegram::Telegram(){}
String Telegram::sMsgToSend = "Telegram Startup.";

void handleNewMessages(String sMessage)
{
    static String chat_id_sendto(CHAT_ID); //we will only ever send to our own account...
    /*if (chat_id != CHAT_ID){
      bot.sendSimpleMessage(chat_id, "Unauthorised user", "");
      return;
    }*/

    // Print the received message
    LogLn("Received Command:" + sMessage);

    if (sMessage == "/ledon")
    {
      digitalWrite(ledPin, HIGH);
      bot.sendSimpleMessage(chat_id_sendto, "Led is ON", "");
    }

    if (sMessage == "/ledoff")
    {
      digitalWrite(ledPin, LOW); // turn the LED off
      bot.sendSimpleMessage(chat_id_sendto, "Led is OFF", "");
    }

    if (sMessage == "/status")
    {
      //Telegram::QueueTelegram
      bot.sendSimpleMessage(chat_id_sendto, RKPClass::mbIsPanelAlarm == true ? "Red Light is ON!" : "Red Light is Off", "");
      bot.sendSimpleMessage(chat_id_sendto, RKPClass::mbIsPanelWarning == true ? "Yellow Light is ON!" : "Yellow Light is Off", "");

    }

    if (sMessage == "/start")
    {
      String welcome = "Welcome.\n";
      //welcome += "Available Comamnds are.\n\n";
      //welcome += "/ledon : to switch the Led ON\n";
      //welcome += "/ledoff : to switch the Led OFF\n";
      //welcome += "/status : Returns current status of LED\n";
      LogLn("Sending Start");
      bot.sendSimpleMessage(chat_id_sendto, welcome, "");
      //bot.sendSimpleMessage(chat_id_sendto, welcome, "Markdown");
    }
    return;
}

void Telegram::QueueTelegram(String msgToSend)
{
    LogLn(F("Queue Telegram."));
    if (Telegram::sMsgToSend.length() > 1000)
      Telegram::sMsgToSend.clear();
    Telegram::sMsgToSend += msgToSend+".";
}


int nErrorCount = 0;
void SendTelegramThread(void* parameter)
{
    LogLn(F("Start TelMon"));
    while(true)
    {
        String sMessage = bot.getUpdates();
        if (sMessage.length() > 0)
            handleNewMessages(sMessage); //bot.sFromID

        if (Telegram::sMsgToSend.length() > 0)
        {//Alarm etc. Send to our fixed user 0
          if (bot.sendSimpleMessage(CHAT_ID, Telegram::sMsgToSend, ""))
          {
            LogLn("Tel Sent: " + Telegram::sMsgToSend);
            Telegram::sMsgToSend.clear();
          }
        }

        bot.closeClient();

        //bot_lasttime = millis();
        delay(BOT_MTBS); //all ok - wait x secs before checking again

        #ifdef REPORT_STACK_SPACE
        Logf("threadTel %d\n", uxTaskGetStackHighWaterMark(NULL)); //currently have about 2K ram spare for this thread - loads
        #endif
    }
}

void Telegram::StartMonitor()
{
    LogLn("Telegram Enabled");
    xTaskCreatePinnedToCore(
        SendTelegramThread,
        "threadTel", // Task name
        5000,            // Stack size (bytes)
        NULL,             // Parameter
        1,                // Task priority
        NULL,             // Task handle
        1                 // ESP Core
    );
}
#endif

