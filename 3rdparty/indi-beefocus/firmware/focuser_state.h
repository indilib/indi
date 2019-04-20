#ifndef __FOCUSER_STATE_H__
#define __FOCUSER_STATE_H__

#include <vector>
#include <memory>
#include <string>
#include <assert.h>
#include <unordered_map>
#include "net_interface.h"
#include "hardware_interface.h"
#include "command_parser.h"

///
/// @brief Focuser Namespace
/// 
/// This is a collection of code that has to do with the focuser's
/// state.  The code in this namespace 
///
/// - Initializes the hardware
/// - Moves the stepper motor
/// - Accepts input from the network
///
/// \b Concepts
///
/// - <b> Basic Flow: </b>
///       Callers run the focuser by repeatly calling Focuser::loop; this 
///       is simular to the basic Arduino loop.  Focuser::loop returns the 
///       amount of time (in micro-seconds) it would like the caller to wait 
///       before invoking Focuser::loop again.
/// - <b> Net Interface: </b>
///       The Net Interface (NetInterface) is where the focuser gets input 
///       and sends output to.  Normally that's the WIFI connection to the 
///       host computer, but there's a Mock Network Interface that's used 
///       for testing.
/// - <b> Hardware Interface: </b>
///       The Hardware Interface (HWI) is how the focuser interacts with
///       the actual hardware.  Normally this as the ESP8266 itself, but
///       there's a Mock Hardware Interface that's used for testing.
/// - <b> Debug Interface: </b>
///       The Debug Interface (DebugInterface) can be used to send debug
///       messages to a host computer.  It's a developer only interface.
/// - <b> Commands: </b>
///       A command is an instruction that comes from the the comes from the 
///       the Net Interface.  i.e., "What is the focuser position",  
///       "Move the focuser to this position".  The list of valid commands 
///       is declared in CommandParser::Command
/// - <b> Invidivual State: </b>
///       The Focuser is most similar to a state machine.  Focuser::loop
///       just runs the handler for the current state. For example, if the
///       current state is "moving to a new position" the focuser might
///       figure out where it is right now, what direction it has to move,
///       and how many steps it needs to take to get to that positon.
/// - <b> State Stack: </b>
///       The Focuser actually has a stack of states.  The stack is useful
///       because it's sometimes easier to describe a complex operation
///       using simpler operations.  i.e., you can move the stepper motor
///       one step by running State::STEPPER_ACTIVE_AND_WAIT and then
///       State::STEPPER_INACTIVE_AND_WAIT.  A move can be done by setting
///       a direction using State::SET_DIR and then using State::DO_STEPS
///       to take multiple steps.
///
namespace FS {

/// @brief Focuser's State Enum
///
/// @see FS Namespace for high level description.
///
enum class State 
{
  START_OF_STATES = 0,        ///< Start of States
  ACCEPT_COMMANDS = 0,        ///< Accepting commands from the net interface
  DO_STEPS,                   ///< Doing n Stepper Motor Steps
  STEPPER_INACTIVE_AND_WAIT,  ///< Set Stepper to Inactive and Pause
  STEPPER_ACTIVE_AND_WAIT,    ///< Set Stepper to Active and Pause
  SET_DIR,                    ///< Set the Direction Pin
  MOVING,                     ///< Move to an absolute position
  STOP_AT_HOME,               ///< Rewind until the Home input is active
  SLEEP,                      ///< Low Power State
  ERROR_STATE,                ///< Error Errror Error
  END_OF_STATES               ///< End of States
};

/// @brief What direction is the focuser going?
enum class Dir {
  FORWARD,    ///< Go Forward
  REVERSE     ///< Go Backward
};

class StateArg
{
  public: 

  enum class Type {
    NONE,
    INT,
    DIR
  };

  StateArg() : type{ Type::NONE}  {}
  StateArg( int i ) : type{Type::INT}, intArg{ i } {}
  StateArg( Dir d ) : type{Type::DIR}, dirArg{ d } {}
  Type getType() { return type; }
  int getInt() { assert( type==Type::INT ); return intArg; }
  Dir getDir() { assert( type==Type::DIR ); return dirArg; }

  private:

  Type type;

  union {
    int intArg;
    Dir dirArg;
  };
};

class TimingParams
{
  public:

