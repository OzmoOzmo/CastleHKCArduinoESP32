// Alexa.h


#include "config.h"

#ifdef ALEXA

#ifndef _ALEXA_h
#define _ALEXA_h

#include <HTTPServer.hpp>

void AlexaStart(httpsserver::HTTPServer* secureServer);

void AlexaLoop();

#endif
#endif //#endif def alexa
