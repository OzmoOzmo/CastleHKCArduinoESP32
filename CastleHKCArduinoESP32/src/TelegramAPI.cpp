#include "config.h" //for BOT_TOKEN

#ifdef ENABLE_TELEGRAM
#include <Arduino.h>
#include "TelegramAPI.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Log.h"

//#define TELEGRAM_DEBUG 1

// Go Daddy Root Certificate Authority
//- for access to telegram with no errors - valid until end 2037
// alternative is to use client.setInsecure(); certStore or at least client.setFingerPrint
const char* TELEGRAM_CERTIFICATE_ROOT = nullptr;
/*const char TELEGRAM_CERTIFICATE_ROOT[] = R"=EOF=(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx
EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT
EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp
ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz
NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH
EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE
AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD
E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH
/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy
DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh
GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR
tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA
AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE
FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX
WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu
9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr
gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo
2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO
LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI
4uJEvlz36hz1
-----END CERTIFICATE-----
)=EOF=";
*/

//Get string between two tags
String midString(String str, String start, String finish, int& offset){
  int locStart = str.indexOf(start, offset);
  if (locStart==-1) {offset=-1; return "";}
  locStart += start.length();
  int locFinish = str.indexOf(finish, locStart);
  if (locFinish==-1) {offset=-1; return "";}
  offset = locFinish;
  return str.substring(locStart, locFinish);
}

//Get string between two tags (alweays starts at 0 - good in case fields change in telegram response)
String midString(String str, String start, String finish){
  int locStart = str.indexOf(start, 0);
  if (locStart==-1) {return "";}
  locStart += start.length();
  int locFinish = str.indexOf(finish, locStart);
  if (locFinish==-1) {return "";}
  return str.substring(locStart, locFinish);
}

//TelegramAPI::TelegramAPI(const String& token, Client &client)
TelegramAPI::TelegramAPI()
{
}

String TelegramAPI::sendGetToTelegram(const String& command)
{
  String body, headers;
  
  WiFiClientSecure client;
  if (TELEGRAM_CERTIFICATE_ROOT == nullptr)
    client.setInsecure();
  else
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  // Connect with api.telegram.org if not already connected
  if (!client.connected()) {
    #ifdef TELEGRAM_DEBUG  
        LogLn(F("[BOT]Connecting to server"));
    #endif
    if (!client.connect(TELEGRAM_HOST, TELEGRAM_SSL_PORT)) {
      #ifdef TELEGRAM_DEBUG  
        LogLn(F("[BOT]Conection error"));
      #endif
    }
  }
  if (client.connected()) {

    #ifdef TELEGRAM_DEBUG  
        LogLn("sending: " + command); //]sending: bot6210358051:AAHMnu-Ffj_JkrYB8ZCE7Mhoaawsf8AEG1A/getUpdates?offset=421984839&limit=1
    #endif  
    client.print(F("GET /"));
    client.print(command);
    client.println(F(" HTTP/1.1"));
    client.println(F("Host:" TELEGRAM_HOST));
    client.println(F("Accept: application/json"));
    client.println(F("Cache-Control: no-cache"));
    client.println();

    readHTTPAnswer(body, headers, client);

    client.stop();
  }

  return body;
}

bool TelegramAPI::readHTTPAnswer(String &body, String &headers, Client& client) {
  int ch_count = 0;
  unsigned long now = millis();
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  bool responseReceived = false;

  #define longPoll 0

  while (millis() - now < longPoll * 1000 + waitForResponse) {
    while (client.available()) {
      char c = client.read();
      responseReceived = true;

      if (!finishedHeaders)
      {
        if (currentLineIsBlank && c == '\n')

          finishedHeaders = true;
        else
          headers += c;
      } else
      {
        if (ch_count < maxMessageLength) {
          body += c;
          ch_count++;
        }
      }

      if (c == '\n')
        currentLineIsBlank = true;
      else if (c != '\r')
        currentLineIsBlank = false;
    }

    if (responseReceived) {
      #ifdef TELEGRAM_DEBUG  
        LogLn("HTTP Response:"+body);
        LogLn();
      #endif
      break;
    }
  }
  return responseReceived;
}


