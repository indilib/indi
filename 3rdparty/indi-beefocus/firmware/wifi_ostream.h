#ifndef __WifiOstream_H__
#define __WifiOstream_H__

#include "simple_ostream.h"

class WifiOstream {
  public:

  WifiOstream() : m_connectedClient{nullptr}
  {
  }

  ~WifiOstream()
  {
    m_connectedClient = nullptr;
  }

  WifiOstream& operator<<( char c )
  {
    if ( m_connectedClient )
    {
      *m_connectedClient << c;
    }

    return *this;
  }

  void setConnectedClientAlias( NetConnection* connectedClient )
  {
    m_connectedClient = connectedClient;
  }

  private:
    NetConnection* m_connectedClient;
};

#endif