  TimingParams( 
    int msEpochBetweenCommandChecksRHS    = 100,        // 100 ms
    int maxStepsBetweenChecksRHS          = 50,
    unsigned msInactivityToSleepRHS       = 5*60*1000,  // 5 minutes
    int msEpochForSleepCommandChecksRHS   = 1*1000,     // 1 seconds
    int msToPowerStepperRHS               = 1*1000      // 1 second
  ) :
    msEpochBetweenCommandChecks{ msEpochBetweenCommandChecksRHS },
    maxStepsBetweenChecks{ maxStepsBetweenChecksRHS },
    msInactivityToSleep{ msInactivityToSleepRHS },
    msEpochForSleepCommandChecks{ msEpochForSleepCommandChecksRHS },
    msToPowerStepper{ msToPowerStepperRHS}
  {
  }

  int getEpochBetweenCommandChecks() const 
  { 
    return msEpochBetweenCommandChecks; 
  }
  int getMaxStepsBetweenChecks() const 
  { 
    return maxStepsBetweenChecks; 
  }
  unsigned getInactivityToSleep() const 
  { 
    return msInactivityToSleep; 
  }
  int getEpochForSleepCommandChecks() const 
  { 
    return msEpochForSleepCommandChecks; 
  }
  int getTimeToPowerStepper() const 
  { 
    return msToPowerStepper;
  }

  private:
  int msEpochBetweenCommandChecks;
  int maxStepsBetweenChecks;
  unsigned msInactivityToSleep;
  int msEpochForSleepCommandChecks;
  int msToPowerStepper;
};

enum class Build
{
  LOW_POWER_HYPERSTAR_FOCUSER,
  TRADITIONAL_FOCUSER,
  UNIT_TEST_BUILD_HYPERSTAR,
  UNIT_TEST_TRADITIONAL_FOCUSER
};


class BuildParams {
  public:

  using BuildParamMap = const std::unordered_map<Build, BuildParams, EnumHash >;

  BuildParams() = delete;

  BuildParams( 
    TimingParams timingParamsRHS,
    bool focuserHasHomeRHS,
    unsigned int maxAbsPosRHS
  ) : 
    timingParams{ timingParamsRHS },
    focuserHasHome{ focuserHasHomeRHS },
    maxAbsPos { maxAbsPosRHS }
  {
  }
  BuildParams( Build buildType )
  {
    *this = builds.at( buildType );
  } 

  TimingParams timingParams;
  bool focuserHasHome;
  unsigned int maxAbsPos;
  static BuildParamMap builds;

  private:
};

///
/// @brief Stack of FS:States.
///
/// Invariants:
///
/// - In normal operation, the stack's bottom is always an ACCEPT_COMMANDS state
/// - After construction, the stack can never be empty
/// - If a pop operation leaves the stack empty an ERROR_STATE is pushed
/// 
class StateStack {
  public:

  StateStack()
  {
    push( State::ACCEPT_COMMANDS, StateArg() );
  }

  /// @brief Reset the stack to the newly initialized state.
  void reset( void )
  {
    while ( stack.size() > 1 ) pop();
  }

  /// @brief Get the top state.
  State topState( void )
  {
    return stack.back().state;
  }

  /// @brief Get the top state's argumment.
  StateArg topArg( void )
  {
    return stack.back().arg;
  }

  /// @brief Set the top state's argumment.
  void topArgSet( StateArg newVal )
  {
    stack.back().arg = newVal;
  }

  /// @brief Pop the top entry on the stack.
  void pop( void )
  {
    stack.pop_back();
    if ( stack.size() > 10 ) 
    {
      push( State::ERROR_STATE, StateArg(__LINE__) ); 
    }
    if ( stack.empty() ) 
    {
      // bug, should never happen.
      push( State::ERROR_STATE, StateArg(__LINE__) ); 
    }
  }

  /// @brief Push a new entry onto the stack
  void push( State newState, StateArg newArg = StateArg() )
  {
    stack.push_back( { newState , newArg } );
  }  
  
  private:

  typedef struct 
  {
    State state;   
    StateArg arg; 
  } CommandPacket;

  std::vector< CommandPacket > stack;
};

class Focuser 
{
  public:
 
  /// @brief Focuser State Constructor
  ///
  /// @param[in] netArg       - Interface to the network
  /// @param[in] hardwareArg  - Interface to the Hardware
  /// @param[in] debugArg     - Interface to the debug logger.
  /// @param[in] params       - Hardware Parameters 
  ///
  Focuser( 
		std::unique_ptr<NetInterface> netArg,
		std::unique_ptr<HWI> hardwareArg,
		std::unique_ptr<DebugInterface> debugArg,
    const BuildParams params
	);

