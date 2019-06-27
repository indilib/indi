#include "test_helpers.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "beefocus.h"
#include "indistandardproperty.h"
#include "lilxml.h"

namespace BeeTest {

///
/// @brief Advance time on the simulated driver 
///
/// @param[out] driver - The BeeFocused driver
/// @param[in]  mSec   - The amount of time, in mSec, to move forward.
///
void AdvanceTimeForward(
  BeeFocused::Driver& driver,
  unsigned int mSec)
{
  //
  // For simplicity, restrict to 'AdvanceOnTimerHit' intervals.
  // 
  ASSERT_EQ( 0, mSec % BeeFocused::AdvanceOnTimerHit );
  unsigned int timerHits = mSec / BeeFocused::AdvanceOnTimerHit;

  for ( unsigned int i = 0; i < timerHits; ++i )
  {
    driver.TimerHit();
  }
}

///
/// @brief Test the Tokenizer used to parse input from the Firmware
///
TEST( DEVICE, BasicTokenizer)
{
  auto result = BeeFocused::Tokenize( std::string("my dog has fleas"));
  std::vector<std::string> golden = {"my", "dog", "has", "fleas" };
  ASSERT_EQ( result, golden );
}

///
/// @brief Test the Tokenizer used to parse input from the Firmware
///
/// Extra spaces case.
///
TEST( DEVICE, StringWithExtraSpaces)
{
  auto result = BeeFocused::Tokenize( std::string("  my    dog  has fleas   "));
  std::vector<std::string> golden = {"my", "dog", "has", "fleas" };
  ASSERT_EQ( result, golden );
}

///
/// @brief Basic connect to device and verify.
///
/// Connects to the simulate device and checks output. Used by >1 test
///
/// Flow Control:
///
/// 1. Do basic initialization
/// 2. Set the connection mode to "Simulated"
/// 3. Attempt to connect
/// 4. Look for connection messages in the output 
/// 
void EstablishConnection( BeeFocused::Driver& driver )
{
  ITH::StdoutCapture outCap;

  // 1. Do basic initialization
  driver.setDeviceName("BeeFocusUnitTest");
  driver.ISGetProperties( driver.getDeviceName() );

  // 2. Set the connection mode to "Simulated"
  ITH::TurnSwitch( driver, "CONNECTION_MODE",
      ITH::StateData{{{"SIMULATED_CONNECTION", ISS_ON }}} );

  // 3. Attempt to connect
  ITH::TurnSwitch( driver, INDI::SP::CONNECTION,
      ITH::StateData{{{"CONNECT", ISS_ON }}} );

  // 4. Look for connection messages in the output 
  std::string output = outCap.getOutput();

  ASSERT_NE( std::string::npos, output.find("[INFO] Handshake Success")); 
  ASSERT_NE( std::string::npos, output.find("[INFO] BeeFocusUnitTest is online"));
  ASSERT_NE( std::string::npos, output.find("[INFO] Connection Succeeded")); 
}

///
/// @brief Verify device connects properly.
///
/// 1. Establish Connection
/// 2. Check Connection Status message
/// 3. Disconnect and check connection status.
///
TEST( DEVICE, FocuserConnectsProperly )
{
  // 1. Establish Connection
  //
  BeeFocused::Driver driver;
  EstablishConnection( driver );

  //
  // 2. Check Connection Status message
  //
  // The connection status is updated on TimerHit.  Move time forward
  // a bit and check for the proper connection status.
  //
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 250 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "CONNECT_STATUS"), "Connected" );
  }

  // 3. Disconnect and check connection status.
  {
    ITH::StdoutCapture outCap;
    ITH::TurnSwitch( driver, INDI::SP::CONNECTION,
        ITH::StateData{{{"DISCONNECT", ISS_ON }}} );
    AdvanceTimeForward( driver, 250 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "CONNECT_STATUS"), "Not Connected" );
  }
}

///
/// @brief Verify that a focuser with an end-stop auto-syncs
///
/// Flow Control:
///
/// 1. Establish Connection
/// 2. Advance time by 4000 ms and verify focuser isn't synced yet
/// 3. Advance time by 200 ms and verify focuser is now synced
///
TEST( DEVICE, FocuserHomesToEndStop)
{
  // 1. Establish Connection
  BeeFocused::Driver driver;
  EstablishConnection(  driver );

  // 2. Advance time by 4000 ms and verify focuser isn't synced yet
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 4000 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Not Synced" );
  }

  // 3. Advance time by 200 ms and verify focuser is now synced
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 250 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Synced" );
  }
}


