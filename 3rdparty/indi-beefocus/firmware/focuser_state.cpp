
#include <iterator>
#include <vector>
#include <string>
#include <memory>
#include "command_parser.h"
#include "wifi_debug_ostream.h"
#include "focuser_state.h"

using namespace FS;

const StateToString FS::stateNames =
{
  { State::ACCEPT_COMMANDS,               "ACCEPTING_COMMANDS" },
  { State::DO_STEPS,                      "DO_STEPS"           },
  { State::STEPPER_INACTIVE_AND_WAIT,     "STEPPER_INACTIVE"   },
  { State::STEPPER_ACTIVE_AND_WAIT,       "STEPPER_ACTIVE"     },
  { State::SET_DIR,                       "SET_DIR"            },
  { State::MOVING,                        "MOVING"             },
  { State::STOP_AT_HOME,                  "STOP_AT_HOME"       },
  { State::SLEEP,                         "LOW_POWER"          },
  { State::ERROR_STATE,                   "ERROR ERROR ERROR"  },
};

const CommandToBool FS::doesCommandInterrupt= 
{
  { CommandParser::Command::Abort,         true   },
  { CommandParser::Command::Home,          true   },
  { CommandParser::Command::LHome,         true   },
  { CommandParser::Command::PStatus,       false  },
  { CommandParser::Command::MStatus,       false  },
  { CommandParser::Command::SStatus,       false  },
  { CommandParser::Command::ABSPos,        true   },
  { CommandParser::Command::RELPos,        true   },
  { CommandParser::Command::Sync,          true   },
  { CommandParser::Command::Firmware,      false  },
  { CommandParser::Command::Caps,          false  },
  { CommandParser::Command::NoCommand,     false  },
};

const std::unordered_map<CommandParser::Command,
  void (Focuser::*)( CommandParser::CommandPacket),EnumHash> 
  Focuser::commandImpl = 
{
  { CommandParser::Command::Abort,      &Focuser::doAbort },
  { CommandParser::Command::Home,       &Focuser::doHome },
  { CommandParser::Command::LHome,      &Focuser::doLHome },
  { CommandParser::Command::PStatus,    &Focuser::doPStatus },
  { CommandParser::Command::MStatus,    &Focuser::doMStatus },
  { CommandParser::Command::SStatus,    &Focuser::doSStatus },
  { CommandParser::Command::ABSPos,     &Focuser::doABSPos },
  { CommandParser::Command::RELPos,     &Focuser::doRELPos },
  { CommandParser::Command::Sync,       &Focuser::doSync},
  { CommandParser::Command::Firmware,   &Focuser::doFirmware},
  { CommandParser::Command::Caps,       &Focuser::doCaps},
  { CommandParser::Command::NoCommand,  &Focuser::doError },
};

const std::unordered_map<State,unsigned int (Focuser::*)( void ),EnumHash>
  Focuser::stateImpl =
{
  { State::ACCEPT_COMMANDS,           &Focuser::stateAcceptCommands },
  { State::DO_STEPS,                  &Focuser::stateDoingSteps },
  { State::STEPPER_INACTIVE_AND_WAIT, &Focuser::stateStepInactiveAndWait },
  { State::STEPPER_ACTIVE_AND_WAIT,   &Focuser::stateStepActiveAndWait },
  { State::SET_DIR,                   &Focuser::stateSetDir },
  { State::MOVING,                    &Focuser::stateMoving },
  { State::STOP_AT_HOME,              &Focuser::stateStopAtHome },
  { State::SLEEP,                     &Focuser::stateSleep },
  { State::ERROR_STATE,               &Focuser::stateError }
};

