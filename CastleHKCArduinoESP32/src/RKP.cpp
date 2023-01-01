/*
 * Created: 3/30/2014 11:35:06 PM
 *
 *   Aritech Alarm Panel Arduino Internet Enabled Keypad -  CS350 - CD34 - CD72 - CD91 and more
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

#include <Arduino.h>
#include "RKP.h"
#include "LOG.h"
#include "Websocket.h"
#include "SMTP.h"
#include "Config.h"

#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <SoftwareSerial.h>

SoftwareSerial swSer;

#define nSerialBaudKP_RX 1658 // HKC Baudrate
#define ixMaxPanel 64 + 5	  // 64+5 bytes enough for a panel message buffer
bool RKPClass::mbIsPanelWarning = false;
bool RKPClass::mbIsPanelAlarm = false;
unsigned long RKPClass::timeToSwitchOffLed = 0;

char RKPClass::dispBuffer[DISP_BUF_LEN + 1] = "Not Connected";

FIFO RKPClass::fifo;
byte RKPClass::lastkey = 0xFF;

// HKC Specific Scanning variables
#define RKPINVALID -1
static byte RKPID = RKPINVALID; //this is the keypad id - assigned by the panel
static int bIsScanning=-1;
static bool bWaitingNewID = false;
static byte RKPSerial[] = {0x41, 0xC7, 0x08};

RKPClass::RKPClass()
{ // never runs
}

void RKPClass::SerialInit()
{
	// Initialise the Serial Port -
	// Protocal uses:  1 stop bit 0 parity     9-bit Character size   1658 baud

	swSer.begin(nSerialBaudKP_RX, SWSERIAL_8S1, SERIAL1_RXPIN, SERIAL1_TXPIN, false); // 9 bit Mark/Space

}

void RKPClass::Init()
{
	SerialInit();
	pinMode(LED_Stat, OUTPUT);

	bIsScanning = -1; //we are not currently scanning
	bWaitingNewID = false; //we are not in Add New Devices mode
	RKPID = RKPINVALID;	//we dont know our ID yet
}

char RKPClass::PopKeyPress()
{
	return (char)fifo.pop();
}

void RKPClass::PushKey(char key)
{
	// called from html code when button pressed
	fifo.push(key);
}

void RKPClass::Poll()
{
#ifdef DUMP_RAW_LINE_DATA
	// this will test the circuit is ok - dumps everything on the line to the console
	// each byte is about 5mS - so wait for 10mS before end of packet
	static byte buf[64];
	static int bufix = 0;

	int tiLast = millis();
	while (millis() - tiLast < 10)
	{
		while (swSer.available())
		{
			tiLast = millis();
			buf[bufix++] = swSer.read();
			if (bufix == 64)
			{
				LogHex(buf, bufix);
				bufix = 0;
			}
		}
	}
	if (bufix > 0)
	{
		LogHex(buf, bufix);
		bufix = 0;
	}
#else
	//This produces No errors - but I think is missing packets maybe?
	static byte buf[64];
	static int bufix = 0;
	int tiLast = millis();
	while (millis() - tiLast < 10)
	{
		while (swSer.available())
		{
			tiLast = millis();
			buf[bufix++] = (byte)swSer.read();
			if (bufix == 64){
				Log("Packet too long:"); LogHex(buf, bufix);
				bufix = 0;
			}
		}
	}
	if (bufix > 0)
	{
		RKPClass::loop_PanelMon_ProcessLine(buf, bufix);
		//LogHex(buf, bufix);
		bufix = 0;
	}


	//RKPClass::loop_PanelMon(); //too slow?

	/* 
	//better error handling - but too many errors
	static byte buf[64];
	static int bufix = 0;

	int tiLast = millis();
	while (millis() - tiLast < 10)
	{
		while (swSer.available())
		{
			tiLast = millis();
			bool messageStart = swSer.readParity();
			if (messageStart && bufix > 0)
			{
				Log("Packet Corrupt:");
				LogHex(buf, bufix);
				bufix = 0;
			}
			buf[bufix++] = (byte)swSer.read();
			if (bufix == 64)
			{//message too long - error
				//LogHex(buf, bufix);
				//RKPClass::loop_PanelMon_ProcessLine(buf, bufix);
				Log("Packet too long:"); LogHex(buf, bufix);
				bufix = 0;
			}
		}
	}
	if (bufix > 0)
	{
		//RKPClass::loop_PanelMon_ProcessLine(buf, bufix);
		LogHex(buf, bufix);
		bufix = 0;
	}
	*/

