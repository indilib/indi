/*
    Focus Lynx INDI driver
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef FOCUSLYNXBASE_H
#define FOCUSLYNXBASE_H

#include <map>
#include "indibase/indifocuser.h"

class FocusLynxBase : public INDI::Focuser
{
public:
  FocusLynxBase();
  FocusLynxBase(const char * target);
  ~FocusLynxBase();
  
  enum { FOCUS_A_COEFF, FOCUS_B_COEFF, FOCUS_C_COEFF, FOCUS_D_COEFF, FOCUS_E_COEFF, FOCUS_F_COEFF };
  typedef enum { STATUS_MOVING, STATUS_HOMING, STATUS_HOMED, STATUS_FFDETECT, STATUS_TMPPROBE, STATUS_REMOTEIO, STATUS_HNDCTRL, STATUS_REVERSE, STATUS_UNKNOWN} LYNX_STATUS;
  enum { GOTO_CENTER, GOTO_HOME };
  
  virtual bool Connect();
  virtual bool Disconnect();
  const char * getDefaultName();
  virtual bool initProperties();
  virtual void ISGetProperties(const char *dev);
  virtual bool updateProperties();
  virtual bool saveConfigItems(FILE *fp);
  
  virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    
  virtual IPState MoveAbsFocuser(uint32_t ticks);
  virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
  virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
  virtual bool AbortFocuser();
  virtual void TimerHit();
  virtual bool RemoteDisconnect();
  virtual bool RemoteConnect();  
  virtual int getVersion(int *major, int *minor, int *sub);
  
  void setFocusTarget(const char *target);
  const char * getFocusTarget();  
  void debugTriggered(bool enable);
  

  // Device
  bool setDeviceType(int index);
  uint32_t DBG_FOCUS;


  // Misc functions
  bool ack();
  bool isResponseOK();
  
  bool isFromRemote;  
  
protected:  
  // Move from private to public to validate
  int PortFD;
  bool configurationComplete;
   
  // List all supported models
  ISwitch *ModelS;
  ISwitchVectorProperty ModelSP;
    
private:
  
  uint32_t simPosition;
  uint32_t targetPosition;
  uint32_t maxControllerTicks;
  
  ISState simStatus[8];
  bool simCompensationOn;
  char focusTarget[8];
  
  //double targetPos, lastPos, lastTemperature, simPosition;
  //unsigned int currentSpeed, temperaturegetCounter;
  
  std::map<std::string, std::string> lynxModels;
//  std::map<std::string, std::string> lynxModelsF2;
  
  struct timeval focusMoveStart;
  float focusMoveRequest;
  
  // Get functions
  bool getFocusConfig();
  bool getFocusStatus();
  
  // Set functions
    
  // Position
  bool setFocusPosition(u_int16_t position);
  
  // Temperature
  bool setTemperatureCompensation(bool enable);
  bool setTemperatureCompensationMode(char mode);
  bool setTemperatureCompensationCoeff(char mode, int16_t coeff);
  bool setTemperatureCompensationOnStart(bool enable);
  
  // Backlash
  bool setBacklashCompensation(bool enable);
  bool setBacklashCompensationSteps(uint16_t steps);
  
  // Sync
  bool sync(uint32_t position);
  
  // Motion functions
  bool stop();
  bool home();
  bool center();
  bool reverse(bool enable);
  
  // Led level
  bool setLedLevel(int level);
  
  // Device Nickname
  bool setDeviceNickname(const char * nickname);
  
  // Misc functions
  bool resetFactory();
  float calcTimeLeft(timeval,float);
  
  // Properties
  
  // Set/Get Temperature
  INumber TemperatureN[1];
  INumberVectorProperty TemperatureNP;
  
  // Enable/Disable temperature compnesation
  ISwitch TemperatureCompensateS[2];
  ISwitchVectorProperty TemperatureCompensateSP;
  
  // Enable/Disable temperature compnesation on start
  ISwitch TemperatureCompensateOnStartS[2];
  ISwitchVectorProperty TemperatureCompensateOnStartSP;
  
  // Temperature Coefficient
  INumber TemperatureCoeffN[5];
  INumberVectorProperty TemperatureCoeffNP;
  
  // Temperature Coefficient Mode
  ISwitch TemperatureCompensateModeS[5];
  ISwitchVectorProperty TemperatureCompensateModeSP;
  
  // Enable/Disable backlash
  ISwitch BacklashCompensationS[2];
  ISwitchVectorProperty BacklashCompensationSP;
  
  // Backlash Value
  INumber BacklashN[1];
  INumberVectorProperty BacklashNP;
  
  // Reset to Factory setting
  ISwitch ResetS[1];
  ISwitchVectorProperty ResetSP;
  
  // Reverse Direction
  ISwitch ReverseS[2];
  ISwitchVectorProperty ReverseSP;
  
  // Go to home/center
  ISwitch GotoS[2];
  ISwitchVectorProperty GotoSP;
  
  // Status indicators
  ILight StatusL[8];
  ILightVectorProperty StatusLP;
  
  // Sync to a particular position
  INumber SyncN[1];
  INumberVectorProperty SyncNP;
  
  // Max Travel for relative focusers
  INumber MaxTravelN[1];
  INumberVectorProperty MaxTravelNP;
  
  // Focus name configure in the HUB
  IText HFocusNameT[1];
  ITextVectorProperty HFocusNameTP;

  // Led Intensity Value
  INumber LedN[1];
  INumberVectorProperty LedNP;

  bool isAbsolute;
  bool isSynced;
  bool isHoming;
  
};

#endif // FOCUSLYNXBASE_H