  ///
  /// @brief Update the Focuser's State
  ///
  /// @return The amount of time the caller should wait (in microsecodns)
  ///         before calling loop again.
  ///
  unsigned int loop();

  static const std::unordered_map<CommandParser::Command,
    void (Focuser::*)( CommandParser::CommandPacket),EnumHash> 
    commandImpl;

  using ptrToMember = unsigned int ( Focuser::*) ( void );
  static const std::unordered_map< State, ptrToMember, EnumHash > stateImpl;

  private:

  /// @brief Deleted copy constructor
  Focuser( const Focuser& other ) = delete;
  /// @brief Deleted default constructor
  Focuser() = delete;
  /// @brief Deleted assignment operator
  Focuser& operator=( const Focuser& ) = delete;
  
  StateStack stateStack;

  void processCommand( CommandParser::CommandPacket cp );

  /// @brief Wait for commands from the network interface
  unsigned int stateAcceptCommands( void ); 
  /// @brief Move to position @arg
  unsigned int stateMoving( void );
  /// @brief Move the stepper @arg steps 
  unsigned int stateDoingSteps( void );
  /// @brief If needed, Change the state of the direction pin and pause
  unsigned int stateSetDir( void );
  /// @brief Set the Stepper to active (i.e., start step) and wait
  unsigned int stateStepActiveAndWait( void );
  /// @brief Set the Stepper to inactive (i.e., finish step) and wait
  unsigned int stateStepInactiveAndWait( void );
  /// @brief Rewind the focuser until the home input is active.
  unsigned int stateStopAtHome( void );
  /// @brief Low power mode
  unsigned int stateSleep( void );
  /// @brief If we land in this state, complain a lot.
  unsigned int stateError( void );

  void doAbort( CommandParser::CommandPacket );
  void doHome( CommandParser::CommandPacket );
  void doLHome( CommandParser::CommandPacket );
  void doPStatus( CommandParser::CommandPacket );
  void doMStatus( CommandParser::CommandPacket );
  void doSStatus( CommandParser::CommandPacket );
  void doABSPos( CommandParser::CommandPacket );
  void doRELPos( CommandParser::CommandPacket );
  void doSync( CommandParser::CommandPacket );
  void doFirmware( CommandParser::CommandPacket );
  void doCaps( CommandParser::CommandPacket );
  void doError( CommandParser::CommandPacket );

  std::unique_ptr<NetInterface> net;
  std::unique_ptr<HWI> hardware;
  std::unique_ptr<DebugInterface> debugLog;
  
  const BuildParams buildParams;

  /// @brief What direction are we going? 
  ///
  /// FORWARD = counting up.
  /// REVERSE = counting down.
  ///
  Dir dir;

  enum class MotorState {
    ON,
    OFF
  };

  /// @brief Is the Stepper Motor On or Off. 
  MotorState motorState;

  void setMotor( WifiDebugOstream& log, MotorState );

  /// @brief What is the focuser's position of record
  int focuserPosition;

  /// @brief Is the focuser synched to a "known good" position
  bool isSynched;

  /// @brief Focuser uptime in MS
  unsigned int time;

  /// @brief For computing time in Focuser::loop
  unsigned int uSecRemainder;

  /// @brief Time the last command that could have caused an interrupt happened
  unsigned int timeLastInterruptingCommandOccured;
};

/// @brief Increment operator for State enum
///
inline State& operator++( State &s )
{
  return BeeFocus::advance< State, State::END_OF_STATES>(s);
}

/// @brief State to std::string Unordered Map
using StateToString = std::unordered_map< State, const std::string, EnumHash >;
/// @brief Command to bool Unordered Map
using CommandToBool = std::unordered_map< CommandParser::Command, bool, EnumHash >;

extern const StateToString stateNames;
///
/// @brief Does a particular incoming command interrupt the current state
///
/// Example 1.  A "Status" Command will not interrupt a move sequence
/// Example 2.  A "Home" Command will interrupt a focuser's move sequence
///
extern const CommandToBool doesCommandInterrupt;

/// @brief Output StateArg
template <class T,
  typename = my_enable_if_t<is_beefocus_ostream<T>::value>>
T& operator<<( T& ostream, StateArg sA )
{
  switch (sA.getType() )
  {
    case StateArg::Type::NONE:
      ostream << "NoArg";
      break;
    case StateArg::Type::INT:
      ostream << sA.getInt();
      break;
    default:
      assert(0);
  }
  return ostream; 
}

}

#endif