//GetUpdates - function to receive new messages from telegram
String TelegramAPI::getUpdates()
{

  #ifdef TELEGRAM_DEBUG  
    LogLn(F("GET Update Messages"));
  #endif

  String command = "bot" BOT_TOKEN "/getUpdates?offset="+ next_message_to_receive + "&limit=1";

  String response = sendGetToTelegram(command); // receive reply from telegram.org

  //LogLn("Send:"+command);
  //LogLn("Recv:"+response);

  if (response == "") {
    #ifdef TELEGRAM_DEBUG  
        LogLn(F("Received empty string in response!"));
    #endif
    // close the client as there's nothing to do with an empty string
    closeClient();
    return "";
  }
  else
  {
    //eg. {"ok":true,"result":[]}
    //eg. {"ok":true,"result":[
    //      {"update_id":421984839,"message":{"message_id":9,"from":{"id":5872930423,"is_bot":false,"first_name":"A",
    //        "last_name":"Clarke","language_code":"en"},"chat":{"id":5872930423,"first_name":"A","last_name":"Clarke",
    //        "type":"private"},"date":1676942020,"text":"/ledon","entities":[{"offset":0,"length":6,"type":"bot_command"}]}}
    //    ]}
    
    //Or:
    //Recv:{"ok":true,"result":[{"update_id":421984863,
    //"my_chat_member":{"chat":{"id":5872930423,"first_name":"A","last_name":"Clarke","type":"private"},
    // "from":{"id":5872930423,"is_bot":false,"first_name":"A","last_name":"Clarke","language_code":"en"},"date":1677102562,
    // "old_chat_member":{"user":{"id":6210358051,"is_bot":true,"first_name":"CastleHKCBot","username":"CastleHKCBot"},"status":"member"},
    //  "new_chat_member":{"user":{"id":6210358051,"is_bot":true,"first_name":"CastleHKCBot","username":"CastleHKCBot"},"status":"kicked","until_date":0}}}]}
    //ID:421984863    UsrID:5872930423  TEXT:

    //look for   "result":[{" //then will have results
    if (response.indexOf("\"result\":[{\"") >= 0)
    {//there will be Only 1 Message - as we have Limit set to 1 in query
      int msgCount = 0;
      const String _ID = "\"update_id\":"; //want "update_id":421984839,
      const String _UsrID = "\"from\":{\"id\":"; //want "from":{"id":5872930423,
      const String _TXT= "\"text\":\"";    //want "text":"/ledon",

      String sID = midString(response, _ID, ","); //,p
      LogLn("ID:"+sID);
      if (sID.length()==0) return "";

      next_message_to_receive = String(sID.toInt()+1, 10);
      LogLn("NEXT:"+next_message_to_receive);

      String sFromID = midString(response, _UsrID, ",");  //,p
      LogLn("UsrID:"+sFromID);
      if (sFromID.length()==0) return "";

      //Authorised?
      if (!sFromID.equals(CHAT_ID)){
        LogLn("User Not Authorised");
        return "";
      }


      //Note: there is not always a TEXT: - can be message to say user logs off
      String sMessage = midString(response, _TXT, "\"");  //,p
      LogLn("TEXT:"+sMessage);
      //if (sMessage.length() == 0) return "";

      //we could keep the client open as may be a response to be given
      return sMessage;
    }

    // Close the client as no response is to be given
    closeClient();
    return "";
  }
}

bool TelegramAPI::checkForOkResponse(const String& response) {
  //{"ok":true,"result":{"message_id":43,"from":{"id":6210358051,"is_bot":true,"first_name":"CastleHKCBot","username":"CastleHKCBot"},"chat":{"id":5872930423,"first_name":"A","last_name":"Clarke","type":"private"},"date":1677036456,"text":"Led is ON"}}
  #ifdef TELEGRAM_DEBUG 
  LogLn("Response:"+response);
  #endif
  return response.startsWith("{\"ok\":true,");
}


// send a text message to telegram - chat_id, text to transmit and markup(optional)
bool TelegramAPI::sendSimpleMessage(const String& chat_id, const String& text, const String& parse_mode)
{
  bool sent = false;
  #ifdef TELEGRAM_DEBUG  
    LogLn(F("sendSimpleMessage: SEND Simple Message"));
  #endif
  unsigned long sttime = millis();

  if (text != "") {
    while (millis() - sttime < 8000ul)
    { // loop for a while to send the message
      String command = "bot" BOT_TOKEN "/sendMessage?chat_id="+ chat_id+ "&text="+text+"&parse_mode="+parse_mode;
      
      String response = sendGetToTelegram(command);
      #ifdef TELEGRAM_DEBUG  
        LogLn(response);
      #endif
      if (checkForOkResponse(response)){
        sent=true;
        break;
      }
    }
  }
  closeClient();
  return sent;
}

void TelegramAPI::closeClient()
{
  //if (client->connected()) {
  //  #ifdef TELEGRAM_DEBUG  
  //      LogLn(F("Closing client"));
  //  #endif
  //  client->stop();
  //}
}

#endif //ENABLE_TELEGRAM