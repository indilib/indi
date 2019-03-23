#include <ESP8266WiFi.h>
#include "hardware_esp8266.h"

const std::unordered_map<HWI::Pin, int, EnumHash > HardwareESP8266::pinMap = {
  { Pin::STEP,        4 },
  { Pin::DIR,         5 },
  { Pin::MOTOR_ENA,   14},
  { Pin::HOME,        13}
};

const std::unordered_map<HWI::PinState, int, EnumHash > HardwareESP8266::pinStateMap = {
  { PinState::STEP_ACTIVE,    HIGH },
  { PinState::STEP_INACTIVE,  LOW  },
  { PinState::DIR_BACKWARD,   LOW  },
  { PinState::DIR_FORWARD,    HIGH },
  { PinState::MOTOR_OFF,      HIGH },    // Active low
  { PinState::MOTOR_ON,       LOW  },    // Active low
  { PinState::HOME_INACTIVE,  HIGH },    // Active low
  { PinState::HOME_ACTIVE,    LOW},      // Active low
};

void HardwareESP8266::DigitalWrite( Pin pin, PinState state )
{
  int actualPin = pinMap.at( pin );
  int actualState = pinStateMap.at( state );
  digitalWrite( actualPin, actualState );
}

void HardwareESP8266::PinMode( Pin pin, PinIOMode mode )
{
  int actualPin = pinMap.at( pin );
  pinMode( actualPin, mode == PinIOMode::M_OUTPUT ? OUTPUT : INPUT );
}

HWI::PinState HardwareESP8266::DigitalRead( Pin pin)
{
  int actualPin = pinMap.at( pin );
  // Tmp Hack.  It's always the home pin, which is active low
  return digitalRead( actualPin ) ? PinState::HOME_INACTIVE : PinState::HOME_ACTIVE;
}

