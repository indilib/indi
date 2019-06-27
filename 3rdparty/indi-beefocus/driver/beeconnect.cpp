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

#include "indifocuser.h"
#include "indicom.h"
#include "unistd.h"
#include <memory>
#include <string.h>
#include <string>
#include <unordered_map>
#include <functional>
#include "beeconnect.h"

namespace BeeFocusedCon {

using TTYErrorToString= std::unordered_map< const int, const std::string, EnumHash >;

static const TTYErrorToString errorReason =
{
  { TTY_OK,                   "TTY_OK (no error)" },
  { TTY_READ_ERROR,           "TTY_READ_ERROR" },
  { TTY_WRITE_ERROR,          "TTY_WRITE_ERROR" },
  { TTY_SELECT_ERROR,         "TTY_SELECT_ERROR" },
  { TTY_TIME_OUT,             "TTY_TIME_OUT" },
  { TTY_PORT_FAILURE,         "TTY_PORT_FAILURE" },
  { TTY_PARAM_ERROR,          "TTY_PARAM_ERROR }" },
  { TTY_ERRNO,                "TTY_ERRNO" },
  { TTY_OVERFLOW,             "TTY_OVERFLOW" }
};

static std::string GetIndiErrorReason( int errorID )
{
  return errorReason.find( errorID ) == errorReason.end()  ?
    std::string("Unknown Error Code ") + std::to_string( errorID ) :
    errorReason.at( errorID );
} 

bool TCP::DataReady( void ) 
{
  if ( Failed() )
  {
    return false;
  }
  int status = tty_timeout( fd, 0 );
  if ( status != TTY_OK && status != TTY_TIME_OUT )
  {
    Fail("Error on DataReady (" + GetIndiErrorReason( status ) +")");
    return false;
  }
  return status == TTY_OK;
}

Interface& TCP::operator<<( char c ) 
{
  if ( Failed() )
  {
    return *this;
  }

  // tty_write will output data until it succeeds or fails.
  int nbytes_written = 0;
  int status = tty_write( fd, &c, 1, &nbytes_written );

  if ( status != TTY_OK )
  {
    Fail("Error on Write (" + GetIndiErrorReason( status ) +")");
  }
  else if ( nbytes_written != 1 ) 
  {
    // Should never happen.
    Fail("tty_write wrote 0 bytes, expected 1");
  }

  return *this;
}

Interface& TCP::operator>>(char &c )
{
  c = 0;
  if ( Failed() )
  {
    return *this;
  }

  int nbytes_read = 0;
  int status = tty_read( fd, &c, 1, GetTimeout(), &nbytes_read );

  if ( status != TTY_OK )
  {
    Fail( "Error on Read (" + GetIndiErrorReason( status ) +")" );
  }
  else if ( nbytes_read == 0 )
  {
    // Should never happen.
    Fail( "Error on Read - Expected 1 byte, got 0" );
  }
  return *this;
}

int TCP::GetTimeout( void )
{
  return 10;
}

//================================================================

Interface& Sim::operator<<( char c )
{
  toFirmware.push(c);
  return *this;
}

Interface& Sim::operator>>( char &c )
{
  // Handle an empty input by erroring out...
  // This doesn't match reality - in reality we'd block until the other
  // end of the connection wrote something, timing out.  I could mock
  // that by implementing the connection as a pipe and running both
  // ends of the connection in their own thread, or some other kind of
  // magic.  But I want to use this for integration testing, and I'm
  // worried that adding concurrency to the test would reduce the chances
  // of reproducing any issue that came up.

  if ( fromFirmware.empty() )
  {
    Fail( "Read called when mock queue was empty" );
  }
  c = fromFirmware.front();
  fromFirmware.pop();
  return *this;
}

bool Sim::DataReady()
{
  return !fromFirmware.empty();
}

//================================================================

static std::string GetStringRaw( Interface& con )
{
  std::string rval;

  for ( ;; )
  {
    char c;
    con >> c;
    if ( con.Failed())
    {
      return std::string();
    }
    if ( c == '\n' )
    {
      break;
    }
    rval.push_back(c);
  }
  return rval;
}

std::string GetString( Interface& con )
{
  std::string rval;
  rval = GetStringRaw( con );
  return (con.Failed() || rval.find("#")) == 0 ? std::string() : rval;
}

Interface& operator<<( Interface& ostream, const char* string )
{
  std::size_t length = strlen( string );
  for ( std::size_t i = 0; i < length; ++i )
  {
    ostream << string[i];
  }
  return ostream;
}

Interface& operator<<( Interface& ostream, unsigned int i )
{
  // Handle digits that aren't the lowest digit (if any)
  if ( i >= 10 )
  {
    ostream << i/10;
  }

  // Handle the lowest digit
  ostream << (char) ('0' + (i % 10));
  return ostream;
}

Interface& operator<<( Interface& ostream, int i )
{
  if ( i < 0 ) {
    ostream << "-";
    i = -i;
  }
  ostream << (unsigned int) i;
  return ostream;
}
}

//================================================================

namespace Connection {

Sim::Sim(INDI::DefaultDevice *dev ) : Interface(dev, CONNECTION_CUSTOM)
{
}

bool Sim::Connect()
{
  bool rc = Handshake();
  if ( rc )
  {
    LOGF_INFO("%s is online.", getDeviceName());
  }
  else
  {
    LOG_DEBUG("Handshake failed.");
  }
  return rc;
}

bool Sim::Disconnect()
{
  return false;
}

void Sim::Activated()
{
}

void Sim::Deactivated()
{
}

}

