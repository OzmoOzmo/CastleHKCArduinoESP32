/*
 * WebSocket.h
 *
 * Created: 3/30/2014 11:54:30 PM Ozmo
 * Modified:6/02/2023 For ESP32
 */

#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

typedef void* httpd_handle_t;

class WebSocket
{
	private:
		void static WebSocket_WiFi_Init();
		void static EtherPoll();
		void static Verify_WiFi();
		bool static WebSocket_send();//Send display to all connected browsers

	public:
		void static StartWebServerMonitor();
		void static ServerInit(); //initialise what can be before we have wifi

		String static sIPAddr;
		String static escapedMac;
		static httpd_handle_t server;
	    static int nConnectState; //0 = no wifi   1=waiting for wifi   2= wifi+sockets ok
};

#endif /* WEBSOCKET_H_ */
