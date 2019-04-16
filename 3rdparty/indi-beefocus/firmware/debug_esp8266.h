#ifndef __DEBUG_ESP8266_H__
#define __DEBUG_ESP8266_H__

#include "debug_interface.h"

class DebugESP8266: public DebugInterface
{
	public:

	DebugESP8266();
	DebugESP8266( DebugESP8266& other ) = delete;

	void rawWrite( const char *bytes, std::size_t numByes ) override;
};

#endif

