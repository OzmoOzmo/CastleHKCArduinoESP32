/*
 * WebSocket.cpp - Websocket Implementation - works on most modern browsers and Mobile Phones
 *
 * Created: 3/30/2014 9:57:39 PM
 * Modified:6/02/2023 Modified For ESP32
 *
 *   HKC Alarm Panel Arduino Internet Enabled Keypad
 *
 *
 *   See Circuit Diagram for wiring instructions
 *
 *   Author: Ambrose Clarke
 *
 *   See: http://www.boards.ie/vbulletin/showthread.php?p=88215184
 *
 *   Modified:6/02/2023 For ESP32
 * 
*/

#include "Arduino.h"
#include "WebSocket.h"
#include "LOG.h"

int WebSocket::nConnectState = WIFI_DOWN; //0 = no wifi  1= wificonnecting 2=wifi+sockets ok
String WebSocket::sIPAddr = ""; //for reporting and Alexa
String WebSocket::escapedMac = "";

#ifdef WEBSERVER
static const char* TAG = "WS";
#include "RKP.h"  //for PushKey
#include <esp_http_server.h>
#include <lwip/sockets.h>
#include <WiFi.h>

httpd_handle_t WebSocket::server = NULL;

#ifdef ALEXA
	#include "Alexa.h"
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifdef HTTPS
#define WS "wss"
#else
#define WS "ws"
#endif

const String htmlSite =
"<!DOCTYPE html>"
"<html><head><title>Castle ESP32 HKC</title>"
"<meta name='viewport' content='width=320, initial-scale=1.8, user-scalable=no'>" //"no" as screen unzooms when press button
    "<style>\n"
        ".long {height: 64px;}\n"
        "button {height: 35px;width: 35px;}\n"
        ".led {height: 10px;width: 10px;margin-top:3px;margin-bottom:3px;margin-left:1px;border-radius: 10px;border:1px solid;}\n"
        ".ledx {background-color: #FFFFFF !important;}\n"
        ".ledg {background-color: #77FF77;border-color: #55CC55;}\n"
        ".ledy {background-color: #FFDD77;border-color: #CCBB55;}\n"
        ".ledr {background-color: #EE2222;border-color: #CC0000;}\n"
    "</style>"
"</head><body>"
"<table id=table border=0>\n"
	"<tr>"
		/*"<td><span id='ledg' class='led ledg'>&nbsp;</span><span id='ledy' class='led ledy ledx'>&nbsp;</span><span id='ledr' class='led ledr'>&nbsp;</span></td>"
		"<td colspan='3'><div style='border: 5px solid black;width: 130px;display: flex'>&nbsp;<div id='msg1'>Not Connected</div>&nbsp;</div></td>"
		*/
		"<td colspan='4' >"
		"<div style='border: 4px solid black;width: 180px;display: flex;'>"
		"<div id='ledg' class='led ledg ledx float:right'>&nbsp;</div>"
		"<div id='ledy' class='led ledy ledx'>&nbsp;</div>"
		"<div id='ledr' class='led ledr ledx'>&nbsp;</div>"
		"&nbsp;<div id='msg1'>Not Connected</div>&nbsp;"
		"</div></td>"
	"</tr>"
	"<tr><td><button>1</button></td><td><button>2</button></td><td><button>3</button></td><td rowspan='2'><button class='long'>Q</button></td></tr>\n"
	"<tr><td><button>4</button></td><td><button>5</button></td><td><button>6</button></td></tr>\n"
	"<tr><td><button>7</button></td><td><button>8</button></td><td><button>9</button></td><td><button>Y</button></td></tr>\n"
	"<tr><td><button value='*'>" KEY_STAR "</button></td><td><button>0</button></td><td><button value='#'>" KEY_POUND "</button></td><td><button>N</button></td></tr>"
	"</tr>"
