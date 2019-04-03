#ifndef __COMMAND_PARSER_H__
#define __COMMAND_PARSER_H__

#include "basic_types.h"
#include "hardware_interface.h"
#include "debug_interface.h"

class NetInterface;

namespace CommandParser {

  int process_int( const std::string& string,  size_t pos );

  enum class Command {
    StartOfCommands = 0,  ///<  Start of the command list
    Abort = 0,            ///<  Abort a move
    Home,                 ///<  Rewind until the home pin is active
    LHome,                ///<  Lazy Home.  Home if not already synched
    PStatus,              ///<  Return Position to Caller
    MStatus,              ///<  Return the Mode (i.e., "moving", "homing")
    SStatus,              ///<  Is the focuser synced (i.e., homed)
    ABSPos,               ///<  Move to an absolute position
    RELPos,               ///<  Move relative to the current position
    Sync,                 ///<  Argument is the new position
    Firmware,             ///<  Get the firmware version
    Caps,                 ///<  Get build specific focuser capabilities
    NoCommand,            ///<  No command was specified.
    EndOfCommands         ///<  End of the comand list.
  };

  constexpr int NoArg = -1;

  class CommandPacket  {
    public:
    CommandPacket(): command{Command::NoCommand}, optionalArg{NoArg}
    {
    }
    CommandPacket( Command c ): command{c}, optionalArg{NoArg}
    {
    }
    CommandPacket( Command c, int o ): command{c}, optionalArg{o}
    {
    }

    bool operator==( const CommandPacket &rhs ) const 
    {
      return rhs.command == command && rhs.optionalArg == optionalArg;
    }

    Command command;
    int optionalArg;
  };

  /// @brief Get commands from the network interface
  ///
  /// @param[in] log          - Debug Log stream
  /// @param[in] netInterface - The network interface that we'll query
  ///            for the command.
  /// @return    New requests from netInterface that need to be acted
  ///            on.
  ///
  /// TODO 
  /// - Error handling (has none).
  /// - Move extra parameters used by the STATUS command.
  ///
  const CommandPacket checkForCommands( 
    DebugInterface& log,				// Input: Debug Log Strem
    NetInterface& netInterface	// Input: Network Interface
  );

};

/// @brief Increment operator for Command enum
///
inline CommandParser::Command& operator++( CommandParser::Command &c )
{
  return BeeFocus::advance< CommandParser::Command, CommandParser::Command::EndOfCommands >(c);
}

#endif