BuildParams::BuildParamMap BuildParams::builds = {
  {
    Build::LOW_POWER_HYPERSTAR_FOCUSER,
    {
      TimingParams { 
        100,        // Check for new commands every 100ms
        50,         // Take 50 steps before checking for interrupts
        5*60*1000,  // Go to sleep after 5 minutes of inactivity
        1000,       // Check for new input in sleep mode every second
        1000        // Take 1 second to power up the focuser motor on awaken
      },
      true,         // Focuser can use a home switch to synch
      35000         // End of the line for my focuser
    }
  },
  { Build::UNIT_TEST_BUILD_HYPERSTAR, 
    {
      TimingParams { 
        10,         // Check for new commands every 10ms
        2,          // Take 2 steps before checking for interrupts
        1000,       // Go to sleep after 1 second of inactivity
        500,        // Check for new input in sleep mode every 500ms
        200,        // Allow 200ms to power on the motor
      },
      true,         // Focuser can use a home switch to synch
      35000         // End of the line for my focuser
    }
  },
  {
    Build::TRADITIONAL_FOCUSER,
    {
      TimingParams { 
        100,        // Check for new commands every 100ms
        50,         // Take 50 steps before checking for interrupts
        10*24*60*1000,  // Go to sleep after 10 days of inactivity
        1000,       // Check for new input in sleep mode every second
        1000        // Take 1 second to power up the focuser motor on awaken
      },
      false,        // Focuser cannot use a home switch to synch
      5000          // Mostly a place holder
    }
  },
  { Build::UNIT_TEST_TRADITIONAL_FOCUSER, 
    {
      TimingParams { 
        10,         // Check for new commands every 10ms
        2,          // Take 2 steps before checking for interrupts
        1000,       // Go to sleep after 1 second of inactivity
        500,        // Check for new input in sleep mode every 500ms
        200,        // Allow 200ms to power on the motor
      },
      false,        // Focuser cannot use a home switch to synch
      5000          // Mostly a place holder
    }
  },
};

Focuser::Focuser(
    std::unique_ptr<NetInterface> netArg,
    std::unique_ptr<HWI> hardwareArg,
    std::unique_ptr<DebugInterface> debugArg,
    const BuildParams params
) : buildParams{ params }
{
  focuserPosition = 0;
  isSynched = false;
  time = 0;
  uSecRemainder = 0;
  timeLastInterruptingCommandOccured = 0;
  motorState = MotorState::OFF;

  std::swap( net, netArg );
  std::swap( hardware, hardwareArg );
  std::swap( debugLog, debugArg );
  
  DebugInterface& dlog = *debugLog;
  dlog << "Bringing up net interface\n";
  
  // Bring up the interface to the controlling computer

  net->setup( dlog );
  WifiDebugOstream log( debugLog.get(), net.get() );

  //
  // Set the pin modes 
  //
  hardware->PinMode(HWI::Pin::STEP,       HWI::PinIOMode::M_OUTPUT );  
  hardware->PinMode(HWI::Pin::DIR,        HWI::PinIOMode::M_OUTPUT );  
  hardware->PinMode(HWI::Pin::MOTOR_ENA,  HWI::PinIOMode::M_OUTPUT );  
  hardware->PinMode(HWI::Pin::HOME,       HWI::PinIOMode::M_INPUT ); 
 
  //
  // Set the output pin defaults and internal state
  // 
  setMotor( log, MotorState::ON ); 

  dir = Dir::FORWARD;
  hardware->DigitalWrite( HWI::Pin::DIR, HWI::PinState::DIR_FORWARD); 
  hardware->DigitalWrite( HWI::Pin::STEP, HWI::PinState::STEP_INACTIVE );

  log << "Focuser is up\n";
}

void Focuser::doAbort( CommandParser::CommandPacket cp )
{
  (void) cp;
  // Do nothing - command triggers a state interrupt.
}

void Focuser::doHome( CommandParser::CommandPacket cp )
{
  (void) cp;
  if ( buildParams.focuserHasHome )
  {
    stateStack.push( State::STOP_AT_HOME );
  }
}

void Focuser::doLHome( CommandParser::CommandPacket cp )
{
  (void) cp;
  if ( buildParams.focuserHasHome )
  {
    if ( !isSynched )
    {
      stateStack.push( State::STOP_AT_HOME );
    }
  }
}

void Focuser::doPStatus( CommandParser::CommandPacket cp )
{
  (void) cp;
  DebugInterface& log = *debugLog;
  log << "Processing pstatus request\n";
  *net << "Position: " << focuserPosition << "\n";
}

void Focuser::doMStatus( CommandParser::CommandPacket cp )
{
  (void) cp;
  DebugInterface& log = *debugLog;

  log << "Processing mstatus request\n";
  *net << "State: " << stateNames.at(stateStack.topState()) << 
                " " << stateStack.topArg() << "\n";
}

void Focuser::doSStatus( CommandParser::CommandPacket cp )
{
  (void) cp;
  DebugInterface& log = *debugLog;

  log << "Processing sstatus request\n";
  *net << "Synched: " << (isSynched ? "YES" : "NO" ) << "\n";
}

void Focuser::doFirmware( CommandParser::CommandPacket cp )
{
  (void) cp;
  DebugInterface& log = *debugLog;

  log << "Processing firmware request\n";
  *net << "Firmware: 1.0\n";
}

