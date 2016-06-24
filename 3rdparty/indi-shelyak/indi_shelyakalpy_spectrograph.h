#ifndef SHELYAKALPY_SPECTROGRAPH_H
#define SHELYAKALPY_SPECTROGRAPH_H
//On serial connection
//11\n : calib lamp on
//21\n : flat lamp on
//31\n : dark on
//(and the opposite, 10, 20, 30)
//00 : shut off all

//
#include <map>

#include "defaultdevice.h"

std::map<ISState, char> COMMANDS = {
  {ISS_ON, 0x31}, {ISS_OFF, 0x30}    //"1" and "0"
};
std::map<std::string, char> PARAMETERS = {
  {"DARK", 0x31}, {"ARNE", 0x32}, {"TUNGSTEN", 0x33} //"1", "2", "3"
};

class ShelyakAlpy : public INDI::DefaultDevice
{
public:
  ShelyakAlpy();
  ~ShelyakAlpy();

  void ISGetProperties (const char *dev);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

protected:
  const char *getDefaultName();

  bool initProperties();
  bool updateProperties();

  bool Connect();
  bool Disconnect();

private:
  int PortFD; // file descriptor for serial port

  // Main Control
  ISwitchVectorProperty LampSP;
  ISwitch LampS[3];

  // Options
  ITextVectorProperty PortTP;
  IText PortT[1];

  // Spectrograph Settings
  INumberVectorProperty SettingsNP;
  INumber SettingsN[2];

  bool calibrationUnitCommand(char command, char parameter);

};

#endif // SHELYAKALPY_SPECTROGRAPH_H
