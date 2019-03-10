#ifndef __WIFI_DEBUG_OSTREAM__
#define __WIFI_DEBUG_OSTREAM__

#include "simple_ostream.h"
#include "net_interface.h"
#include "debug_interface.h"

/// @brief Wifi target debug ostream
///
class WifiDebugOstream	
{
  public:
  WifiDebugOstream( DebugInterface* serialDebugArg, NetInterface* wifiDebugArg  )
    : m_wifiDebug{ wifiDebugArg }, 
      m_serialDebug{ serialDebugArg},
      m_lastWasNewline{ true }
  {
  }

  WifiDebugOstream& operator<<( char c )
  { 
    *(m_serialDebug) << c;
    bool isNewLine = ( c == '\n' );
    if ( m_lastWasNewline && !isNewLine )
    {
      (*m_wifiDebug) << "# ";
    }
    *(m_wifiDebug) << c;
    m_lastWasNewline = isNewLine;
    return *this;
  }

  private:
  NetInterface* m_wifiDebug;
  DebugInterface* m_serialDebug;
  bool m_lastWasNewline;
};

#endif

