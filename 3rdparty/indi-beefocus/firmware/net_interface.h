#ifndef __NetInterface_H__
#define __NetInterface_H__

#include <string>
#include "hardware_interface.h"
#include "debug_interface.h"

class WifiOstream;
class WifiDebugOstream;

/// @brief Interface to the client
///
/// This class's one job is to provide an interface to the client.
///
class NetInterface {
  public:

  NetInterface()
  {
  }
  ~NetInterface()
  {
  }

  virtual void setup( DebugInterface &debugLog ) = 0;

  virtual bool getString( WifiDebugOstream &log, std::string& string ) = 0;
  virtual NetInterface& operator<<( char c ) = 0;

  private:
};

class NetConnection {
  public:

  NetConnection& operator<<( char c )
  {
    putChar( c );
    return *this;
  }
  virtual bool getString( WifiDebugOstream& lot, std::string& string )=0;
  virtual operator bool( void ) = 0;
  virtual void reset( void ) = 0;

  protected:

  virtual void putChar( char c ) = 0;

};

#endif

