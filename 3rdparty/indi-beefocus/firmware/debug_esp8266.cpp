#include <ESP8266WiFi.h>
#include "debug_esp8266.h"

DebugESP8266::DebugESP8266( void )
{
	Serial.begin( 115200 );
}

void DebugESP8266::rawWrite( const char* bytes, size_t num )
{
	Serial.write( (uint8 *) bytes, num );
}

