#include "net_interface.h"
#include "net_esp8266.h"
#include "wifi_ostream.h"
#include "wifi_debug_ostream.h"

void WifiInterfaceEthernet::setup( DebugInterface& log ) {
  delay(10);

  log << "Init Wifi\n";

  // Connect to WiFi network
  log << "Connecting to " << ssid << "\n";

  // Disable Wifi Persistence.  It's not needed and wears the flash memory.
  // Kudos Erik H. Bakke for pointing this point.
  WiFi.persistent( false );
  WiFi.mode( WIFI_STA );
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    log << ".";
  }
  log << "\n";
  log << "WiFi Connected\n";
   
  // Start the server
  m_server.begin();
  log << "Server started\n";

  // Print the IP address
  BeeFocus::IpAddress adr;
  auto dsIP = WiFi.localIP();
  for ( int i = 0; i < 4; ++ i )
    adr[i] = dsIP[i];
  log << "Telnet to this address to connect: " << adr << " " << tcp_port << "\n";

}

bool WifiInterfaceEthernet::getString( WifiDebugOstream& log, std::string& string )
{
  handleNewConnections( log );
  return std::any_of( m_connections.begin(), m_connections.end(), [&] ( NetConnection& connection )
  {
    return connection.getString( log, string );
  });
}

void WifiInterfaceEthernet::handleNewConnections( WifiDebugOstream &log )
{
  if ( m_server.hasClient() )
  {  
    log << "New client connecting\n";
   
    ConnectionArray::iterator slot = 
      std::find_if( m_connections.begin(), m_connections.end(), [&] ( NetConnection& connection )
      {
        return !connection;
      });

    if ( slot == m_connections.end() )
    {
      slot = m_nextToKick;      
      m_nextToKick++;
      m_nextToKick = ( m_nextToKick == m_connections.end()) ? m_connections.begin() : m_nextToKick;
    }
    
    log << "Using slot " << slot - m_connections.begin() << " of " << m_connections.size()-1 << " for the new client\n";

    if ( *slot )
    {
      log << "An existing client exists - disconnecting it\n";
    }

    slot->initConnection( m_server );
  }
}

WifiInterfaceEthernet& WifiInterfaceEthernet::operator<<( char c )
{
  std::for_each( m_connections.begin(), m_connections.end(), [&] ( NetConnection& interface )
  {
    interface << c;
  }); 
  return *this;
}

void WifiInterfaceEthernet::reset(void)
{
  std::for_each( m_connections.begin(), m_connections.end(), [] ( NetConnection& interface )
  {
    interface.reset(); 
  });
}

void WifiConnectionEthernet::initConnection( WiFiServer &server )
{
  if ( m_connectedClient )
  {
      (*this) << "# New Client and no free slots - Dropping Your Connection.\n";
      m_connectedClient.stop();
  }
  m_connectedClient = server.available();
  (*this) << "# Bee Focuser is ready for commands\n";  
}

bool WifiConnectionEthernet::getString( WifiDebugOstream &log, std::string& string )
{
  handleNewIncomingData( log );

  std::string& incomingBuffer = m_incomingBuffers[ m_currentIncomingBuffer ];
  size_t newLine = incomingBuffer.find('\n');
  if ( newLine != std::string::npos )
  {
    // log << "Found newline at " << newLine << "\n";
    string.replace( 0, newLine, incomingBuffer );
    string.resize( newLine );
    m_currentIncomingBuffer = 1-m_currentIncomingBuffer;
    std::string& newIncomingBuffer = m_incomingBuffers[ m_currentIncomingBuffer ];
    size_t rest = incomingBuffer.length() - newLine - 1;
    newIncomingBuffer.replace( 0, rest, incomingBuffer, newLine+1, rest );
    newIncomingBuffer.resize( rest );
    
    return true;
  }
  
  return false;
}


void WifiConnectionEthernet::handleNewIncomingData( WifiDebugOstream& log )
{
  (void) log; // silence argsused warning
  std::string& incomingBuffer = m_incomingBuffers[ m_currentIncomingBuffer ];
  
  if ( !m_connectedClient || !m_connectedClient.available())
  {
    return;
  }

  while (m_connectedClient.available())
  {
    uint8_t byte;
    m_connectedClient.read( &byte, 1 );
    incomingBuffer += ((char) byte);
  }
}



