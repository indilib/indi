
#include <memory>
#include "focuser_state.h"
#include "net_esp8266.h"
#include "hardware_esp8266.h"
#include "debug_esp8266.h"

std::unique_ptr<FS::Focuser> focuser;

void loop() {
  unsigned int pause = focuser->loop();
  if ( pause != 0 )
  {
    int ms = pause / 1000;
    int usRemainder = pause % 1000;
    delay( ms );
    delayMicroseconds( usRemainder );
  }
}

void setup() {
  std::unique_ptr<NetInterface> wifi( new WifiInterfaceEthernet );
  std::unique_ptr<HWI> hardware( new HardwareESP8266 );
  std::unique_ptr<DebugInterface> debug( new DebugESP8266 );
  FS::BuildParams params( FS::Build::LOW_POWER_HYPERSTAR_FOCUSER );
  focuser = std::unique_ptr<FS::Focuser>(
     new FS::Focuser( 
        std::move(wifi), 
        std::move(hardware),
				std::move(debug),
        params )
  );
}