#endif

}


void RKPClass::loop_PanelMon_ProcessLine(byte* buf, int nBufLen)
{
	if (nBufLen < 3)
	{
		Log("Packet too short"); 
		LogHex((byte*)buf, nBufLen);
	}
	else
	{
		static byte msglen = 0;

		bool bIsPanelMsg = ((*buf & 0x80) != 0);

		{
			// how long is the message supposed to be?
			byte b0 = buf[0];
			byte b1 = buf[1];

			if (bIsPanelMsg)
			{ // message from Main Panel
				if (b1 == 0x00)
					msglen = 3; // P  A1 00 A1
				else if (b1 == 0x01)
					msglen = 20; // length is in byte[2] but message is always padded with spaces //P  D0 01 0F 44 65 76 69 63 65 73 20 66 6F 75 6E 64 20 33 20 52 -- -- -- --:P..Devices.found.3.R####
				else if (b1 == 0x02)
					msglen = 4; // P  D0 02 00 D2
				else if (b1 == 0x03)
					msglen = 5; // P  C0 03 33 3F 35 Possibly to light leds or sound buzzer?
				else if (b1 == 0x04)
					msglen = 5; // P  C0 04 08 00 CC  happened during unset   D0 04 08 00 DC   D0 04 08 00 DC  on entering unset
				else if (b1 == 0x05)
					msglen = 3; // P  D0 05 D5      All devices enter scan mode
				else if (b1 == 0x06)
					msglen = 3; // P  8F 06 95      Scan Next
				else if (b1 == 0x07)
					msglen = 8; // P  9F 07 01 41 C7 08 01 B8 //assign id
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				else if (b1 == 0x0C)
					msglen = 4; // P  00 0C 0C    D0 0C 0C E8  //When you press 0 - screen clears?
				else if (b1 == 0x0D)
					msglen = 4; // P  D0 0D FF DC  //on entering eng mode
				else if (b1 == 0x0E)
					msglen = 4; // P  C0 0E 01 CF  //on unsetting   (comms fault.bat fault)
				else if (b1 == 0x0F)
					msglen = 5; // P C0 0F 00 3F 0E   Leaving eng mode
				else
				{
					LogHex(buf, nBufLen);
					LogLn("Unknown Command");
				}
			}
			else
			{ // message from another keypad or device not main panel
				if (b1 == 0x00)
					msglen = 7; // K0 20 00 72 FF FF FF 8F
				else if (b1 == 0x01)
					msglen = 3; // K0 00 01 01
				else if (b1 == 0x02)
					msglen = 3; //   10 02 12
				else if (b1 == 0x03)
					msglen = 3; // K0 10 03 13  Command#3 Possibly to light leds or sound buzzer?
				else if (b1 == 0x04)
					msglen = 3; //   10 04 14 on entering Unset //rkp during unset
				else if (b1 == 0x05)
					msglen = 0; // No response ever given
				else if (b1 == 0x06)
					msglen = 7; // K1 0F 06 41 C7 08 01 26  (scan response)
				else if (b1 == 0x07)
					msglen = 4; // K1 11 07 00 18
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				else if (b1 == 0x0C)
					msglen = 3; // K1 C0 0C 0C D8  //When you press 0 - screen clears
				else if (b1 == 0x0D)
					msglen = 3; // K1 10 0D 1D  //on entering eng mode
				else if (b1 == 0x0E)
					msglen = 3; // P  00 0E 0E  //on unsetting
				else if (b1 == 0x0F)
					msglen = 3; // K1 00 0F 0F Leaving eng mode
				else
				{
					LogLn("Unknown Command");
					LogHex(buf, nBufLen);
				}
			}
		}

		if (nBufLen == msglen)
		{ // complete message
			//Log("Ok: ");LogHex(buf, bufix);
			RKPClass::ReplyToPanel(buf, nBufLen);
			Log(bIsPanelMsg ? "P " : "K "); LogHex(buf, nBufLen); //this shows every message on the wire...
		}
		else
		{
			Log("Incomplete expecting:"); 
			LogLn(nBufLen); 
			Log(bIsPanelMsg ? "P " : "K ");
			LogHex((byte*)buf, nBufLen);
		}
	}

	return;
}


