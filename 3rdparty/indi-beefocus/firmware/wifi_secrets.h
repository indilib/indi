#ifndef __WIFI_SECRETS_H__
#define __WIFI_SECRETS_H__

//
// This file contains wifi credentials.  Separate file because this 
// information will need to be modified during development, but wifi
// details shouldn't be checked into a public repository.
// 

namespace WifiSecrets {
  // your SSID. Do not check in :)
	constexpr const char* ssid     = "yourssid";	     
  // your Passwoord. Do not check in :)
	constexpr const char* password = "yourpassword";   
}

#endif

