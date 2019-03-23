///
/// @brief Connection Interface and Helper Functions.
///
/// File Jobs:
///
/// - Provide an interface to the focuser
/// - Convenience utilities for communicating with the focuser. 
///

/*******************************************************************************
 Copyright(c) 2018 Andrew Brownbill. All rights reserved.

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

#include "indifocuser.h"
#include "indicom.h"
#include "unistd.h"
#include <memory>
#include <string.h>
#include <string>
#include <queue>
#include <unordered_map>
#include <functional>
#include "connectionplugins/connectioninterface.h"

namespace BeeFocusedCon {

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
/// @brief A Queue of Characters.
///
/// Used by the driver to talk to a simulated focuser.
///
using QueueOfChar = std::queue<char>;

///
/// @brief An Interface to a basic IO stream.
///
/// A basic interface to a stream of characters (like a TCP/IP connection)
///
/// Class Jobs:
///
/// - Provide an interface to the focuser that works on both the real
///   focuser hardware and a simulated version of the focuser.  The later is
///   used for testing.
/// - Simplify Error handling and reporting.
///   
class Interface
{
  public:

  Interface() : 
    isFailed { false },
    conStatus { "Connected" }
  {
  }

  /// @brief Has the connection errored out?
  bool Failed( void ) const { return isFailed; }

  /// @brief Put the connection into a failed state
  void Fail( std::string reason ) 
  {
    isFailed = true;
    conStatus = reason;
  }

  /// @brief User friendly connection status.
  virtual std::string GetStatus( void ) const { return conStatus; }

  /// @brief Output a character to the connection.
  virtual Interface& operator<<( char c ) = 0;

  /// @brief Input a character from the connection.
  virtual Interface& operator>>( char &c ) = 0; 

  /// @brief Are there characters ready to be read?
  virtual bool DataReady() = 0;

  protected:

  bool isFailed;
  std::string conStatus;

  private:
};

///
/// @brief A TCP/IP connection to the Focuser.
///
/// Class Jobs:
///
/// - Implements the interface to the focuser hardware.
/// - Error handling and reporting.
///
/// Actual connection setup is done by INDI.  Communicates using INDI's
/// tty_* functions.
///  
class TCP: public Interface
{
  public:

  /// @brief The one true constructor
  TCP( int fd_arg ) : fd{ fd_arg }
  {
  }

  // delete unused constructors for safety
  TCP() = delete;
  TCP( const TCP& ) = delete;
  TCP& operator=( const TCP& ) = delete;

  /// @brief Output a character to the connection.
  Interface& operator<<( char c ) override; 

  /// @brief Input a character from the connection.
  Interface& operator>>( char &c ) override; 

  /// @brief Are there characters ready to be read?
  bool DataReady(void) override;

  private:

  int GetTimeout();

  int fd;
};

///
/// @brief INDI Implementation of a simulated connection.
///
/// - Implements the interface to the simulated focuser
/// - Error handling and reporting.
/// 
/// Communicates with the simulated focuser using shared Queues of 
/// Characters (QueueOfChar), which are passed in.
/// 
class Sim: public Interface
{
  public:

  /// @brief The one true constructor
  Sim( QueueOfChar& toFirmwareRHS, QueueOfChar& fromFirmwareRHS
  ) : toFirmware{ toFirmwareRHS } , fromFirmware{ fromFirmwareRHS } 
  {
  }

  // delete unused constructors for safety
  Sim() = delete;
  Sim( const Sim& ) = delete;
  Sim& operator=( const Sim& ) = delete;

  /// @brief Output a character to the connection.
  Interface& operator<<( char c ) override; 

  /// @brief Input a character from the connection.
  Interface& operator>>( char &c ) override; 

  /// @brief Are there characters ready to be read?
  bool DataReady(void) override;

  private:

  std::queue<char>& toFirmware;
  std::queue<char>& fromFirmware;
  std::string outputString;
};

///
/// @brief Blocking call to get a string from a connection
///
/// @param[in]    The connection
/// @return       The string
///
/// If the connection fails an empty string is returned.
///
std::string GetString( Interface& con );

///
/// @brief Stream operator for a C style string.
///
Interface& operator<<( Interface& ostream, const char* string );

///
/// @brief Stream operator for an unsigned number
///
Interface& operator<<( Interface& ostream, unsigned int i );

///
/// @brief Stream operator for a signed number
///
Interface& operator<<( Interface& ostream, int i );

} // End BeeFocusedCon Namespace. 

// 
// Connection is a core INDI library namespace.
// 
namespace Connection {

///
/// @brief Sim interface for unit testing
///
class Sim: public Interface
{
  public:

  Sim(INDI::DefaultDevice *dev );
  bool Connect() override;
  bool Disconnect() override;
  void Activated() override;
  void Deactivated() override;
  std::string name() override { return "SIMULATED_CONNECTION"; }
  std::string label() override { return "Simulated"; }

};

}