// Check see if UART has any data for us from panel - if a complete message was received parse it.
// Note - nothing here is to be blocking
//- so no delays or loops waiting for connections or data
bool RKPClass::loop_PanelMon()
{
	static byte buf[64 + 5];
	static int bufix = 0;
	static bool bReceivingPacketNow = false;
	bool bReceivedNewPacket = false;

	// Knock off the "We Sent To Panel" Led not less than 100ms after we turn it on
	if (timeToSwitchOffLed != 0 && millis() > timeToSwitchOffLed)
	{
		timeToSwitchOffLed = 0;
		digitalWrite(LED_Stat, LOW);
	}

	while (swSer.available())
	while (swSer.available())
	{
		boolean isAddr = swSer.readParity(); // Must read First
		char a = (byte)swSer.read();

		static byte msglen = 0;

		if (isAddr)
		{ // dump frame and start again
			if (bufix > 0)
			{
				Log("Bad Frame:");
				LogHex(buf, bufix);
				LogLn("");
			}
			bufix = 0;
			buf[bufix++] = a;
			// Log("New Frame");LogHex(buf,bufix);
			continue;
		}
		buf[bufix] = a;
		bool bIsPanelMsg = ((*buf & 0x80) != 0);

		if (bufix == 2)
		{
			// how long is the message supposed to be?
			byte b0 = buf[0];
			byte b1 = buf[1];

			if (bIsPanelMsg)
			{ // message from Main Panel
				if (b1 == 0x00)
					msglen = 3; // P  A1 00 A1
				else if (b1 == 0x01)
					msglen = 20; // length is in byte[2] but message is always padded with spaces //P  D0 01 0F 44 65 76 69 63 65 73 20 66 6F 75 6E 64 20 33 20 52 -- -- -- --:P..Devices.found.3.R####
				else if (b1 == 0x02)
					msglen = 4; // P  D0 02 00 D2
				else if (b1 == 0x03)
					msglen = 5; // P  C0 03 33 3F 35 Possibly to light leds or sound buzzer?
				else if (b1 == 0x04)
					msglen = 5; // P  C0 04 08 00 CC  happened during unset   D0 04 08 00 DC   D0 04 08 00 DC  on entering unset
				else if (b1 == 0x05)
					msglen = 3; // P  D0 05 D5      All devices enter scan mode
				else if (b1 == 0x06)
					msglen = 3; // P  8F 06 95      Scan Next
				else if (b1 == 0x07)
					msglen = 8; // P  9F 07 01 41 C7 08 01 B8 //assign id
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				else if (b1 == 0x0C)
					msglen = 4; // P  00 0C 0C    D0 0C 0C E8  //When you press 0 - screen clears?
				else if (b1 == 0x0D)
					msglen = 4; // P  D0 0D FF DC  //on entering eng mode
				else if (b1 == 0x0E)
					msglen = 4; // P  C0 0E 01 CF  //on unsetting   (comms fault.bat fault)
				else if (b1 == 0x0F)
					msglen = 5; // P C0 0F 00 3F 0E   Leaving eng mode
				else
				{
					LogHex(buf, bufix);
					LogLn("Unknown Command");
				}
			}
			else
			{ // message from another keypad or device not main panel
				if (b1 == 0x00)
					msglen = 7; // K0 20 00 72 FF FF FF 8F
				else if (b1 == 0x01)
					msglen = 3; // K0 00 01 01
				else if (b1 == 0x02)
					msglen = 3; //   10 02 12
				else if (b1 == 0x03)
					msglen = 3; // K0 10 03 13  Command#3 Possibly to light leds or sound buzzer?
				else if (b1 == 0x04)
					msglen = 3; //   10 04 14 on entering Unset //rkp during unset
				else if (b1 == 0x05)
					msglen = 0; // No response ever given
				else if (b1 == 0x06)
					msglen = 7; // K1 0F 06 41 C7 08 01 26  (scan response)
				else if (b1 == 0x07)
					msglen = 4; // K1 11 07 00 18
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				// unused code?
				else if (b1 == 0x0C)
					msglen = 3; // K1 C0 0C 0C D8  //When you press 0 - screen clears
				else if (b1 == 0x0D)
					msglen = 3; // K1 10 0D 1D  //on entering eng mode
				else if (b1 == 0x0E)
					msglen = 3; // P  00 0E 0E  //on unsetting
				else if (b1 == 0x0F)
					msglen = 3; // K1 00 0F 0F Leaving eng mode
				else
				{
					LogLn("Unknown Command");
					LogHex(buf, bufix);
				}
			}
		}
		bufix += 1;

		if (bufix == msglen)
		{ // complete message
			//Log("Ok: ");LogHex(buf, bufix);
			RKPClass::ReplyToPanel(buf, bufix);
			Log(bIsPanelMsg ? "P " : "K "); LogHex(buf, bufix);
			bufix = 0;
			msglen = 0;
		}
	}

	return bReceivedNewPacket;
}

