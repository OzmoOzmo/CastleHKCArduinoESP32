#pragma once

#include <Arduino.h>
#include "config.h"

#define TELEGRAM_HOST "api.telegram.org"
#define TELEGRAM_SSL_PORT 443
#define HANDLE_MESSAGES 1

class TelegramAPI {
public:
  //]TelegramAPI(const String& token, Client &client);
  TelegramAPI();
  String sendGetToTelegram(const String& command);
  bool readHTTPAnswer(String &body, String &headers, Client& client);
  bool sendSimpleMessage(const String& chat_id, const String& text, const String& parse_mode);

  String getUpdates();
  bool checkForOkResponse(const String& response);
  String next_message_to_receive;
  unsigned int waitForResponse = 1500;
  
  int last_sent_message_id = 0;
  int maxMessageLength = 1500;
  //]String sFromID; //user we received msg from
  void closeClient();
private:
};
