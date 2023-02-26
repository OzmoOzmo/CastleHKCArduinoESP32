// 
// 
// 

#include "config.h"

#ifdef ALEXA

#include <esp_http_server.h> //for responding to alexas requests
#include "Alexa.h"

// Includes for the server
#include <WiFiUdp.h>

#include "Log.h"
#include "WebSocket.h" //for the IPaddress
#include "RKP.h" //for keypress

static const char* TAG = "ALEXA";
bool devValues[]{ false,false};
String devNames[]{ "Home Security", "Home Partset"};
uint8_t currentDeviceCount = sizeof(devValues)/sizeof(devValues[0]);

WiFiUDP espalexaUdp;
IPAddress ipMulti;
bool udpConnected = false;

String deviceJsonString(uint8_t deviceId)
{
	String escapedMac = WebSocket::escapedMac; //this takes a copy
	Serial.println("Using Mac: " + escapedMac);
	
	String buf_lightid = escapedMac.substring(0,10); //first 10 chars
	if(deviceId <= 15)
		buf_lightid += "0";
	buf_lightid += String(deviceId, HEX);
	LogLn("Unique Device: "+ buf_lightid);
	
	String sValue = devValues[deviceId - 1] ? "true" : "false";

	String buf = R"({"state":{"on":)" + sValue 
	+ R"(,"bri":254,"alert":"none","mode":"homeautomation","reachable":true})"
	+ R"(,"type":"Dimmable light","name":")" + devNames[deviceId - 1] + R"(")"
	+ R"(,"modelid":"LWB010","manufacturername":"Philips","productname":"E1")"
	+ R"(,"uniqueid":")" + buf_lightid + R"(")"
	+ R"(,"swversion":"espalexa-2.5.0"})";

	LogLn("==");
	LogLn(buf);
	LogLn("==");

	return buf;
}

esp_err_t alexaServeDescription(httpd_req_t *req)
{
	// Status code is 200 OK by default.
	httpd_resp_set_hdr(req, "Content-Type", "text/xml");
	String sIP = WebSocket::sIPAddr;

	String xml = "<?xml version=\"1.0\" ?>"
		"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
		"<specVersion><major>1</major><minor>0</minor></specVersion>"
		"<URLBase>http://" + sIP + ":80/</URLBase>"
		"<device>"
		"<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
		"<friendlyName>Espalexa (" + sIP + ":80)</friendlyName>"
		"<manufacturer>Royal Philips Electronics</manufacturer>"
		"<manufacturerURL>http://www.philips.com</manufacturerURL>"
		"<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
		"<modelName>Philips hue bridge 2012</modelName>"
		"<modelNumber>929000226503</modelNumber>"
		"<modelURL>http://www.meethue.com</modelURL>"
		"<serialNumber>" + WebSocket::escapedMac + "</serialNumber>"
		"<UDN>uuid:2f402f80-da50-11e1-9b23-" + WebSocket::escapedMac + "</UDN>"
		"<presentationURL>index.html</presentationURL>"
		"</device>"
		"</root>";

	const char* pStr = xml.c_str();
	httpd_resp_sendstr(req, pStr);
    return ESP_OK;
}

void respondToSearch()
{
	String sIP = WebSocket::sIPAddr;

	String buf = "HTTP/1.1 200 OK\r\n"
		"EXT:\r\n"
		"CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
		"LOCATION: http://" + sIP + ":80/description.xml\r\n"
		"SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
		"hue-bridgeid: " + WebSocket::escapedMac + "\r\n"
		"ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
		"USN: uuid:2f402f80-da50-11e1-9b23-" + WebSocket::escapedMac + "::upnp:rootdevice\r\n" // _uuid::_deviceType
		"\r\n";

	espalexaUdp.beginPacket(espalexaUdp.remoteIP(), espalexaUdp.remotePort());
	espalexaUdp.write((uint8_t*)buf.c_str(), strlen(buf.c_str()));
	espalexaUdp.endPacket();
	LogLn("UDPResp: " + buf);
}