void Focuser::doCaps( CommandParser::CommandPacket cp )
{
  (void) cp;
  DebugInterface& log = *debugLog;

  log << "Processing capabilities request\n";
  *net << "MaxPos: " << buildParams.maxAbsPos << "\n";
  *net << "CanHome: " << (buildParams.focuserHasHome ? "YES\n" : "NO\n" );
}

void Focuser::doRELPos( CommandParser::CommandPacket cp )
{
  cp.optionalArg += focuserPosition;
  doABSPos( cp );
}

void Focuser::doABSPos( CommandParser::CommandPacket cp )
{
  int new_position = cp.optionalArg;
  new_position = std::min( new_position, (int) buildParams.maxAbsPos );
  new_position = std::max( new_position, (int) 0 );

  stateStack.push( State::MOVING, new_position );

  if ( new_position < focuserPosition )
  {
    int backtrack = new_position - 500;
    backtrack = std::max( backtrack, (int) 0 );
    stateStack.push( State::MOVING, backtrack );
  }
}

void Focuser::doSync( CommandParser::CommandPacket cp )
{
  stateStack.push( State::MOVING, cp.optionalArg );
  focuserPosition = cp.optionalArg;
  isSynched = true;
}

void Focuser::doError( CommandParser::CommandPacket cp )
{
  (void) cp;
  stateStack.push( State::ERROR_STATE, __LINE__ );   
}

void Focuser::processCommand( CommandParser::CommandPacket cp )
{
  if ( doesCommandInterrupt.at( cp.command ))
  {
    timeLastInterruptingCommandOccured = time;
  }
  auto function = commandImpl.at( cp.command );
  (this->*function)( cp );
}

unsigned int Focuser::stateAcceptCommands()
{
  DebugInterface& log = *debugLog;
  auto cp = CommandParser::checkForCommands( log, *net );

  if ( cp.command != CommandParser::Command::NoCommand )
  {
    processCommand( cp );
    return 0;
  }
  const unsigned int timeSinceLastInterrupt = 
      time - timeLastInterruptingCommandOccured;

  if ( timeSinceLastInterrupt > buildParams.timingParams.getInactivityToSleep() )
  {
    stateStack.push( State::SLEEP );
    return 0;
  }

  const int timeBetweenChecks = buildParams.timingParams.getEpochBetweenCommandChecks();
  const int mSecToNextEpoch = timeBetweenChecks - ( time % timeBetweenChecks );

  return mSecToNextEpoch * 1000;
}

unsigned int Focuser::stateSetDir()
{
  Dir desiredDir = stateStack.topArg().getDir();

  stateStack.pop();

  if ( desiredDir != dir )
  {
    dir = desiredDir;
    if ( dir == Dir::FORWARD )
      hardware->DigitalWrite( HWI::Pin::DIR, HWI::PinState::DIR_FORWARD); 
    if ( dir == Dir::REVERSE )       
      hardware->DigitalWrite( HWI::Pin::DIR, HWI::PinState::DIR_BACKWARD );
      // Trigger a 1ms pause so the stepper motor controller sees the
      // state change before we try to do anything.
    return 1000;
  }    

  return 0;
}

unsigned int Focuser::stateStepInactiveAndWait()
{
  hardware->DigitalWrite( HWI::Pin::STEP, HWI::PinState::STEP_INACTIVE );
  stateStack.pop();
  return 1000;
}

unsigned int Focuser::stateStepActiveAndWait()
{
  hardware->DigitalWrite( HWI::Pin::STEP, HWI::PinState::STEP_ACTIVE );
  stateStack.pop();
  return 1000;
}

unsigned int Focuser::stateDoingSteps()
{
  if ( stateStack.topArg().getInt() == 0 )
  {
    // We're done at 0
    stateStack.pop();
    return 0;
  }
  stateStack.topArgSet( stateStack.topArg().getInt()-1 );

  stateStack.push( State::STEPPER_INACTIVE_AND_WAIT );
  stateStack.push( State::STEPPER_ACTIVE_AND_WAIT );

  focuserPosition += (dir == Dir::FORWARD) ? 1 : -1;
  //focuserPosition = focuserPosition >= 0 ? focuserPosition : 0;

  return 0;  
}

