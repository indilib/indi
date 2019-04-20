#include "beesimfirmware.h"

class MockFirmwareHardware: public HWI
{
  public:

  MockFirmwareHardware() : count{ 0 }
  {
  }

  void DigitalWrite( HWI::Pin pin, HWI::PinState state ) override
  {
    // -Wunused-parameter - dumbest warning ever.
    (void)pin;
    (void)state;
  }
  void PinMode( HWI::Pin pin, HWI::PinIOMode mode ) override
  {
    // -Wunused-parameter - dumbest warning ever.
    (void)pin;
    (void)mode;
  }

  HWI::PinState DigitalRead( Pin pin ) override
  {
    assert( pin == HWI::Pin::HOME );
    count += 1;
    return count < 2000 ? HWI::PinState::HOME_INACTIVE : HWI::PinState::HOME_ACTIVE;
  }

  private:
  int count;
};

class MockFirmwareWifi: public NetInterface
{
  public:

  MockFirmwareWifi(
    BeeFocusedCon::QueueOfChar* toFirmwareRHS,
    BeeFocusedCon::QueueOfChar* fromFirmwareRHS
  ) :
    toFirmware( toFirmwareRHS ),
    fromFirmware( fromFirmwareRHS )
  {
  }

  void setup( DebugInterface& debugLog ) override
  {
    (void)debugLog;
  }

  bool getString( WifiDebugOstream& log, std::string& returnString ) override
  {
    (void)log;

    while ( !toFirmware->empty())
    {
      char top = toFirmware->front();
      toFirmware->pop();
      if ( top == '\n' )
      {
        returnString = outputString;
        outputString = "";
        return true;
      }
      outputString += top;
    }
    return false;
  }

  NetInterface& operator<<( char c ) override
  {
    fromFirmware->push(c);
    return *this;
  }

  private:
    BeeFocusedCon::QueueOfChar* toFirmware;
    BeeFocusedCon::QueueOfChar* fromFirmware;
    std::string outputString;
};


class MockFirmwareDebug: public DebugInterface
{
  public:

  void rawWrite( const char* bytes, std::size_t numBytes ) override
  {
    // Silence -Wunused-parameter 
    (void)bytes;
    (void)numBytes;
  }
};


//
// Bring up the simulated focuser.  This is actually the ESP8266
// firmware sitting on the end of an emulated WiFi connection
//
BeeSimFirmware::BeeSimFirmware(
  BeeFocusedCon::QueueOfChar* toFirmware,
  BeeFocusedCon::QueueOfChar* fromFirmware ) : time{ 0 }, simulatedFocuserNextUpdate{ 0 }
{
  std::unique_ptr<MockFirmwareDebug>
    mockFirmwareDebug( new MockFirmwareDebug );
  std::unique_ptr<MockFirmwareHardware>
    mockFirmwareHardware( new MockFirmwareHardware);
  std::unique_ptr<MockFirmwareWifi>
    mockFirmwareWifi( new MockFirmwareWifi( toFirmware, fromFirmware ) );
  FS::BuildParams params( FS::Build::LOW_POWER_HYPERSTAR_FOCUSER );
  simulatedFocuser.reset( new FS::Focuser(
    std::move( mockFirmwareWifi ),
    std::move( mockFirmwareHardware ),
    std::move( mockFirmwareDebug ),
    params
  ));
  simulatedFocuserNextUpdate = 0;
}

void BeeSimFirmware::advanceTime( unsigned int amount )
{
  time += amount;

  if ( simulatedFocuser )
  {
    while ( simulatedFocuserNextUpdate / 1000 <= time )
    {
      simulatedFocuserNextUpdate+=simulatedFocuser->loop();
    }
  }
}


