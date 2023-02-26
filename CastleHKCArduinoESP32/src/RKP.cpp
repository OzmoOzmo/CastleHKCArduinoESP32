/*
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
#include "Config.h"
#include <Arduino.h>
#include "RKP.h"
#include "LOG.h"
#include "Websocket.h"
#include "SMTP.h"
#include "SoftwareSerial.h"
#ifdef ENABLE_TELEGRAM
	#include "Telegram.h"
#endif

SoftwareSerial swSer;


#define nSerialBaudKP_RX 1658 //1658 // HKC Baudrate (measured 600uS per bit which is 1667)
#define ixMaxPanel 64 + 5	  // 64+5 bytes enough for a panel message buffer
bool RKPClass::mbIsPanelWarning = false;
bool RKPClass::mbIsPanelAlarm = false;
unsigned long RKPClass::timeToSwitchOffLed = 0;

char RKPClass::dispBuffer[DISP_BUF_LEN + 1] = "Not Connected";
char RKPClass::dispBufferLast[DISP_BUF_LEN + 1] = "RKP Unitialised";
	
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
	swSer.begin(nSerialBaudKP_RX, SWSERIAL_8S1, SERIAL1_RXPIN, SERIAL1_TXPIN, false); // Recommended way to do 9 bit Mark/Space

	if (SERIAL1_TXACTIVE>0){
		swSer.setTransmitEnablePin(SERIAL1_TXACTIVE);
	}
}

void RKPClass::Init()
{
	SerialInit();

	bIsScanning = -1; //we are not currently scanning
	bWaitingNewID = false; //we are not in Add New Devices mode
	RKPID = RKPINVALID;	//we dont know our ID yet
}


char RKPClass::PopKeyPress()
{
	//return (char)fifo.pop();
	char c = (char)fifo.pop();
	//map desktop keyboard presess to mobile phone keys
	if (c == 'f')                              c= '*';	//UP  (* for IPhone)
	else if (c == 'v')                         c = '#';	//DOWN (# for IPhone)
	//?else if (c == 'p')                      c = 0x0d;	//UP + DOWN (Panic)
	else if (c == 'x'||c == ';' || c == 'n')   c = 'N';	//UP + 0 or X(reject) (WAIT on IPhone numpad)
	else if (c == 13||c == '+'||c == 'y')      c = 'Y';	//UP + 0 or *(accept) (+ on IPhone numpad)
	
	//Keys are sent as the following: (msb 3 bits are row - lsb 3 bits are column)
	if (c == '1') return 0x01; //		   001
	if (c == '2') return 0x02; //		   010
	if (c == '3') return 0x04; //		   100
	if (c == '4') return 0x11; //		 10001
	if (c == '5') return 0x12; //		 10010
	if (c == '6') return 0x14; //		 10100
	if (c == 'Q') return 0x18; //		 11000		(not always sent?)
	if (c == '7') return 0x21; //		100001
	if (c == '8') return 0x22; //		100010
	if (c == '9') return 0x24; //		100100
	if (c == 'Y') return 0x28; // 		101000		(not always sent?)
	if (c == '*') return 0x31; //		110001
	if (c == '0') return 0x32; //		110010
	if (c == '#') return 0x34; //		110100
	if (c == 'N') return 0x38; //		111000
	return 0xFF;
}

void RKPClass::PushKey(char key)
{
	// called from html code when button pressed
	fifo.push(key);
}

#define HAL_FORCE_MODIFY_U32_REG_FIELD(base_reg, reg_field, field_val)    \
{                                                           \
    uint32_t temp_val = base_reg.val;                       \
    typeof(base_reg) temp_reg;                              \
    temp_reg.val = temp_val;                                \
    temp_reg.reg_field = (field_val);                       \
    (base_reg).val = temp_reg.val;                          \
}

void RKPClass::Poll()
{
	// Knock off the "We Sent To Panel" Led a few ms after we turn it on
	if (timeToSwitchOffLed != 0 && millis() > 1) //timeToSwitchOffLed
	{
		timeToSwitchOffLed = 0;
		digitalWrite(LED_Stat, LOW);
	}


	#ifdef simpleRXHexDump
	//simple dump hex bytes
	if (swSer.available()){
		static byte buf[100];
		static int bufix = 0;
		int a = swSer.read();
		buf[bufix++] = a;
		if (bufix>=24){
			LogHex(buf, bufix);
			bufix = 0;
		}
	}
	return;
	#endif

	//more complex read of line using packet lengths
	#define MAX_RX_BUFFER 64
	static byte buf[MAX_RX_BUFFER];
	static int bufix = 0;
	static int tiLast = millis();
	
	while (swSer.available())
	{
		tiLast = millis();
		int i = swSer.read();
		bool bParity =  swSer.readParity();
		if (bParity && bufix != 0){
			Log("Junk:");
			LogHex(buf, bufix); 
			bufix=0;
		}
		
		buf[bufix++] = (byte)i;

		if (bufix >= MAX_RX_BUFFER){
			//buffer is too big - silently reduce to make it fit.
			//LogHex(buf, bufix); Log("(Too Long)");
			bufix--;
			memcpy(buf, buf+1, bufix);
		}
		bool bReported=false;
		while(true)
		{
			int msgLen = GetExpectedMsgLen(buf, bufix);
			if (msgLen == bufix)
			{
				// check checksum
				byte ics = 0;
				for (int n = 0; n < (bufix - 1); n++)
					ics += buf[n];
				if (ics == 0)
					ics--;
				if (ics != buf[bufix - 1])
				{
					if (!bReported){
						Log(F("CSF: ")); LogHex(buf, bufix);
						bReported = true;
					}
					bufix--;
					memcpy(buf, buf+1, bufix); //wait for next byte - no need retest will fails cs again without more data
					break;
				}
				else
				{
					delay(50);
					RKPClass::ReplyToPanel(buf, msgLen);
					#ifdef DEBUG_SHOW_ALL_RX
					//Log(bIsPanelMsg ? "P " : "K "); 
					Log("OK: "); LogHex(buf, bufix); 
					//Log(".");
					#endif
					bufix = 0; break; //as only one byte at a time goes into beffer before we test - cannot be anything left in buffer...
				}
			}
			else if (msgLen == -2){
				if (!bReported){
					Log("BAD:"); LogHex(buf, bufix);
					bReported = true;
				}
				bufix--;
				memcpy(buf, buf+1, bufix);
				if (bufix>=3)
					continue;
			}
			break;
		}

		if (bufix > 0 && (millis()-tiLast > 200))
		{
			Log("Timeout: ");
			LogHex(buf, bufix);
			int msgLen = GetExpectedMsgLen(buf, bufix);
			if (msgLen == bufix)
				LogLn("(Good)");
			else
				Logf("(Bad) %d != %d\n", msgLen, bufix);
			bufix = 0;
		}
	}
}

inline int RKPClass::GetExpectedMsgLen(byte* buf, int nBufMax)
{// how long is the message supposed to be?  
	//returns   -2 if Unknown message
	//			-1 if not enough data loaded yet to determine
	bool bIsPanelMsg = ((*buf & 0x80) != 0);
	
	int msglen = -1;
	if (nBufMax < 3)
		return msglen; //not error - just not enough bytes yet

	//byte b0 = buf[0];
	byte b1 = buf[1];
	
	if (bIsPanelMsg)
	{ // message from Main Panel
		if (b1 == 0x00)	//Status Request
			msglen = 3; // P  A1 00 A1
		else if (b1 == 0x01)  //Display message
			msglen = 20; // length is in byte[2] but message is always padded with spaces //P  D0 01 0F 44 65 76 69 63 65 73 20 66 6F 75 6E 64 20 33 20 52 -- -- -- --:P..Devices.found.3.R####
		else if (b1 == 0x02) //Buzzer on/off
			msglen = 4; // P  D0 02 00 D2
		else if (b1 == 0x03) //Leds on/off
			msglen = 5; // P  C0 03 33 3F 35 Possibly to light leds or sound buzzer?
		else if (b1 == 0x04)  //   (bit4)PlayOff|PlayOn|RecordOff|RecordOn|(bit0)LCD backlight
			msglen = 5; // P  C0 04 08 00 CC  happened during unset   D0 04 08 00 DC   D0 04 08 00 DC  on entering unset
		else if (b1 == 0x05)  //Scan mode start
			msglen = 3; // P  D0 05 D5      All devices enter scan mode
		else if (b1 == 0x06)  //Scan mode next
			msglen = 3; // P  8F 06 95      Scan Next
		else if (b1 == 0x07)  //Scan mode assign id
			msglen = 8; // P  9F 07 01 41 C7 08 01 B8 //assign id
		// 0x08 = Device search by serial number
		// 0x09 = Panel request for RKP's information
		// 0x0A = Panel request for RKP's shock sensor levels
		// 0x0B = unused?
		else if (b1 == 0x0C)
			msglen = 4; // P  00 0C 0C    D0 0C 0C E8  RKP Clear screen - happens When you press 0
		else if (b1 == 0x0D)
			msglen = 4; // P  D0 0D FF DC  //on entering eng mode - clear Leds and Buzzer alarms.
		else if (b1 == 0x0E)
			msglen = 4; // P  C0 0E 01 CF  //on unsetting   (comms fault.bat fault)
		else if (b1 == 0x0F)
			msglen = 5; // P C0 0F 00 3F 0E   Leaving eng mode
		else
		{
			//Log("Unknown P Cmd:");
			//LogHex(buf, nBufMax);
			return -2;
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
		// unused codes here
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
			//Log("Unknown K Cmd:");
			//LogHex(buf, nBufMax);
			return -2;
		}
	}

	return msglen;
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
			//byte addr = (b0 & 0x30) >> 4;
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
			dispBuffer[0] = (char)'1'; // always green
			dispBuffer[1] = RKPClass::mbIsPanelWarning ? (char)'1' : (char)'0';
			dispBuffer[2] = RKPClass::mbIsPanelAlarm ? (char)'1' : (char)'0';
			int n = 3;
			for (; n < (16+3); n++) // always 16 characters
			{
				dispBuffer[n] = (buf[n] & 0x7f);	//is Bit 8 blinking? underscore? remove it.
			}
			dispBuffer[n]=0;
			
			//Screen Has Updated
			strcpy(RKPClass::dispBufferLast, dispBuffer); // keep a copy for new websocket connections
			FlagWebsocketUpdate();	// data for websocket has changed
			#ifdef DEBUG_SHOW_DISPLAY
				Log("Screen Update:"); 
				LogLn((char*)dispBuffer);
				LogHex((byte*)dispBuffer, DISP_BUF_LEN+1);
			#endif
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
				// NO RED: D0 03 (0)F 3F 21
				//    RED: C0 03 (3)F 3F 41
				// This is the RED(Led3) - 00=Off,01 or 10 = flash, 11=Led On
				byte b = (buf[2] & 0x30);
				if (b == 0x00)
				{//Red Led Clear
					if (RKPClass::mbIsPanelAlarm == true)
					{
						LogLn(F("Alarm Cleared"));
						RKPClass::mbIsPanelAlarm = false;
						#ifdef ENABLE_TELEGRAM
							Telegram::QueueTelegram("Alarm Condition Cleared OK");
						#endif
					}
				}
				else if (b == 0x30)
				{//Red Led Solid ON (ignore flashings mode)
					if (RKPClass::mbIsPanelAlarm == false)
					{
						RKPClass::mbIsPanelAlarm = true;
						LogLn(F("Alarm!!!!"));
						SMTP::QueueEmail(MSG_ALARM);
					    #ifdef ENABLE_TELEGRAM
						    Telegram::QueueTelegram("Alarm Red Light!");
    					#endif
					}
				}
				b = (buf[2] & 0b1100);
				if (b == 0x00)
				{//Yellow Led Clear
					if (RKPClass::mbIsPanelWarning == true)
					{
						LogLn(F("Warning Cleared"));
						RKPClass::mbIsPanelWarning = false;
						#ifdef ENABLE_TELEGRAM
							Telegram::QueueTelegram("Warning Condition Cleared");
						#endif
					}
				}
				else if (b == 0b1100)
				{//Yellow Led Solid ON
					if (RKPClass::mbIsPanelWarning == false)
					{
						RKPClass::mbIsPanelWarning = true;
						LogLn(F("Warning!!!!"));
						SMTP::QueueEmail(MSG_ALARM);
						#ifdef ENABLE_TELEGRAM
							Telegram::QueueTelegram("Alarm Yellow Light!");
						#endif
					}
				}
				//We could also listen to Green Led On/Off 0b0011
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
		else if (b1 == 0x02 || b1 == 0x04 || b1 == 0x0C || b1 == 0x0D || b1 == 0x0E)
		{ // These are commands we dont need to implement - they all require the same ack to be sent
			//(2 is Buzzer - 4 is Backlight/Record/Play)

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

			// Command 0x02 is buzzer
			// 			00=Buzzer OFF
			// 			01=Buzzer On/Off
			// 			10=Buzzer On/Off
			// 			11=Buzzer Constant ON
			// Command 0x04 controls keypad (x|x|x|PlayOff|PlayOn|RecOff|RecOn|LightOn
			// Command 0x0C: Happens when you press 0 - screen clears //P  00 0C 0C
			// Command 0x0D: Happens when entering engineer mode	//P  D0 0D FF DC
			// Command 0x0F: Happens when leaving engineer mode //P C0 0F 00 3F 0E

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
		{ // AllocateId - handled below in if (bIsPanelMsg) block
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

				//byte addr = (b0 & 0x0f);
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
		}

	} // #if is from panel

	return bSent;
}

// Actually sends the message
void RKPClass::SendToPanel(byte *r, int len)
{
	digitalWrite(LED_Stat, HIGH);
	timeToSwitchOffLed = millis() + 50;

	//Cannot use Hardware uart - no 9bit support.
	if (len > 0){
		//Need to MARK 1st byte
		uint16_t i = r[0]; 
		swSer.write(i, SWSERIAL_PARITY_MARK);
	}
	if (len > 1){
		//Need to SPACE other bytes
		swSer.write(r+1, len-1, SWSERIAL_PARITY_SPACE);
	}
	digitalWrite(LED_Stat, LOW);

#ifdef DEBUG_SHOW_ALL_TX
	Log("Wrote:"); LogHex(r,len); //keep 
	//Log("A");
	//if (RKPID == 0xFF)
	//	Log("?");
	//else
	//	Log(RKPID);
	//Log(">");
	//LogHex(r, len); // Log((b0&0x10)==0?0:1);
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
	Log("Added Keypress. Count="); //]shorten text
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