"</table>"
"<script defer>var ws;"
"function ge(x){return document.getElementById(x);}\n"
"function st(x,g,y,r){"
  "ge('msg1').innerText=x;"
  "ge('ledg').className='led ledg'+(g=='1'?'':' ledx');"
  "ge('ledy').className='led ledy'+(y=='1'?'':' ledx');"
  "ge('ledr').className='led ledr'+(r=='1'?'':' ledx');"
"}\n"
"try{"
"ws = new WebSocket('" WS "://'+location.hostname+'/ws');"
"ws.onmessage = function(evt){\n"
"var d=evt.data;st(d.substring(3),d[0],d[1],d[2]);}\n"
"ws.onerror = function(evt){st('ERR:' + evt.data,'','','');}\n"
"ws.onclose = function(evt){st('Connection Closed','','','');}\n"
//pc keyboard support
"document.body.onkeydown = function(e){ws.send(String.fromCharCode(e.keyCode));}\n"
//buttons on html
"ge('table').onclick = function(e){ws.send(e.target.value || e.target.innerText);}\n"
"} catch(ex) {alert(ex.message);}\n"
"</script></body></html>";

/*move leds outside of window
<td><span id="ledg" class="led ledg">  </span><span id="ledy" style="" class="led ledy ledx">&nbsp;</span><span id="ledr" class="led ledr">&nbsp;</span></td>
<td colspan="3"><div style="border: 5px solid black;width: 130px;display: flex;">&nbsp;<div id="msg1">  System Armed  </div>&nbsp;</div></td>
*/


/* Root HTTP GET handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	const char* pStr = htmlSite.c_str();
	httpd_resp_sendstr(req, pStr);
    return ESP_OK;
}


//For WS: Structure holding server handle and internal socket fd in order to use out of request send
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};


//called from queue async when its time to send message
static void ws_async_send(void* arg)
{
	static const char* data = RKPClass::dispBuffer;
	struct async_resp_arg* resp_arg = (async_resp_arg*)arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;
	httpd_ws_frame_t ws_pkt;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t*)data;
	ws_pkt.len = DISP_BUF_LEN; //strlen(data);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	httpd_ws_send_frame_async(hd, fd, &ws_pkt);
	free(resp_arg);
}

//Websocket Start Handler
static esp_err_t ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (unsigned char*)(void*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        // Set max_len = ws_pkt.len to get the frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s (%d).", ws_pkt.payload, ws_pkt.payload[0]);
		RKPClass::PushKey((char)ws_pkt.payload[0]);
    }
    /*ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        return trigger_async_send(req->handle, req);
    }
	*/
	free(buf);
	//At Initialisation - Send initial screen (last display message we got from panel)
	ws_pkt.payload = (byte*)RKPClass::dispBufferLast;
	ws_pkt.len = strlen(RKPClass::dispBufferLast);
    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
		return ret;
    }

	
	{//this calls - static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
		struct async_resp_arg* resp_arg = (async_resp_arg*)(void*)malloc(sizeof(struct async_resp_arg));
		resp_arg->hd = req->handle; //same as ::server
		resp_arg->fd = httpd_req_to_sockfd(req); //]need keep fd - this this is req->aux->sd->fd
		esp_err_t ret = httpd_queue_work(WebSocket::server, ws_async_send, resp_arg);

		Logf("New fd is %d\n", resp_arg->fd);
	}

	FlagDisplayUpdate();
	return ret;
}

//this is the favicon image
static esp_err_t image_get_handler(httpd_req_t *req)
{
	// Set Content-Type
	httpd_resp_set_hdr(req, "Content-Type", "image/vnd.microsoft.icon");
	// Binary data for the favicon
	const char FAVICON_DATA[] = 
	{
		0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x08, 0x09, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0x78, 0x00,
		0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x12, 0x00,
		0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0xc1, 0x1e,
		0x00, 0x00, 0xc1, 0x1e, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x9c, 0x00, 0x00, 0x00, 0x9c, 0x00, 0x00, 0x00, 0x88, 0x00,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xa2, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0xa2, 0x00,
		0x00, 0x00, 0xeb, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
		
    //httpd_resp_set_hdr( req, "Connection", "close"); //get chrome to close unneeded sockets (doesnt work)
	#define FAVICON_LENGTH (sizeof(FAVICON_DATA)/sizeof(FAVICON_DATA[0]))
    httpd_resp_send(req, FAVICON_DATA, FAVICON_LENGTH);

    return ESP_OK;
}

static const httpd_uri_t root = {
	.uri       = "/",
	.method    = HTTP_GET,
	.handler   = root_get_handler,
	.user_ctx  = NULL
};

static const httpd_uri_t ws = {
	.uri        = "/ws",
	.method     = HTTP_GET,
	.handler    = ws_handler,
	.user_ctx   = NULL,
	.is_websocket = true
};

static const httpd_uri_t icon = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = image_get_handler,
    .user_ctx  = NULL
};