esp_err_t alexaHandleApiCall(httpd_req_t *res) 
{
	LogLn("AlexaApiCall");

	//std::string std_req = res->getRequestString(); //httpd_req_get_url_query_len
	String sReq;
	int buf_len = httpd_req_get_url_query_len(res) + 1;
    if (buf_len > 1)
	{
		char buf[buf_len+1];
        if (httpd_req_get_url_query_str(res, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
        }
		sReq=String((char*)buf);
    }
	//]String req(std_req.c_str());

	//get the content - and transfer buffer to a String
	String body;
	{
		size_t nBodyLen = res->content_len; //]-getContentLength();
		char cbody[nBodyLen + 1];
		// Read the body data request
		int ret = httpd_req_recv(res, cbody, nBodyLen);
		//] Log data received
		//]ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
		//]ESP_LOGI(TAG, "%.*s", ret, buf);
		//]ESP_LOGI(TAG, "====================================");
		//]res->readChars(cbody, nBodyLen);
		cbody[nBodyLen] = 0;
		body = String(cbody);
	}
	LogLn("=HTTP REQUEST=");
	LogLn("Req: " + sReq);
	LogLn("Bdy: " + body);

	if (body.indexOf("devicetype") > 0)
	{//client wants a hue api username - any name will do.
		LogLn("devicetype");
		body = "";
		const char szResponse[] = "[{\"success\":{\"username\":\"admin\"}}]";
		httpd_resp_set_hdr(res, "Content-Type", "application/json");
		httpd_resp_sendstr(res, szResponse); //]res->println(sResponse);
		Logf("Rsp1: %s\n", szResponse);
		LogLn("==============");
		return ESP_OK;
	}
	else if ((sReq.indexOf("state") > 0) && (body.length() > 0))
	{//client wants to control light
		//respond quickly...
		static char szResponse[] = "[{\"success\":{\"/lights/1/state/\": true}}]";
		httpd_resp_set_hdr(res, "Content-Type", "application/json");
		httpd_resp_sendstr(res, szResponse); //]res->println(sResponse);
		Logf("Rsp: %s\n", szResponse);
		LogLn("==============");

		uint32_t devId = sReq.substring(sReq.indexOf("lights") + 7).toInt();
		Logf("Light State: %d\n", devId);
		devId--; //zero-based for devices array
		if (devId > currentDeviceCount) {
			LogLn("*Error Incorrect Device %d*\n");
			return ESP_OK;
		}

		if (body.indexOf("false") > 0) //OFF command
		{
			devValues[devId] = false;
			LogLn("**Update: " + devNames[devId] + " " + String(devValues[devId]));
		}
		else if (body.indexOf("true") > 0) //ON command
		{
			devValues[devId] = true;
			LogLn("**Update: " + devNames[devId] + " " + String(devValues[devId]));

			RKPClass::PushKey('0');
			RKPClass::PushKey('1');
			RKPClass::PushKey('1');
			RKPClass::PushKey('1');
			RKPClass::PushKey('1');
		}
		return ESP_OK;
	}
	else
	{
		//Request: GET /api/2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr/lights/1 
		//Request: GET /api/2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr/lights 
		//Request: GET /api/admin/lights/1 
		//int pos = req.indexOf("lights");
		String sResponse="";
		int pNumber = 18;
		int pos = sReq.indexOf("/api/admin/lights");
		if (pos >= 0) //client wants light info
		{
			int devId = sReq.substring(pos + pNumber).toInt();
			if (devId == 0) //client wants all lights
			{
				LogLn("Light: All!");
				sResponse = "{";
				for (int i = 0; i < currentDeviceCount; i++)
				{
					sResponse += "\"" + String(i + 1) + "\":";
					sResponse += deviceJsonString(i + 1);;
					if (i < currentDeviceCount - 1)
						sResponse += ",";
				}
				sResponse += "}";
			}
			else //client wants one light (devId)
			{
				Logf("Light Char: %d\n", devId);
				if (devId > currentDeviceCount)
				{
					Serial.printf("*Error Incorrect Device %d*\n");
				}
				else
				{
					//httpd_resp_set_hdr(res, "Content-Type", "application/json");
					sResponse += deviceJsonString(devId);
				}
			}
		}

		const char* pStr = sResponse.c_str();
		httpd_resp_set_hdr(res, "Content-Type", "application/json");
		httpd_resp_sendstr(res, pStr);
		//]res->finalize();

		//]Serial.println("===");
		//]Serial.println("Req: " + req);
		//]Serial.println("Bdy: " + body);
		Serial.println("Rsp: " + sResponse);
		Serial.println("===");
		return ESP_OK;
	}
}

void AlexaStart(httpd_handle_t secureServer)
{
	LogLn("Start Alexa");
	udpConnected = espalexaUdp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);

	Logf("This hub is Hosting %d Devices\n", currentDeviceCount);
}

void AlexaLoop() {
	if (udpConnected)
	{
		int packetSize = espalexaUdp.parsePacket();
		if (packetSize < 1)
			return; //no new udp packet

		unsigned char packetBuffer[packetSize + 1]; //buffer to hold incoming udp packet
		espalexaUdp.read(packetBuffer, packetSize);
		packetBuffer[packetSize] = 0;

		espalexaUdp.flush();

		const char* request = (const char*)packetBuffer;

		//LogLn("Got UDP: ");
		//LogLn(request);
		//LogLn(";");

		if (strstr(request, "M-SEARCH") == nullptr)
			return;

		if (strstr(request, "ssdp:disc") != nullptr &&  //short for "ssdp:discover"
			(strstr(request, "upnp:rootd") != nullptr || //short for "upnp:rootdevice"
				strstr(request, "ssdp:all") != nullptr ||
				strstr(request, "asic:1") != nullptr)) //short for "device:basic:1"
		{
			LogLn("Responding search req...");
			respondToSearch();
		}
	}
}

#endif //#ifdef alexa