unsigned int Focuser::stateMoving()
{
  WifiDebugOstream log( debugLog.get(), net.get() );
  log << "Moving " << focuserPosition << "\n";
  
  if ( stateStack.topArg().getInt() == focuserPosition ) {
    // We're at the target,  exit
    stateStack.pop();
    return 0;    
  }

  // Check for new commands
  DebugInterface& debug= *debugLog;
  auto cp = CommandParser::checkForCommands( debug, *net );

  if ( cp.command != CommandParser::Command::NoCommand )
  {
    if ( doesCommandInterrupt.at( cp.command ))
    {
      stateStack.reset();
    }
    processCommand( cp );
    if ( doesCommandInterrupt.at( cp.command ))
    {
      return 0;
    }
  }

  const int  steps        = stateStack.topArg().getInt() - focuserPosition;
  const Dir  nextDir      = steps > 0 ? Dir::FORWARD : Dir::REVERSE;
  const int  absSteps     = steps > 0 ? steps : -steps;
  const int  doStepsMax   = buildParams.timingParams.getMaxStepsBetweenChecks(); 
  const int  clippedSteps = absSteps > doStepsMax ? doStepsMax : absSteps;

  stateStack.push( State::DO_STEPS, clippedSteps );
  stateStack.push( State::SET_DIR,  nextDir );
  return 0;        
}

unsigned int Focuser::stateStopAtHome()
{
  WifiDebugOstream log( debugLog.get(), net.get() );

  assert ( motorState == MotorState::ON );

  if ( hardware->DigitalRead( HWI::Pin::HOME ) == HWI::PinState::HOME_ACTIVE ) 
  {
    log << "Hit home at position " << focuserPosition << "\n";
    log << "Resetting position to 0\n";
    focuserPosition = 0;
    isSynched = true;
    stateStack.pop();
    return 0;        
  }

  const int  doStepsMax   = buildParams.timingParams.getMaxStepsBetweenChecks(); 

  if ( ((focuserPosition) % doStepsMax) == 0 )
  {
    log << "Homing " << focuserPosition << "\n";

    // Check for new commands
    DebugInterface& debug= *debugLog;
    auto cp = CommandParser::checkForCommands( debug, *net );

    if ( cp.command != CommandParser::Command::NoCommand )
    {
      if ( doesCommandInterrupt.at( cp.command ))
      {
        stateStack.reset();
      }
      processCommand( cp );
      if ( doesCommandInterrupt.at( cp.command ))
      {
        return 0;
      }
    }
  }

  stateStack.push( State::DO_STEPS, 1 );
  stateStack.push( State::SET_DIR, Dir::REVERSE );
  return 0;        
}

unsigned int Focuser::stateSleep()
{
  WifiDebugOstream log( debugLog.get(), net.get() );
  // Check for new commands
  DebugInterface& debug= *debugLog;
  auto cp = CommandParser::checkForCommands( debug, *net );

  if ( cp.command != CommandParser::Command::NoCommand )
  {
    if ( doesCommandInterrupt.at( cp.command ))
    {
      stateStack.reset();
    }
    processCommand( cp );
    if ( doesCommandInterrupt.at( cp.command ))
    {
      if ( motorState != MotorState::ON ) 
      {
        setMotor( log, MotorState::ON );
        return buildParams.timingParams.getTimeToPowerStepper() * 1000;
      }
    }
    return 0;   // Go until we're out of commands.
  }

  if ( motorState != MotorState::OFF )
  {
    setMotor( log, MotorState::OFF );
  }
  const int sleepEpoch = buildParams.timingParams.getEpochForSleepCommandChecks();
  const int mSecToNextEpoch = sleepEpoch - ( time % sleepEpoch ); 

  return mSecToNextEpoch * 1000;
}

unsigned int Focuser::stateError()
{
  WifiDebugOstream log( debugLog.get(), net.get() );
  log << "hep hep hep error error error\n";
  return 10*1000*1000; // 10 sec pause 
}

unsigned int Focuser::loop()
{
  ptrToMember function = stateImpl.at( stateStack.topState() );
  const unsigned uSecToNextCall = (this->*function)();
  uSecRemainder += uSecToNextCall;
  time += uSecRemainder / 1000;
  uSecRemainder = uSecRemainder % 1000;
  return uSecToNextCall;
}

void Focuser::setMotor( WifiDebugOstream& log, MotorState m )
{
  motorState = m;

  hardware->DigitalWrite( HWI::Pin::MOTOR_ENA, ( m == MotorState::ON ) ?
      HWI::PinState::MOTOR_ON :
      HWI::PinState::MOTOR_OFF );

  log << "Motor set " << (( m == MotorState::ON ) ? "on" : "off" ) << "\n";
}