// HKC Specific
bool RKPClass::ReplyToPanel(byte *buf, int nBufLen)
{
	// check checksum
	byte ics = 0;
	for (int n = 0; n < (nBufLen - 1); n++)
		ics += buf[n];
	if (ics == 0)
		ics--;
	if (ics != buf[nBufLen - 1])
	{
		Log(F("CS Fail :( "));
		LogHex(buf, nBufLen);
		LogLn("");
		return false;
	}

	// 8 = 1000  //9 = 1001  //A = 1010  //B = 1011  //C = 1100  //D = 1101

	byte b0 = buf[0];
	byte b1 = buf[1];

	bool bSent = false;
	bool bIsPanelMsg = ((b0 & 0x80) == 0x80);
	if (bIsPanelMsg && RKPID != RKPINVALID)
	{ // message from Panel - and we are already setup as an RKP ok
		if (b1 == 0x00)
		{
			/* Command#0: ping
			  K0 00 01 01 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			  P  A1 00 A1 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:!.!#####################
			  K1 21 00 60 FF FF FF 7E -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:!.`...~#################
			  P  A2 00 A2 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:"."#####################
			  K2 22 00 62 FF FF FF 81 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:".b....#################
			  P  A0 00 A0 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			  K0 20 00 62 FF FF FF 7F -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..b....#################
			*/
			byte addr = (b0 & 0x0f);
			if (addr == RKPID)
			{ // ACK     P B1 00 B1
				byte r[7];
				r[0] = b0 & 0x33;	  // keep bit5&6(Counter) and bit1&0(address)
				r[1] = 0;			  // always 0?
				r[2] = 0x60;		  // can be 60, 70 (72=Tamper) (78 just after tamper happens)
				r[3] = PopKeyPress(); // keypress here eg 0x12
				r[4] = PopKeyPress(); // keypress here eg 0x12
				r[5] = PopKeyPress(); // keypress here eg 0x12
				r[6] = (byte)(r[0] + r[1] + r[2] + r[3] + r[4] + r[5]);

				SendToPanel(r, 7);
				bSent = true;
			}
			lastkey = 0xFF;
		}
		else if (b1 == 0x01)
		{
			/* Command#1: Display message
			P  C0 01 20 54 75 65 20 32 37 20 4A 61 6E 20 32 30 BA 30 39 76 -- -- -- --:@..Tue.27.Jan.20:09v####
			K0 00 01 01 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			*/
			byte addr = (b0 & 0x30) >> 4;
			if (RKPID == 0x00)
			{ // RKP0 only ever responds
				byte r[3];
				r[0] = b0 & 0x30; ////keep bit5&6(Counter) - C0 respond 0x00; D0 respond 0x10
				r[1] = b1;
				r[2] = r[0] + r[1];
				SendToPanel(r, 3);
				bSent = true;
			}
			// Make a note to send it to the Mobile Phone
			dispBuffer[0] = RKPClass::mbIsPanelAlarm ? 'A' : ' ';
			dispBuffer[1] = RKPClass::mbIsPanelWarning ? 'W' : ' ';
			dispBuffer[2] = ' '; // always space
			int n = 0;
			for (; n < 16; n++) // always 16 characters
				dispBuffer[n + 3] = buf[n + 3];
			dispBuffer[n]=0;

			//Screen Has Updated
			strcpy(WebSocket::dispBufferLast, dispBuffer); // keep a copy for new websocket connections
			FlagWebsocketUpdate();	// data for websocket has changed
			// Log("Screen Updated:"); LogLn((char*)dispBuffer);
		}
		else if (b1 == 0x03)
		{ // Led Patterns
			/* Command#3 light leds or sound buzzer? We can use this to monitor alarm status
			Green and Red were lit
			P  C0 03 33 3F 35 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:@.3?5###################
			K0 00 03 03 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			P  D0 03 33 3F 45 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:P.3?E###################
			K0 10 03 13 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			P  C0 03 33 3F 35 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:@.3?5###################
			K0 00 03 03 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			//Enter service menu - only Green Led Lit
			P  D0 03 03 3F 15 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:P..?.###################
			K0 10 03 13 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			P  A0 00 A0 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			K0 20 00 72 FF FF FF 8F -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..r....#################
			P  C0 01 0C 53 65 72 76 69 63 65 20 4D 65 6E 75 20 20 20 20 D3 -- -- -- --:@..Service.Menu....S####
			*/

			if (bIsPanelMsg)
			{
				// NO RED: D0 03 0F 3F 21
				//    RED: C0 03 3F 3F 41
				// LogHex(buf, 5);
				// This is the RED(Led3) - 00=Off,01 or 10 = flash, 11=Led On
				byte b = (buf[2] & 0x30);
				if (b == 0x00)
				{
					if (RKPClass::mbIsPanelAlarm == true)
					{
						LogLn(F("Alarm Cleared"));
						RKPClass::mbIsPanelAlarm = false;
					}
				}
				else if (b == 0x30) // 48 is On - 32 is blinking
				{
					if (RKPClass::mbIsPanelAlarm == false)
					{
						RKPClass::mbIsPanelAlarm = true;
						LogLn(F("Alarm!!!!"));
						SMTP::QueueEmail(MSG_ALARM);
					}
				}
			}

			byte addr = (b0 & 0x0f);
			if (addr == RKPID)
			{ // ONLY RKP0 will reply to this
				byte r[3];
				r[0] = b0 & 0x33; // keep bit5&6(Counter)  C0 respond 0x00; D0 respond 0x10 - will always by RKP 0
				r[1] = b1;
				r[2] = r[0] + r[1];
				SendToPanel(r, 3);
				bSent = true;
			}
		}
		// Command 0x02 is buzzer
		// 00=Buzzer OFF
		// 01=Buzzer On/Off
		// 10=Buzzer On/Off
		// 11=Buzzer Constant ON
		// Command 0x04 controls keypad (x|x|x|PlayOff|PlayOn|RecOff|RecOn|LightOn
		else if (b1 == 0x02 || b1 == 0x04 || b1 == 0x0C || b1 == 0x0D || b1 == 0x0E)
		{ // These are commands we dont need to implement - they all require the same ack to be sent
			// P C0 04 08 00 CC -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:@...L###################
			// K 00 04 04 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			// P D0 0C 10 EC -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:P..l####################
			// K 10 0C 1C -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			// P 90 0D 07 A4 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...$####################
			// K 10 0D 1D -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			// P C0 02 00 C2 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:@..B####################
			// K 00 02 02 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################
			// P C0 0E 01 CF -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:@..O####################
			// K 00 0E 0E -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################

			byte addr = (b0 & 0x0f);
			if (addr == RKPID)
			{ // Possibly ONLY RKP0 will reply to this
				byte r[3];
				r[0] = b0 & 0x3F; // keep bit5&6(Counter)  C0 respond 0x00; D0 respond 0x10 & address
				r[1] = b1;
				r[2] = r[0] + r[1];
				SendToPanel(r, 3);
				bSent = true;
			}
		}
		else if (b1 == 0x07)
		{ // handled below in if (bIsPanelMsg) block
		}
		else
		{
			// Log("P Found Command #"); LogLn(b1);
			Log("P ");
			LogHex(buf, nBufLen);
		}
	}
	else if (b1 > 7 && b1 != 0x0C && b1 != 0x0D && b1 != 0x0E)
	{ // Some command we havnt seen before.
		// Log("K Found Command #"); LogLn(b1);
		Log("K ");
		LogHex(buf, nBufLen);
	}

	// These commands dont need a valid RKPID
	if (bIsPanelMsg)
	{
		if (b1 == 0x05)
		{
			/* scanning start - 3 devices all logs ON
			  P  C0 01 00 D3 E3 E1 EE EE E9 EE E7 A0 CB E5 F9 F0 E1 E4 F3 E3 -- -- -- --:@..Scanning.Keypadsc####  //Message to All
			  K0 00 01 01 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //RKP0 Always responds (only RKP0)
			  P  A0 00 A0 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //Ack P0 -> RKP0
			  K0 20 00 72 FF FF FF 8F -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..r....#################  //K0 responds (0x72 - tamper was open)
			  P  81 00 81 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //Ack P0 -> RKP1
			  K1 01 00 70 FF FF FF 6E -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..p...n#################  //K1 responds (0x70 - No tamper reported)
			  P  B0 00 B0 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:0.0#####################  //Ack P0 -> RKP0 (again?? why)
			  K0 30 00 72 FF FF FF 9F -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:0.r....#################  //K0 responds (0x72 - tamper)
			 *P  D0 05 D5 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:P.U#####################  //Panel Skips C0 - sends D0 command 05 (no one acks)      //All devices enter scan mode
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //Panel Sends      8F  command 06 to ALL                  //Next?
			  K1 0F 06 41 C7 08 01 26 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..AG..&#################  //K1 Responds      0F  command 06            41 C7 08 01  //08C741 is the number written on the chip sticker - 01=version
			  P  9F 07 01 41 C7 08 01 B8 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...AG..8################  //Panel sends      9F          07         01 41 C7 08 01  //Assign 01 to RKP
			  K1 11 07 00 18 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....####################  //K1 Responds      11(RKP1)    07         00              //k1 says ok
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //Panel sends      8F  command 06                         //Next?
			  K0 0F 06 87 BC 08 01 61 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...<..a#################  //K0               0F  command 06            87 BC 08 01
			  P  9F 07 00 87 BC 08 01 F2 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....<..r################  //Panel            9F  command 07         00 87 BC 08 01  //Assign 00 to RKP
			  K0 10 07 00 17 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....####################  //K0               10(RKP0)    07         00 17           //k0 says ok
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //PanelFirstTo2    8F          06                         //Next?
			  K2 0F 06 91 CF 08 01 7E -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...O..~#################  //K2               0F          06            91 CF 08 01  //08CF91 is the number written on the chip sticker
			  P  9F 07 02 91 CF 08 01 11 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....O...################  //Panel Assign ID  9F          07         02 91 CF 08 01  //Assign 02 to RKP
			  K2 12 07 00 19 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....####################  //K2               12(RKP2)    07         00              //k1 says ok
			  P  92 09 9B -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //P                92          09                         //Panel requests version number
			  K2 12 09 00 00 01 04 20 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:.......#################  //K2               12(RKP2)    09         00 00 01 04     //keypad says its v1.4
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //P                8F          06                         //Next? (no response)
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //P                8F          06                         //Next? (no response)
			  P  A2 00 A2 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:"."#####################  //PollK2           A2          00
			  K2 22 00 6A FF FF FF 89 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:".j....#################  //K2               22          00     6A FF FF FF    //no keys
			  P  D0 01 0F 44 65 76 69 63 65 73 20 66 6F 75 6E 64 20 33 20 52 -- -- -- --:P..Devices.found.3.R####
			*/
			/*RKPClass::*/ bIsScanning = 2; // todo:  2 for 3 devices - but is 1 better for 2 devices?
			LogLn("Scanning Mode");
		}
		else if (b1 == 0x06 && bIsScanning > 0)
		{
			/*Request Serial - should wait here some time before sending
			  P  8F 06 95 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...#####################  //Panel Sends      8F  command 06 to ALL                  //Next?
			  K1 0F 06 41 C7 08 01 26 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:..AG..&#################  //K1 Responds      0F  command 06            41 C7 08 01  //08C741 is the number written on the chip sticker - 01=version
			*/

			bIsScanning--;
			if (bIsScanning == 0)
			{// our turn...

				byte addr = (b0 & 0x0f);
				// if (addr == 0x0f)
				{ // ACK
					byte r[8];
					r[0] = 0x1F;		 // Always the Global address          15    //Sometimes this is 0x1F ?why?
					r[1] = b1;			 // repeat command                      6
					r[2] = RKPSerial[0]; /// Our Unique ID (lBf)       41
					r[3] = RKPSerial[1]; //                           C7
					r[4] = RKPSerial[2]; //                           08
					r[5] = 0x01;		 // version #1
					r[6] = (byte)(r[0] + r[1] + r[2] + r[3] + r[4] + r[5]);

					delay(16); // From Scope - 40ms from end of request to start of 07 command

					SendToPanel(r, 7);
					bSent = true;

					LogLn("Waiting for our new ID");
					bIsScanning = -1;
					bWaitingNewID = true;
				}
			}
			else
			{
				bWaitingNewID = false;
				LogLn("Not our turn");
			}
		}
		else if (b1 == 0x07 /*&& bWaitingNewID == true*/)
		{
			/* Command#7: Assign ID
			P  9F 07 01 41 C7 08 01 B8 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:...AG..8################  //Panel sends      9F          07         01 41 C7 08 01  //Assign 01 to RKP
			K1 11 07 00 18 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --:....####################  //K1 Responds      11(RKP1)    07         00              //k1 says ok
			*/

			// LogLn("Set new ID");
			// Log(buf[3]);Log("=");LogLn(RKPSerial[0]);
			// Log(buf[4]);Log("=");LogLn(RKPSerial[1]);
			// Log(buf[5]);Log("=");LogLn(RKPSerial[2]);
			// LogLn("");

			//if (buf[2] == 0)
			//{ // We dont want to become RKP#0 - that should be a real keypad
			//	LogLn("Nah!");
			//}
			//else
			//{
				if (buf[3] == RKPSerial[0] && buf[4] == RKPSerial[1] && buf[5] == RKPSerial[2])
				{ // Serials match - change our ID
					if (buf[2] == 0)
					{ // We dont want to become RKP#0 - that should be a real keypad
						LogLn("ESP32 not to be RKP0");
					}
					else
					{
						RKPID = buf[2];
						byte r[5];
						r[0] = (b0 & 0x30) | RKPID; // 8x 9x Ax bX -> 0x 1x 2x 3x - where x is the RKPID
						r[1] = b1;
						r[2] = 0x00; //? Always 0
						r[3] = r[0] + r[1] + r[2];

						delay(18); // From Scope - 40ms from end of request to start of 07 command

						SendToPanel(r, 4);
						bSent = true;
						Log("New ID Assigned to Us#");
						LogLn(RKPID);
						bWaitingNewID = false;
					}
				}
				else
				{
					LogLn("Not Our Serial");
				}
			//}
		}

	} // #if is from panel

	return true;
}