///
/// @brief Verify Set Absolute Position works & interrupts home
///
/// 1. Set an absolute position before we finish homing.
/// 2. Verify the focuser reaches the target position given time.
/// 3. Try a value past the maximum focuser position (35000)
///
TEST( DEVICE, SetABSPos )
{
  BeeFocused::Driver driver;
  EstablishConnection( driver );

  /// 1. Set an absolute position before we finish homing.
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 2000 );
    ITH::SetNumber( driver, "ABS_FOCUS_POSITION",
      ITH::NumberData{{{"FOCUS_ABSOLUTE_POSITION", 1000.0 }}} );
    AdvanceTimeForward( driver, 2000 );
    ITH::XMLCapture xml( outCap.getOutput() );

    // should be not synced (home interrupted) and at 
    // position 0 (still re-winding from the home attempt)
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Not Synced" );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Busy" );
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "0" );
  }

  /// 2. Verify the focuser reaches the target position given time.
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 5000);
    std::string myout = outCap.getOutput();
    ITH::XMLCapture xml( myout );

    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "1000" );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Ok" );
  }

  // 3. Try a value past the maximum focuser position (35000)
  {
    ITH::StdoutCapture outCap;
    ITH::SetNumber( driver, "ABS_FOCUS_POSITION",
      ITH::NumberData{{{"FOCUS_ABSOLUTE_POSITION", 50000.0 }}} );

    AdvanceTimeForward( driver, 100000);
    std::string output = outCap.getOutput();
    ITH::XMLCapture xml( output );
  
    // Verify warning issued, and that we don't move past 35000.
    ASSERT_NE( std::string::npos, output.find("[WARNING] Focuser will not move past maximum value of 35000")); 
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "35000" );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Ok" );
  }
}

///
/// @brief Verify that abort works
///
/// 1. Test Abort while homing at start-up
/// 2. Start a move and verify that we're moving
/// 3. Issue an abort.  Focuser should stop moving.
/// 4. Make sure focuser position doesn't change if more time passes.
///
TEST( DEVICE, TestAbort )
{
  BeeFocused::Driver driver;
  EstablishConnection( driver );

  /// 1. Test Abort while homing at start-up
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 2000 );

    ITH::TurnSwitch( driver, "FOCUS_ABORT_MOTION",
      ITH::StateData{{{"ABORT", ISS_ON }}} );

    // Give plenty of time to make sure we aborted.
    AdvanceTimeForward( driver, 10000 );
    ITH::XMLCapture xml( outCap.getOutput() );

    // should be not synced (abort interrupted) and at 
    // position 0 (still re-winding from the home attempt)
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Not Synced" );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Ok" );
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "0" );
  }
  // 2. Start a move and verify that we're moving
  {
    ITH::StdoutCapture outCap;
    ITH::SetNumber( driver, "ABS_FOCUS_POSITION",
      ITH::NumberData{{{"FOCUS_ABSOLUTE_POSITION", 10000.0 }}} );
    AdvanceTimeForward( driver, 3000 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Busy" );
    ASSERT_NE( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "0" );
  }
  // 3. Issue an abort.  Focuser should stop moving.
  {
    ITH::StdoutCapture outCap;

    ITH::TurnSwitch( driver, "FOCUS_ABORT_MOTION",
      ITH::StateData{{{"ABORT", ISS_ON }}} );

    // A tiny bit of time to make sure we aborted.
    AdvanceTimeForward( driver, 750 );
    ITH::XMLCapture xml( outCap.getOutput() );

    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Ok" );
    ASSERT_NE( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "0" );
  }
  // 4. Make sure focuser position doesn't change if more time passes.
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 5000 );
    ASSERT_EQ( outCap.getOutput(), "" );
  }
}

///
/// @brief Verify that Syncing works
///
/// 1. Set absolute position to 1000 (also interrupts homing)
/// 2. Sync to 0
/// 3. Sync to 1234
///
TEST( DEVICE, TestSync )
{
  BeeFocused::Driver driver;
  EstablishConnection( driver );

  // 1. Set absolute position to 1000 (also interrupts homing)
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 1000 );
    ITH::SetNumber( driver, "ABS_FOCUS_POSITION",
      ITH::NumberData{{{"FOCUS_ABSOLUTE_POSITION", 1000.0 }}} );
    AdvanceTimeForward( driver, 10000 );
    ITH::XMLCapture xml( outCap.getOutput() );

    // should be not synced (home interrupted) and at 
    // position 1000
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Not Synced" );
    ASSERT_EQ( xml.lastState( "ABS_FOCUS_POSITION"), "Ok" );
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "1000" );
  }
  // 2. Sync to 0
  {
    ITH::StdoutCapture outCap;
    ITH::SetNumber( driver, "FOCUS_SYNC",
      ITH::NumberData{{{"FOCUS_SYNC_VALUE", 0.0 }}} );
    AdvanceTimeForward( driver, 2000 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "HOME_STATUS"), "Synced" );
    ASSERT_EQ( xml.lastState( "FOCUS_SYNC_VALUE"), "0" );
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "0" );
  }
  // 3. Sync to 1234
  {
    ITH::StdoutCapture outCap;
    ITH::SetNumber( driver, "FOCUS_SYNC",
      ITH::NumberData{{{"FOCUS_SYNC_VALUE", 1234 }}} );
    AdvanceTimeForward( driver, 2000 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "FOCUS_SYNC_VALUE"), "1234" );
    ASSERT_EQ( xml.lastState( "FOCUS_ABSOLUTE_POSITION"), "1234" );
  }
}

///
/// @brief Verify that we get a maximum position from the firmware
///
/// 1. Establish Connection
/// 2. Advance time & verify that we got the firmware's maximum position
///
TEST( DEVICE, FocuserUpdatesMaxPos )
{
  // 1. Establish Connection
  BeeFocused::Driver driver;
  EstablishConnection(  driver );

  // 2. Advance time & verify that we got the firmwares maximum position
  {
    ITH::StdoutCapture outCap;
    AdvanceTimeForward( driver, 1000 );
    ITH::XMLCapture xml( outCap.getOutput() );
    ASSERT_EQ( xml.lastState( "FOCUS_MAX_VALUE"), "35000" );
  }
}

} // End BeeTest namespace.

