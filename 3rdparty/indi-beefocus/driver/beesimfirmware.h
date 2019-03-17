/*******************************************************************************
  Based on work Copyright(c) 2018 Andrew Brownbill. All rights reserved.

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

#include "focuser_state.h"    // From the firmware.
#include "beeconnect.h"

///
/// Brief Firmware based simulator for the focuser.
///
/// This class takes the Firmware that's loaded into the BeeFocused ESP8266
/// micro-controller make makes a simulator out of it.  If you connect using
/// a "Simulated" connection this is the code that will be used to simulate
/// the focuser.
///
/// Because it's the actual firmware, it's useful for "unit testing".  
/// unit testing is in quotes because it's really an end to end integration
/// test,  but it's an integration test that runs in 1.5 seconds.
///
/// TODO - 1.5 seconds at least an order of magnitude more than I'd expect,
///        find out why.
/// 
class BeeSimFirmware
{
  public:

  // Remove unused interfaces that could be dangerous if used.
  BeeSimFirmware() = delete;
  BeeSimFirmware( const BeeSimFirmware& ) = delete;
  BeeSimFirmware& operator=( const BeeSimFirmware& ) = delete;

  ///
  /// @brief Constructor for the Firmware Based BeeFocused Simulator
  ///
  /// @param[in]  toFirmware
  ///   A "pipe"/ queue of characters that represent data going to the 
  ///   simulated focuser.
  /// @param[out] toFirmware   
  ///   A "pipe"/ queue of characters that represent data coming from the
  ///   simulated focuser.
  /// 
  BeeSimFirmware(
    BeeFocusedCon::QueueOfChar* toFirmware,
    BeeFocusedCon::QueueOfChar* fromFirmware
  );

  ///
  /// @brief Advance Time on the simulator
  ///
  /// Move the simulator forward by msAmountOfTime (which is in ms).  Gets
  /// input and writes output to the Queues passed into the Constructor.
  ///
  void advanceTime( unsigned int msAmountOfTime );

  private:

  /// @brief Current simulated time in ms.
  unsigned int time; 

  /// @brief The Simulated Focuser firmware class
  std::unique_ptr<FS::Focuser> simulatedFocuser;

  /// @brief When's the next time when we can call the loop function on the
  ///        simulatedFocuser class (in microseconds).
  unsigned long simulatedFocuserNextUpdate; 
};
 
