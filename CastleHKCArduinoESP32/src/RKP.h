/*
 * RKP.h
 *
 * Created: 3/30/2014 11:18:18 PM
 */ 

#ifndef RKP_h
#define RKP_h

#include "Arduino.h"

#define maxkeybufsize 8
#define DISP_BUF_LEN 16+1+2		//16 characters - space - AW - and +1 will be added for Terminator0

class FIFO
{
	public:
		FIFO();
		byte pop();
		void push( byte b );
	private:
		int nextIn;
		int nextOut;
		int count;
		static byte raw[maxkeybufsize];
};


class RKPClass
{
 private:
	static FIFO fifo;
	static void SerialInit();
	static bool ReplyToPanel(byte* buf, int nBufLen);
	static inline int GetExpectedMsgLen(byte* buf, int nBufMax);

public:
	void static Poll();

	static char dispBufferLast[]; //store of the display to send out
	
	//TODO: move private
	void static SendItems();
	//static char NextKeyPress();
	static char PopKeyPress();
	static void PushKey( char param1 );
	RKPClass();
	static void Init();
	static void SendToPanel(byte* r, int nLen);  //send a message to the panel
	static void SendToPanelEx(byte* r, int len);
	static void SendToPanel(bool bAck );
	static char dispBuffer[];
		
	static bool dateFlash;
	
	static bool mbIsPanelWarning;
	static bool mbIsPanelAlarm;
	static unsigned long timeToSwitchOffLed;
	static byte lastkey;

};

#endif