// Actually sends the message
void RKPClass::SendToPanel(byte *r, int len)
{
	//Send first - need get reply out quick
	digitalWrite(LED_Stat, HIGH);
	timeToSwitchOffLed = millis() + 50;
	if (len > 0)
		swSer.write((char*)r, 1, SWSERIAL_PARITY_MARK);
	if (len > 1)
		swSer.write((char*)(r+1), len-1, SWSERIAL_PARITY_SPACE);
#ifdef debug_send
	Log("A");
	if (RKPID == 0xFF)
		Log("?");
	else
		Log(RKPID);
	Log(">");
	LogHex(r, len); // Log((b0&0x10)==0?0:1);
#endif
}

// a 6 character keyboard buffer
byte FIFO::raw[maxkeybufsize];
FIFO::FIFO()
{
	nextIn = nextOut = count = 0;
}
void FIFO::push(byte element)
{
	if (count >= maxkeybufsize)
	{
		Log("Too Full. Count=");
		LogLn(count);
		return; // lost
	}
	count++;
	raw[nextIn++] = element;
	nextIn %= maxkeybufsize;
	Log("Added Item. Count=");
	LogLn(count);
}
byte FIFO::pop()
{
	if (count > 0)
	{
		count--;
		byte c = raw[nextOut++];
		nextOut %= maxkeybufsize;

		return c;
	}
	return 0xFF;
}
