/*******************************************************************************
  Based on a driver Copyright(c) 2012 Jasem Mutlaq. All rights reserved.
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

///
/// @brief Bee Focuser driver (Open Source IOT based focuser)
///
/// The bee focuser is an Open Source Internet of Things focuser.
/// The physical focusing hardware is based around an ESP8266 
/// Internet of things board - these are small commodity boards
/// that can either produce their own WiFi hot spot or connect to
/// an existing WiFi hot spot.
///
/// The focuser only works over WiFi.  When wireless works reliably
/// on a telescope,  not having to deal with cables is really nice.
///

#include "beefocus.h"
#include "beeconnect.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <unistd.h>

#include "connectionplugins/connectiontcp.h"

//////////////////////////////////////////////////////////////////////
//
// Declare the Driver's Singleton  
//
//////////////////////////////////////////////////////////////////////

BeeFocused::Driver* getDriverSingleton()
{
  static std::unique_ptr<BeeFocused::Driver> beeFocused(new BeeFocused::Driver());
  return beeFocused.get();
}

//////////////////////////////////////////////////////////////////////
//
// Driver executable entry points
//
//////////////////////////////////////////////////////////////////////

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    getDriverSingleton()->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    getDriverSingleton()->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    getDriverSingleton()->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    getDriverSingleton()->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    getDriverSingleton()->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////
//
// Driver Class
//
//////////////////////////////////////////////////////////////////////

namespace BeeFocused 
{

///
/// @brief Driver Constructor
///
Driver::Driver() 
{
    simFirmware.reset( new BeeSimFirmware( &toFirmware, &fromFirmware ));

    FI::SetCapability(
        FOCUSER_CAN_ABS_MOVE | 
        FOCUSER_CAN_REL_MOVE | 
        FOCUSER_CAN_ABORT | 
        FOCUSER_CAN_SYNC );

    setSupportedConnections( CONNECTION_TCP );
    timerTicks = 0;
    ignoreNextStatusUpdate = false;

    // 
    // Create custom connection interface for the simulator and unit testing.
    //
    simConnection.reset( new Connection::Sim( this ) );
    simConnection->registerHandshake([&]() { return mockCallHandshake(); });
}

//
// Establish a connection with the host using the INDI 
// TCP connection plugin.  If the socket is established
// TCP::Connect() will call Handshake() (below).
//
bool Driver::Connect()
{
    LOGF_INFO("%s", "Attempting to connect" );

    // For a TCP connection, calling Connect triggers TCP::Connect,
    // which establishes the connection and creates the FD.  
    // Focuser::Handshake gets the FD from the TCP Connection and records
    // it. 

    if (!INDI::Focuser::Connect())
    {
        LOGF_ERROR("%s", "Connection Failed");
        return false;
    }

    LOGF_INFO("%s", "Connection Succeeded");
    return true;
}

//
// See if we can get a valid response from the focuser
//
// 1. Create a connection object
// 2. Sanity Check the Connection
// 3. Reset Focuser Mode (todo - is this the right place)
// 4. Send requests for state down the network
// 5. Wait for responses
//    a) Check for input
//    b) Handle Lost Connection
//    c) Handle Received Handshake
// 6. Handle Timeout
//
bool Driver::Handshake()
{
    Connection::Interface* indiCon = getActiveConnection();

    // 1. Actually create the connection object.
    //    See Driver::Connect() for why we do this here.

    if ( indiCon->type() == Connection::Interface::CONNECTION_TCP )
    {
      // 1a. Handle actual focuser over TCP/IP
      Connection.reset( new BeeFocusedCon::TCP( PortFD ));
    }
    else
    {
      // 1b. Handle Focuser Simulator
      Connection.reset( new BeeFocusedCon::Sim( toFirmware, fromFirmware ) );
    }

    // 2. Sanity Check the Connection
    if ( Connection->Failed() ) 
    {
        LOG_ERROR("Failed HandShake - Connection Failed");
        return false;
    }

    // 3. Send requests for state down the network
    *Connection << "\n";
    *Connection << "pstatus\n";
    *Connection << "sstatus\n";
    *Connection << "mstatus\n";

    // 4. Wait for responses
    constexpr int timeBetweenChecks = 10;       // in ms
    constexpr int timeOut           = 3000;     // in ms
    for ( int mtime= 0;  mtime< timeOut ; mtime += timeBetweenChecks )
    {
        // 4a) Check for input
        HardwareState input( Connection.get() );

        // 4b) Handle Lost Connection
        if ( Connection->Failed() )
        {
            LOG_ERROR("Failed HandShake - Connection went down.");
            return false;
        }

        Optional<Mode> mode = input.getMode();
        if ( mode && *mode != Mode::UNCONNECTED )
        {
            LOG_INFO("Handshake Success");
            LOG_INFO("Sending Home");

            *Connection << "lazyhome\n";
            *Connection << "caps\n";
            *Connection << "pstatus\n";
            *Connection << "sstatus\n";
            *Connection << "mstatus\n";

            return true;
        }
        usleep( timeBetweenChecks * 1000 );
        simFirmware->advanceTime( timeBetweenChecks );
    }

    // 5. Handle Timeout
    LOG_ERROR("Failed HandShake - Timeout");
    return false;
}

//
// 1. Set our state to "disconnected"
// 2. Let everybody else clean up
//
bool Driver::Disconnect()
{
    // 1. Set our state to "disconnected"
    Connection.reset( nullptr );

    // 2. Let everybody else clean up
    INDI::Focuser::Disconnect();

    return true;
}

const char *Driver::getDefaultName()
{
    return (const char *)"Bee Focuser";
}

//
// Called at driver executable initialization via the
// ISGetProperties(const char *dev)
//
// Pass through to the Focuser's ISGetProperties
//
void Driver::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);
}

//
// Create properties that will show up on the INDI UI
// 
// Note - Called from DefaultDevice::ISGetProperties.
// 
bool Driver::initProperties()
{
    //
    // Set default properties for Focusers,  based on the caps we set
    // when we initialized FI::SetCapability in the constructor.
    //
    INDI::Focuser::initProperties();

    //
    // Register a custom connection for the Focuser simulator.  Run after
    // INDI::Focuser::initProperties, which registers the TCP/IP interface,
    // so the TCP/IP interface is the default.
    // 
    registerConnection( simConnection.get() );

    //
    // Connection Status.
    // 
    IUFillText( &CStatusInfoT[0], "CONNECT_STATUS", "Connection Status", getConStatus().c_str());
    IUFillTextVector( &CStatusInfoTP, CStatusInfoT, 1, getDeviceName(), "CSTATUS", 
        "Connection Status", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK );

    IUFillText( &FStatusInfoT[0], "FOCUSER_STATUS", "Focuser Status",  " " );
    IUFillText( &FStatusInfoT[1], "HOME_STATUS", "Focuser Homed", " " );
    IUFillTextVector( &FStatusInfoTP, FStatusInfoT, 2, getDeviceName(), "FSTATUS", 
        "Focuser Status", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK );

    //
    // Default settings.  New settings will be grabbed once we connect
    // 
    FocusAbsPosN[0].min = 0;
    FocusAbsPosN[0].max = 50000;
    FocusAbsPosN[0].value = 28200;
    FocusAbsPosN[0].step = 1;

    //
    // TODO - is port 4999 reasonable
    //
    tcpConnection->setDefaultPort(4999);

    // Sets the desired polling period in the DefaultDevice (POLLMS)
    setDefaultPollingPeriod(AdvanceOnTimerHit);

    return true;
}

bool Driver::AbortFocuser()
{
  if ( !isConnected() || Connection->Failed() ) return false;

  *Connection << "ABORT\n";
  if ( !Connection->Failed() )
  {
    FocusAbsPosNP.s = IPS_IDLE;
    return true;
  }
  LOG_ERROR("Network Error while aborting");
  return false;
}

bool Driver::SyncFocuser( uint32_t ticks )
{
  if ( !isConnected() || Connection->Failed() ) return false;

  *Connection << "SYNC=" << ticks << "\n";
  ignoreNextStatusUpdate=true;
  if ( !Connection->Failed() )
  {
    return true;
  }

  LOG_ERROR("Network Error while syncing");
  return false;
}

void Driver::TimerHit()
{
    simFirmware->advanceTime( POLLMS );
    timerTicks++;

    if ( !isConnected())
    {
        HardwareState nullState;
        UpdateStatusInfo( nullState );
        SetTimer(POLLMS);
        return;
    }

    if ( Connection->Failed() )
    {
        setConnected( false, IPS_ALERT );
        HardwareState nullState;
        UpdateStatusInfo( nullState );
        updateProperties();
        return;
    }

    HardwareState hwState( Connection.get() );

    // Send out a new status request if we get a status request back or
    // every 8 ticks
    if ( ( timerTicks % 8 ) == 0 || hwState.getIsSynced() )
    { 
      *Connection << "SSTATUS\n";
    }
    if ( ( timerTicks % 8 ) == 0 || hwState.getMode() )
    { 
      *Connection << "MSTATUS\n";
    }
    if ( ( timerTicks % 8 ) == 0 || hwState.getPosition() )
    {
      *Connection << "PSTATUS\n";
    }

    if ( !ignoreNextStatusUpdate )
    {
      UpdateStatusInfo( hwState );
    }
    ignoreNextStatusUpdate=false;

    SetTimer( POLLMS );
}

void Driver::UpdateStatusInfo( const HardwareState& hwState )
{
    Optional<Mode> mode = hwState.getMode();
    if ( mode )
    {
      std::string oldMode( FStatusInfoT[0].text );
      std::string currentMode( stateFriendlyName.at( *mode ).c_str());

      if ( oldMode != currentMode )
      {
        IUSaveText( &(FStatusInfoT[0]), currentMode.c_str() );
        IDSetText(&FStatusInfoTP, nullptr );
      }
      IPState newPosState = 
        currentMode == "Moving" ? IPS_BUSY :
        IPS_OK;

      if ( newPosState != FocusAbsPosNP.s )
      {
        FocusAbsPosNP.s = newPosState;
        IDSetNumber(&FocusAbsPosNP, nullptr );
      }

      if ( newPosState != FocusRelPosNP.s )
      {
        FocusRelPosNP.s = newPosState;
        IDSetNumber(&FocusRelPosNP, nullptr );
      }
    }

    std::string oldConStatus = CStatusInfoT[0].text;
    std::string curConStatus = getConStatus();

    if ( oldConStatus != curConStatus )
    {
      IUSaveText( &(CStatusInfoT[0]), curConStatus.c_str() );
      IDSetText(&CStatusInfoTP, nullptr );
    }

    Optional<bool> isSynced = hwState.getIsSynced();
    if ( isSynced )
    {
      std::string currentHomed = *isSynced ? "Synced" : "Not Synced";
      std::string oldHomed( FStatusInfoT[1].text );
      if ( oldHomed != currentHomed )
      {
          IUSaveText( &(FStatusInfoT[1]), currentHomed.c_str() );
          IDSetText(&FStatusInfoTP, nullptr );
      }
    }

    Optional<unsigned int> position = hwState.getPosition();
    if ( position )
    {
      if ( *position != FocusAbsPosN[0].value )
      {
        FocusAbsPosN[0].value = *position;
        IDSetNumber(&FocusAbsPosNP, nullptr );
      }
    }

    Optional<unsigned int> maxAbsPos = hwState.getMaxAbsPos();
    if ( maxAbsPos )
    {
      if ( *maxAbsPos != FocusMaxPosN[0].value )
      {
        FocusMaxPosN[0].value = *maxAbsPos;
        IDSetNumber(&FocusMaxPosNP, nullptr );
      }
    }
}

/************************************************************************************
 *
************************************************************************************/
bool Driver::updateProperties()
{
    defineText( &CStatusInfoTP );
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineText( &FStatusInfoTP );
    }
    else
    {
        deleteProperty( FStatusInfoTP.name );
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool Driver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
************************************************************************************/
bool Driver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Let INDI::Focuser handle any other number properties
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
************************************************************************************/
IPState Driver::MoveAbsFocuser(uint32_t targetTicks)
{
    if ( Connection->Failed() ) 
    {
        // The Timer will handle connection shutdown.
        LOGF_INFO("%s", "Failed Update Focus - connection failed");
        return IPS_ALERT;
    }

    if ( targetTicks > FocusMaxPosN[0].value )
    {
      targetTicks = FocusMaxPosN[0].value;
      LOGF_WARN("Focuser will not move past maximum value of %d", 
          (unsigned int ) FocusMaxPosN[0].value ); 
    }
    // targetTicks is unsigned, we can't go below 0.

    LOGF_INFO("Setting ABS Focus to %d", targetTicks );
    *Connection << "ABS_POS=" << targetTicks << "\n";
    ignoreNextStatusUpdate = true;

    return IPS_BUSY;
}

IPState Driver::MoveRelFocuser(FocusDirection dir, uint32_t uticks)
{
  if ( Connection->Failed() ) 
  {
    // The Timer will handle connection shutdown.
    LOGF_INFO("%s", "Failed Update Focus - connection failed");
    return IPS_ALERT;
  }

  // Firmware will handle out of bounds
  int ticks = (int) uticks;
  ticks = ( dir == FOCUS_INWARD ) ? -ticks : ticks; 
  LOGF_INFO("Changing position by %d", ticks );
  *Connection << "REL_POS=" << ticks << "\n";
  return IPS_BUSY;
}

bool Driver::mockCallHandshake()
{
  return Handshake();
}

///////////////////////////////////////////////////////////////
//
// Some random utilities
//
///////////////////////////////////////////////////////////////

const StringToMode focuserSStatusToMode = {
  { "ACCEPTING_COMMANDS",   Mode::ACCEPT_COMMANDS  },
  { "MOVING",               Mode::MOVING           }, 
  { "STOP_AT_HOME",         Mode::HOMING           }, 
  { "LOW_POWER",            Mode::LOW_POWER        }, 
};

const ModeToString stateFriendlyName = {
  { Mode::UNCONNECTED,       " "                           },
  { Mode::ACCEPT_COMMANDS,   "Ready"                       },
  { Mode::MOVING,            "Moving"                      },
  { Mode::HOMING,            "Searching for Home Position" },
  { Mode::LOW_POWER,         "Ready (Low Power Mode)"      },
};

HardwareState::HardwareState( BeeFocusedCon::Interface* connection )
{
  while ( connection->DataReady())
  {
    std::string input = GetString( *connection );
    std::vector<std::string> tokens = Tokenize( input );
    if (tokens.size() >= 2 )
    {
      std::string& verb = tokens[0];
      std::string& noun = tokens[1];
      if ( verb == "State:" )
      {
        bool found=focuserSStatusToMode.find( noun ) != focuserSStatusToMode.end();
        mode = !found ? Mode::ERROR : focuserSStatusToMode.at( noun );
      }
      if ( verb == "Position:" )
      {
        int pos = std::stoi( noun );
        currentPos = pos > 0 ? pos : 0;
      }
      if ( verb == "Synched:" )
      {
        isSynced = ( noun == "YES" ) ? true : false;
      }
      if ( verb == "MaxPos:" )
      {
        maxAbsPos = std::stoi( noun );
      }
    }
  }
}

std::vector<std::string> Tokenize( const std::string& s)
{
    std::vector< std::string > tokens;
    std::regex token_regex("(\\S+)");
    auto begin = std::sregex_iterator( s.begin(), s.end(), token_regex );

    for ( auto i = begin; i != std::sregex_iterator(); ++i )
    {
      tokens.push_back( i->str() );
    }
    return tokens;
}

}