#ifdef ALEXA
static const httpd_uri_t alexadesc = {
	.uri       = "/description.xml", //xml to send that describes our Alexa Support
	.method    = HTTP_GET,
	.handler   = alexaServeDescription, //this is in Alexa.cpp
	.user_ctx  = (void *)"<xml>" 
};

static const httpd_uri_t alexaroot = {
	.uri       = "/", //xml to send that describes our Alexa Support
	.method    = HTTP_POST, //POST I think...
	.handler   = alexaHandleApiCall, //this is in Alexa.cpp
	.user_ctx  = (void *)"<xml>" 
};
static const httpd_uri_t alexarootGet = {
	.uri       = "/", //xml to send that describes our Alexa Support
	.method    = HTTP_GET, //POST I think...
	.handler   = alexaHandleApiCall, //this is in Alexa.cpp
	.user_ctx  = (void *)"<xml>" 
};
#endif

esp_err_t fnSocketOpen(httpd_handle_t hd, int sockfd)
{//here when socket opens - hd will be same as WebSocket::server
    Logf("Open Socket: hd = %p sockfd = %d \n", hd, sockfd);
    return ESP_OK;
}
void fnSocketClose(httpd_handle_t hd, int sockfd)
{//here when socket closes
    printf("Close Socket: hd = %p sockfd = %d\n", hd, sockfd);
}

