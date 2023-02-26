/* 
* SMTP.h
*
* Created: 4/5/2014 7:45:36 PM
*/

#pragma once

#include "Config.h"
#include "Arduino.h" //for boolean byte etc.

class Telegram
{
//variables
public:
	static String sMsgToSend;

public:
	static void QueueTelegram(String msgToSend);

//functions
public:
	static void StartMonitor();
        
protected:
private:
	Telegram();
}; //Telegram

