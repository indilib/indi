/*
    INDI IEQ Pro driver

    Copyright (C) 2015 Jasem Mutlaq

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

#ifndef IEQPRO_H
#define IEQPRO_H

#include "inditelescope.h"
#include "indiguiderinterface.h"
#include "indicontroller.h"
#include "ieqprodriver.h"

class IEQPro : public INDI::Telescope, public INDI::GuiderInterface
{
 public:
  IEQPro();
  ~IEQPro();

 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:

 virtual const char *getDefaultName();

 virtual bool initProperties();
 virtual bool updateProperties();

 virtual bool Connect(const char *port, uint32_t baud);
 virtual bool Disconnect();

 virtual bool ReadScopeStatus();

 virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
 virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);

 virtual bool saveConfigItems(FILE *fp);

 virtual bool Park();
 virtual bool UnPark();

 virtual bool Sync(double ra, double dec);
 virtual bool Goto(double, double);
 virtual bool Abort();

 virtual bool updateTime(ln_date * utc, double utc_offset);
 virtual bool updateLocation(double latitude, double longitude, double elevation);

 virtual void debugTriggered(bool enable);
 virtual void simulationTriggered(bool enable);

  // Parking
  virtual void SetCurrentPark();
  virtual void SetDefaultPark();

  // Slew Rate
  bool SetSlewRate(int index);

 // Sim
 void mountSim();

 // Guide
 virtual IPState GuideNorth(float ms);
 virtual IPState GuideSouth(float ms);
 virtual IPState GuideEast(float ms);
 virtual IPState GuideWest(float ms);

private:

  /**
  * @brief getStartupData Get initial mount info on startup.
  */
 void getStartupData();

 /* Firmware */
 IText   FirmwareT[5];
 ITextVectorProperty FirmwareTP;

 /* Tracking Mode */
 ISwitchVectorProperty TrackModeSP;
 ISwitch TrackModeS[4];

 /* Custom Tracking Rate */
 INumber CustomTrackRateN[1];
 INumberVectorProperty CustomTrackRateNP;

 /* GPS Status */
 ISwitch GPSStatusS[3];
 ISwitchVectorProperty GPSStatusSP;

 /* Time Source */
 ISwitch TimeSourceS[3];
 ISwitchVectorProperty TimeSourceSP;

 /* Hemisphere */
 ISwitch HemisphereS[2];
 ISwitchVectorProperty HemisphereSP;

 /* Home Control */
 ISwitch HomeS[3];
 ISwitchVectorProperty HomeSP;

 /* Guide Rate */
 INumber GuideRateN[1];
 INumberVectorProperty GuideRateNP;

 unsigned int DBG_SCOPE;
 bool sim;
 bool timeUpdated, locationUpdated;
 double currentRA, currentDEC;
 double targetRA,targetDEC;
 double parkRA, parkDEC;

 IEQInfo scopeInfo;
 FirmwareInfo firmwareInfo;

};

#endif
 