//initialise web server - wifi may not be up yet
void WebSocket::ServerInit()
{
	LogLn("===");
    server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true; // added for webserver
	config.server_port = IP_P;
	config.max_open_sockets = 10; // defaults to 7
	config.open_fn = fnSocketOpen;
    config.close_fn = fnSocketClose;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting app");
    ESP_ERROR_CHECK(esp_netif_init()); //todo: needed?
    ESP_ERROR_CHECK(esp_event_loop_create_default()); //todo: needed?

	Logf("Starting server on port: %d\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);

        // Set URI handlers for HTTP webpages
        ESP_LOGI(TAG, "Registering WWW handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &icon);

        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif

		#ifdef ALEXA

		WebSocket::escapedMac = WiFi.macAddress(); //eg. "30:AE:A4:27:84:14";
		LogLn("mac raw: " + WebSocket::escapedMac); //30:AE:A4:27:84:14
		WebSocket::escapedMac.replace(":", "");
		WebSocket::escapedMac.toLowerCase();
		//WebSocket::escapedMac[0] = DEVICEHUB_ID; //manual change to mac to make it unique again to alexa
		LogLn ("Mac: " + WebSocket::escapedMac);

		//]void serveDescription(httpsserver::HTTPRequest * _req, httpsserver::HTTPResponse * res);
		//]void handleAlexaApiCall(httpsserver::HTTPRequest * _req, httpsserver::HTTPResponse * res);
		//]httpd_register_uri_handler(server, new httpsserver::ResourceNode("/description.xml", "GET", &serveDescription));
		//]secureServer->setDefaultNode(new httpsserver::ResourceNode("", "", &handleAlexaApiCall));
		httpd_register_uri_handler(server, &alexaroot);
		httpd_register_uri_handler(server, &alexarootGet);
		httpd_register_uri_handler(server, &alexadesc);
		#endif

    	ESP_LOGI(TAG, "WebSvr Started");
        return;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return;

}

//Send something to connected browser
bool WebSocket::WebSocket_send()
{
	//LogLn("Send");
	gClients = 0;
	for (int i=0; i<CONFIG_LWIP_MAX_SOCKETS; ++i)
	{
		struct sockaddr addr; //?struct sockaddr_in6 addr;
		socklen_t addr_size = sizeof(addr);
		int sock = LWIP_SOCKET_OFFSET + i;
		int res = getpeername(sock, &addr, &addr_size);
		if (res == 0) {
			//	//ESP_LOGI(TAG, "sock: %d -- addr: %x, port: %d", sock, addr.sin6_addr.un.u32_addr[3], addr.sin6_port);        
			//	//ESP_LOGI(TAG, "Addr: %s Family: %d", addr.sa_data, (int)addr.sa_family_t);
			//	ESP_LOGI(TAG, "sock: %d Family: %d", sock, (int)(addr.sa_family));
		
			//this calls - static esp_err_t trigger_async_send(..) which calls httpd_ws_send_frame_async
			struct async_resp_arg* resp_arg = (async_resp_arg*)(void*)malloc(sizeof(struct async_resp_arg));
			resp_arg->hd = WebSocket::server;
			resp_arg->fd = sock; //this is req->aux->sd->fd
			esp_err_t ret = httpd_queue_work(WebSocket::server, ws_async_send, resp_arg);
			gClients++;
		}
		
		/*{//todo: code to send it right now.. possibly faster response?
		    httpd_ws_frame_t ws_pkt;
			uint8_t *buf = NULL;
			memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
			ws_pkt.type = HTTPD_WS_TYPE_TEXT;
			ws_pkt.payload = (byte*)RKPClass::dispBuffer;
			ws_pkt.len = DISP_BUF_LEN; //strlen(RKPClass::dispBuffer); //16+1+2+(/0)
			
			//if (httpd_ws_send_frame(WebSocket::server, &ws_pkt) != ESP_OK)
			if (httpd_ws_send_frame_async(WebSocket::server, sock, &ws_pkt) != ESP_OK)
			{
				ESP_LOGE(TAG, "httpd_ws_send_frame failed");
				closesocket(sock);
			}
		}*/
	}
	FlagDisplayUpdate(); //update number of connected websockets
	return true;
}

//this is called constantly to service network requests
void WebSocket::EtherPoll()
{
	if (nConnectState < WIFI_OK)
		//No web server just yet
		return;

	if (nConnectState != WIFI_PENDING && WiFi.status() != WL_CONNECTED) {
		LogLn("EtherPoll-Wifi reconnect");
		nConnectState = WIFI_DOWN; //switch to retry connecting to wifi again...
		return;
	}

#ifdef ALEXA
	AlexaLoop();
#endif
}

#else //No WebServer...

void WebSocket::EtherPoll(){}

bool WebSocket::WebSocket_send(){
	return true;
}

void WebSocket::ServerInit(){}

#endif


#ifdef WIFI
	void WebSocket::StartWebServerMonitor()
	{
		xTaskCreatePinnedToCore(
			[](void* parameter) {
				while (true)
				{
					WebSocket::Verify_WiFi();
					delay(500);
					#ifdef REPORT_STACK_SPACE
					Logf("threadWIFI %d\n", uxTaskGetStackHighWaterMark(NULL));
					#endif
				}
			},
			"threadWIFI",     // Task name
			5000,             // Stack size (bytes)
			NULL,             // Parameter
			1,                // Task priority
			NULL,             // Task handle
			1                 // ESP Core
		);

		xTaskCreatePinnedToCore(
			[](void* parameter)
			{
				while (true)
				{
					WebSocket::EtherPoll();
					delay(75);

					#ifdef REPORT_STACK_SPACE
					static int n = 0;
					if (n++ % 10 == 0)
						Logf("threadHTTPS %d\n", uxTaskGetStackHighWaterMark(NULL));
					#endif
				}
			},
			"threadHTTPS",  // Task name
			10000,            // Stack size (bytes)
			NULL,             // Parameter
			1,                // Task priority
			NULL,             // Task handle
			1                 // ESP Core
		);

		//Do all LCD updates in Wifi/WebServer/UI Thread
		xTaskCreatePinnedToCore(
			[](void* parameter)
			{
				while (true)
				{
					if (bDisplayToSend)
					{
						bDisplayToSend = false;
						DisplayUpdateDo();
					}

					if (bWebSocketToSend)
					{
						bWebSocketToSend = false;
						WebSocket::WebSocket_send();
					}

					delay(10);

					#ifdef REPORT_STACK_SPACE
					static int n=0;
					if (n++ % 10 == 0)
						Logf("threadLCD %d\n", uxTaskGetStackHighWaterMark(NULL));
					#endif
				}
			},
			"threadLCD",  // Task name
				3000,     // Stack size (bytes)
				NULL,     // Parameter
				1,        // Task priority
				NULL,     // Task handle
				1         // ESP Core
			);
	}


	// Start process to join a Wifi connection
	void WebSocket::WebSocket_WiFi_Init()
	{
		gWifiStat = "Wifi Init..." WIFI_SSID; FlagDisplayUpdate();

		//Wifi Password is defined in config.h
		IPAddress ip_me; ip_me.fromString(IP_ME);
		IPAddress ip_gw; ip_gw.fromString(IP_GW);
		IPAddress ip_sn; ip_sn.fromString(IP_SN);
		IPAddress ip_dns; ip_dns.fromString(IP_DNS);
		
		WiFi.config(ip_me, ip_gw, ip_sn, ip_dns, ip_dns);
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		nConnectState = WIFI_PENDING;
		LogLn("End EtherInit");
	}

	void WebSocket::GetNtpTime()
	{
		LogLn("Retrieving time: ");
  		configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
		time_t now = time(nullptr);
		while (now < 24 * 3600)
		{
			Logf(".");
			delay(250);
			now = time(nullptr);
		}
		struct tm timeinfo;
  		getLocalTime(&timeinfo);
		LogLn(&timeinfo, " Time: %A, %B %d %Y %H:%M:%S"); //LogLn(now);
	}

	//here every 500ms or so - check if connected
	void WebSocket::Verify_WiFi()
	{
		static int nErrorCount = 0;
		if (nConnectState == WIFI_DOWN)
		{
			LogLn("Wifi Down - Start Wifi");
			gWifiStat = "Cnt: " WIFI_SSID; FlagDisplayUpdate(); //ToScreen(0, "Connecting: " WIFI_SSID);
			WebSocket::WebSocket_WiFi_Init();
			nConnectState = WIFI_PENDING;
			nErrorCount = 0;
			return;
		}

		if (nConnectState == WIFI_PENDING)
			if (WiFi.status() != WL_CONNECTED)
			{
				if (nErrorCount++ < 10){
					Log(F("Connecting.."));
				}
				else
					nConnectState = WIFI_DOWN;
				return;
			}
			else
			{//just got good wifi - set up socket server & web server
				nConnectState = WIFI_OK;
				LogLn("WiFi connected.");
				
				//display on LCD
				WebSocket::sIPAddr = WiFi.localIP().toString();
				gWifiStat = "IP:" + WebSocket::sIPAddr + ":" + IP_P; FlagDisplayUpdate();

				//]GetNtpTime();

			#ifdef ALEXA
				AlexaStart(server);
			#endif
				return;
			}

		if (WiFi.status() != WL_CONNECTED)
		{//failed to connect - try again
			LogLn(F("Retry connect Wifi"));
			nConnectState = WIFI_DOWN;
			return;
		}
	}
#else
	void WebSocket::StartWebServerMonitor()
	{
		gWifiStat = "Wifi OFF"; 
		FlagDisplayUpdate();
		return;
	}

	void WebSocket::Verify_WiFi(){}

	void WebSocket::WebSocket_WiFi_Init(){}
#endif //no WIFI

