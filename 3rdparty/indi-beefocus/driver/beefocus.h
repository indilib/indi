/*******************************************************************************
  Based on work Copyright(c) 2012 Jasem Mutlaq. All rights reserved.
  Modifications Copyright(c) 2018 Andrew Brownbill. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "beesimfirmware.h"
#include "indifocuser.h"
#include "indicom.h"
#include "unistd.h"
#include "focuser_state.h"    // From the firmware.
#include <memory>
#include <string.h>
#include <string>
#include <unordered_map>
#include <functional>
#include "beeconnect.h"

namespace BeeFocused {

///
/// @brief Simple Hash Function Template for C++ 11 Enum Classes
///
struct EnumHash
{
  // @brief Type convert any enum type to a size_t for hashing
  template <class T>
  std::size_t operator()(T enum_val) const
  {
    return static_cast<std::size_t>(enum_val);
  }
};

///
/// @brief What's the Focuser Hardware doing right now?
///
enum class Mode
{
  /// @brief  Not connected yet!
  UNCONNECTED,
  /// @brief  Idle and accepting new commands
  ACCEPT_COMMANDS,
  /// @brief  Moving to a new positive
  MOVING,
  /// @brief  Looking for the "home" end stop so it can sync (if supported)
  HOMING,
  /// @brief  Idle and accepting new commands (low power mode)
  LOW_POWER,
  /// @brief  Error Error Error!  Hopefully we never see this.
  ERROR,
};

/// @brief Map Focuser Mode to a user readable messages
using ModeToString = 
  std::unordered_map< const Mode, const std::string, EnumHash >;

/// @brief Map Focuser Mode to a user readable string.
extern const ModeToString  stateFriendlyName;

/// @brief Map Firmware SStatus String to the Focuser Mode. 
using StringToMode = 
  std::unordered_map< const std::string, const Mode, std::hash<std::string> >;

/// @brief Map Firmware SStatus String to the Focuser Mode. 
extern const StringToMode  focuserSStatusToMode;

/// @brief How much time should we wait between Timer Hits?
constexpr unsigned int AdvanceOnTimerHit=250;

///
/// @brief A very basic optional value template
///
/// In the spirit of boost::optional, but more basic
///
template<class T> class Optional
{
  public:

  /// @brief Default Constructor - value not set.
  Optional( ) : isValueSet{ false }, value{ T() }
  {
  }

  /// @brief Make an optional from a value
  Optional( const T& valueRHS ) : isValueSet{ true }, value{ valueRHS }
  {
  }

  /// @brief   Get the optional's value.
  /// @return  The value.  Asserts if the value doesn't exist.
  T operator*() const
  {
    assert( isValueSet );
    return value;
  }
  
  /// @brief   Does the optional have a value?
  /// @return  true if a value exists.
  explicit operator bool() const
  {
    return isValueSet;
  }

  private:
  bool isValueSet;
  T value;
};

///
/// @brief Class that reads & records input from the Focuser
///
class HardwareState
{
  public:

  ///
  /// @brief Read new status from the Connection
  ///
  /// @param[in] connection - The interface to the connection.
  ///   This may be a TCP/IP connection to the actual hardware
  ///   or an interface to the Simulated Focuser.
  ///
  /// - Checks the connection for input
  /// - Records whatever it finds in this class.
  ///
  HardwareState( BeeFocusedCon::Interface* connection );

  ///
  /// @brief Create an "Unconnected" hardware state.
  ///
  /// For when there's no value connection.
  ///
  HardwareState() 
  {
    mode = Mode::UNCONNECTED;
  }

  /// @brief    What mode (if any) did the focuser send?
  /// @return   The focuser's mode
  Optional<Mode> getMode() const { return mode; }
  /// @brief    What sync status (if any) did the focuser send?
  /// @return   True if the focuser is Synced, false otherwise
  Optional<bool> getIsSynced() const { return isSynced; }
  /// @brief    What position status (if any) did the focuser send?
  /// @return   The Current Position 
  Optional<unsigned int> getPosition() const { return currentPos; }
  /// @brief    What is the focuser absolute maximum position?
  /// @return   The Current Position 
  Optional<unsigned int> getMaxAbsPos() const { return maxAbsPos; }

  private:

  Optional<Mode> mode;
  Optional<bool> isSynced;
  Optional<unsigned int> currentPos;
  Optional<unsigned int> maxAbsPos;
};

///
/// @brief The INDI BeeFocus Driver Class 
///
class Driver: public INDI::Focuser
{
  public:
    Driver();
    virtual ~Driver() = default;

    // INDI device interface starts here

    const char *getDefaultName() override;
    bool initProperties() override;
    void ISGetProperties(const char *dev) override;
    bool AbortFocuser() override;
    bool SyncFocuser( uint32_t ticks ) override;
    bool updateProperties() override;
    bool Connect() override;
    bool Handshake() override;
    bool Disconnect() override;

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    IPState MoveAbsFocuser(uint32_t targetTicks) override;
    IPState MoveRelFocuser(FocusDirection dir, uint32_t targetTicks) override;
    void TimerHit() override;

  private:

    ///
    /// @brief Given new information from the focuser, update status.
    ///
    /// @param[in] stateChanges - Information from the focuser.
    ///
    /// Takes information from the focuser, in the form of stateChanges,
    /// and updates the INDI status fields.
    ///
    void UpdateStatusInfo( const HardwareState& stateChanges );

    ///
    /// @brief   Get connection status in displayable form
    ///
    /// @return  If Connection exists, gets Connection's status.  If 
    ///          Connection doesn't exist returns "Not Connected"
    ///
    std::string getConStatus() const 
    {
      return Connection ? Connection->GetStatus() : std::string("Not Connected");
    }

    /// @brief Status of the Connection
    IText CStatusInfoT[1] {};
    /// @brief Status of the Connection
    ITextVectorProperty CStatusInfoTP;

    /// @brief Status of the Focuser
    IText FStatusInfoT[2] {};
    /// @brief Status of the Focuser
    ITextVectorProperty FStatusInfoTP;

    /// @brief Should we ignore the next status update packet?  Wee hacky.
    bool ignoreNextStatusUpdate;
    /// @brief Timer ticks since the Focuser started.
    int timerTicks;

    /// @brief Abstract interface to the focuser (simulated or real)
    std::unique_ptr<BeeFocusedCon::Interface> Connection;

    //////////////////////////////////////////////////////////////////
    // 
    // Simulated BeeFocus Focuser starts here.  Used for testing
    //
    //////////////////////////////////////////////////////////////////

    /// @brief The connection to the focuser.
    std::unique_ptr<Connection::Sim> simConnection;

    /// @brief Handshake callback for the simulated focuser's connection
    bool mockCallHandshake( void );

    /// @brief Simulated focuser.  Simulates using the real hardware Firmware.
    std::unique_ptr<BeeSimFirmware> simFirmware;

    /// @brief Outgoing data "pipe" to the focuser
    BeeFocusedCon::QueueOfChar toFirmware;

    /// @brief Incoming data "pipe" from the focuser
    BeeFocusedCon::QueueOfChar fromFirmware;
};

///
/// @brief Break a string into Tokens
///
/// @param[in] text  - Input String.
/// @return            Tokenized Output
///
/// See unit testing for usage examples.
/// 
std::vector<std::string> Tokenize( const std::string& text );

};


