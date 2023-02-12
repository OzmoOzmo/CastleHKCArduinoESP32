/*
 *
 * Created: 3/30/2014 11:35:06 PM
 *
 *   HKC Alarm Panel Arduino Internet Enabled Keypad
 *
 *   For ESP32 
 *
 *   See Circuit Diagram for wiring instructions
 *
 *   Author: Ambrose Clarke
 *
 *   See: http://www.boards.ie/vbulletin/showthread.php?p=88215184
 *
*/


#include "Log.h"

#ifndef DEBUG_LOG
	void LogBuf(char* t){}
	void LogHex(byte rx){}
	void LogHex(char* s){}
	void LogHex(byte* s , int len){}
#else
	void LogBuf(char* t)
	{
		int ix=0;
		while(t[ix]!=0)
			Log((char)t[ix++]);
		LogLn("..");
	}
	void LogHex(byte rx)
	{//Show one two digit hex number
		if (rx<16) Log('0'); Log(rx,HEX);
		Log(' ');
	}
	void LogHex(char* s)
	{//Show a buffer as hex
		int n=0;
		while(s[n] != 0)
		{
			Log(s[n], HEX);
			Log(' ');
		}
	}
	void LogHex(byte* s , int len, boolean noNL)
	{
	  LogHex(s,len);
	}
	void LogHex(byte* s , int len)
	{
		const int l = 24;
		for(int col=0;;col++)
		{
			if ((col*l) >= len)		//len=10 = 1*8
				break;
			for(int r=0;r<l;r++)
			{
				if (r!=0) Log(' ');
				int x = col*l+r;
				//byte c = (x<len) ? s[col*l+r] : 0x00;
				//if (c<16) Log('0'); Log2(c, HEX);
				if (x<len)
				{
					byte c = s[col*l+r];
					if (c<16) Log('0');	Log(c, HEX); // displays 0 if necessary
				}
				else
					Log("--");
			}
			Log(':');
			for(int r=0;r<l;r++)
			{
				int x = col*l+r;
				byte c = (x<len) ? (s[col*l+r]) : '#'; ///strip bit 7 (flash)
                       
				if (c <= ' ' || c>= 0x7F)
				{Log('.');}		//brackets important
				else
				{Log((char)c);} //brackets important
			}
			LogLn("");
		}
	}
#endif

void Log_Init()
{
#ifdef DEBUG_LOG
	Serial.begin(nSerialBaudDbg);
	delay(100); // Required to catch initial messages
	LogLn("Log Start");
	LCD_Init();
#endif
}



#ifdef ENABLE_DISPLAY
  #include "oleddisplay.h"
  #include "OLEDDisplaySSD1306Wire.h"
  #include "RKP.h" //for the panel display buffer
  #include "config.h" //for pin assignments
  
#define SCREEN_SDA 5 //SDA
#define SCREEN_SCL 4 //SCL
SSD1306Wire display(0x3c, SCREEN_SDA, SCREEN_SCL, GEOMETRY_128_64);
    
  void LCD_Init()
  {
	//we can use Pin 18 as power to display - means can plug in a 4 pin oled display with no soldering... these 4 pins are all in a row together.
	pinMode(18,OUTPUT); digitalWrite(18,HIGH);
	
  	//display.flipScreenVertically(); //if required
	display.init(); // Initialising the UI will init the display too.
    DisplayUpdateDo();
  	LogLn("LCD Started");
  }
  
  //Send update to LCD only (not to WebSockets)
  void FlagDisplayUpdate()
  {
	bDisplayToSend = true;
  }
  
  
  void DisplayUpdateDo()
  {
  	display.clear();
  	
 	display.setFont(display.ArialMT_Plain_10);
	
	// The coordinates define the "left top" starting point of the text
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawString(0,  0, "Wifi: " + gWifiStat);
	display.drawString(0, 10, "Clients: " + String(gClients));
  	display.drawString(0, 20, "Status: " + (gPanelStat.length()==0? "OK" : gPanelStat));
	display.drawString(0, 30, "Disp:" + String(RKPClass::dispBuffer));

	/*
	// The coordinates define the center of the text
	display.setTextAlignment(TEXT_ALIGN_CENTER);
	display.drawString(64, 22, "!Center aligned (64,22)");

	// The coordinates define the right end of the text
	display.setTextAlignment(TEXT_ALIGN_RIGHT);
	display.drawString(128, 33, "!Right aligned (128,33)");
	*/

  	display.display();
  }

#else

  //stubb out functions when no LCD required
  void LCD_Init()
  {
  	LogLn("LCDInit Disabled");
  }
  
  void FlagDisplayUpdate()
  {
  }
  
  void DisplayUpdateDo()
  {
  }

#endif

//Send to Display And to WebSocket
void FlagWebsocketUpdate()
{
	bWebSocketToSend = true;
	bDisplayToSend = true;   //we need send to LCD display also
}

//Store for what will be displayed next display update.
int gClients = 0;
String gWifiStat = "Off";
String gPanelStat = ""; //reserved for showing internatl stats
bool bDisplayToSend = false;
bool bWebSocketToSend = false;
