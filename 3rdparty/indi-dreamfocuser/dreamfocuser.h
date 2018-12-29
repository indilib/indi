/*
  INDI Driver for DreamFocuser

  Copyright (C) 2016 Piotr Dlugosz

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

#ifndef DREAMFOCUSER_H
#define DREAMFOCUSER_H

#include <string>

#include <indidevapi.h>
#include <indicom.h>
#include <indifocuser.h>

using namespace std;

#define DREAMFOCUSER_STEP_SIZE      32
#define DREAMFOCUSER_ERROR_BUFFER   1024


class DreamFocuser : public INDI::Focuser
{

public:

  struct DreamFocuserCommand  {
    char M = 'M';
    char k;
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;
    unsigned char n = '\0';
    unsigned char z;
  };

  DreamFocuser();
  ~DreamFocuser();

  // Standard INDI interface functions
  virtual bool Connect();
  virtual bool Disconnect();
  const char *getDefaultName();
  virtual bool initProperties();
  virtual bool updateProperties();
  virtual bool saveConfigItems(FILE *fp);
  //virtual void ISGetProperties(const char *dev);
  virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
  virtual void TimerHit();

  //virtual bool SetFocuserSpeed(int speed);
  //virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
  virtual IPState MoveAbsFocuser(uint32_t ticks);
  virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
  virtual bool AbortFocuser();

private:

  INumber MaxTravelN[1];
  INumberVectorProperty MaxTravelNP;

  INumber MaxPositionN[1];
  INumberVectorProperty MaxPositionNP;

  INumber EnvironmentN[2];
  INumberVectorProperty EnvironmentNP;

  ISwitch SyncS[1];
  ISwitchVectorProperty SyncSP;

  ISwitch ParkS[1];
  ISwitchVectorProperty ParkSP;

  ISwitch StatusS[2];
  ISwitchVectorProperty StatusSP;

  //INumber SetBacklashN[1];
  //INumberVectorProperty SetBacklashNP;

  unsigned char calculate_checksum(DreamFocuserCommand c);
  bool send_command(char k, uint32_t l = 0);
  bool read_response();
  bool dispatch_command(char k, uint32_t l = 0);

  bool getTemperature();
  bool getStatus();
  bool getPosition();
  bool setPosition(int32_t position);
  bool setSync(int32_t position = 0);
  bool setPark();

  // Variables
  float focusMoveRequest;
  string default_port;
  int fd;
  float simulatedTemperature, currentTemperature;
  float simulatedHumidity, currentHumidity;
  int32_t simulatedPosition, currentPosition;
  bool isAbsolute;
  bool isMoving;
  DreamFocuserCommand currentResponse;
};

#endif
