#ifndef __HARDWARE_ARDUINO_H__
#define __HARDWARE_ARDUINO_H__

#include "hardware_interface.h"

class HardwareESP8266: public HWI
{
  public:

  void     DigitalWrite( Pin pin, PinState state ) override;
  void     PinMode( Pin pin, PinIOMode state ) override;
  PinState DigitalRead( Pin pin) override;

  private:
 
  static const std::unordered_map<HWI::Pin, int, EnumHash > pinMap;
  static const std::unordered_map<HWI::PinState, int, EnumHash > pinStateMap;
};

#endif

