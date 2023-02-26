// Alexa.h


#include "config.h"
#include <esp_http_server.h> //for responding to alexas requests

#ifdef ALEXA

#ifndef _ALEXA_h
#define _ALEXA_h

void AlexaStart(void* secureServer);
esp_err_t alexaServeDescription(httpd_req_t *req);
esp_err_t alexaHandleApiCall(httpd_req_t *req);
void AlexaLoop();

#endif
#endif //#endif def alexa
