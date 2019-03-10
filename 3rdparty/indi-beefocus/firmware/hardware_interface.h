#ifndef __HARDWARE_INTERFACE_H__
#define __HARDWARE_INTERFACE_H__

#include <unordered_map>
#include <string>
#include "basic_types.h"

struct EnumHash
{
  // @brief Type convert any enum type to a size_t for hashing
  template <class T>
  std::size_t operator()(T enum_val) const
  {
    return static_cast<std::size_t>(enum_val);
  }
};

class HWI
{
  public:

  enum class Pin {
    START_OF_PINS = 0,
    STEP = 0,
    DIR,
    MOTOR_ENA,
    HOME,
    END_OF_PINS 
  };

  enum class PinState {
    START_OF_PIN_STATES = 0,
    STEP_ACTIVE = 0,
    STEP_INACTIVE,
    DIR_FORWARD,
    DIR_BACKWARD,
    MOTOR_ON,       // 0 on the nema build
    MOTOR_OFF,      // 1 on the nema build
    HOME_ACTIVE,    // 0 on the nema build
    HOME_INACTIVE,  // 1 on the nema build
    END_OF_PIN_STATES
  };

  // Arduino.h uses #define to redefine OUTPUT and INPUT,  so use
  //           M_OUTPUT and M_INPUT here.  Thanks Obama.
  enum class PinIOMode {
    START_OF_PIN_IO_MODES = 0,
    M_OUTPUT = 0,
    M_INPUT = 1,
    END_OF_IO_MODES
  };

  const static std::unordered_map<Pin,std::string,EnumHash> pinNames;
  const static std::unordered_map<PinState,std::string,EnumHash> pinStateNames;
  const static std::unordered_map<PinIOMode,std::string,EnumHash> pinIOModeNames;

  virtual void DigitalWrite( Pin pin, PinState state ) = 0;
  virtual void PinMode( Pin pin, PinIOMode mode ) = 0;
  virtual PinState DigitalRead( Pin pin) = 0;
};

// @brief Increment operator for Hardware Interface Pin
//
// @param[in] pin - The pin to increment. 
// @return - The next value in the Enum.
//
inline HWI::Pin& operator++( HWI::Pin &pin ) 
{
  return BeeFocus::advance< HWI::Pin, HWI::Pin::END_OF_PINS >( pin );
}

// @brief Increment operator for Hardware Interface Pin State
//
// @param[in] pinState - The pin state to increment. 
// @return - The next value in the Enum.
// 
inline HWI::PinState& operator++( HWI::PinState &pin ) 
{
  return BeeFocus::advance< HWI::PinState, HWI::PinState::END_OF_PIN_STATES >( pin );
}

// @brief Increment operator for Hardware Interface Pin IO Mode
//
// @param[in] pinIoMode - The pin IO Mode to increment. 
// @return - The next value in the Enum.
// 
inline HWI::PinIOMode& operator++( HWI::PinIOMode &pin ) 
{
  return BeeFocus::advance< HWI::PinIOMode, HWI::PinIOMode::END_OF_IO_MODES >( pin );
}

#endif

