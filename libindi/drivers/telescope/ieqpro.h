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
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool ISSnoopDevice(XMLEle *root);

  static void joystickHelper(const char * joystick_n, double mag, double angle, void *context);
  static void buttonHelper(const char * button_n, ISState state, void *context);

protected:

 virtual const char *getDefaultName();

 virtual bool initProperties();
 virtual bool updateProperties();

 virtual bool Connect(const char *port);
 virtual bool Disconnect();

 virtual bool ReadScopeStatus();

 virtual bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command);
 virtual bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command);


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

 // Guide
 virtual bool GuideNorth(float ms);
 virtual bool GuideSouth(float ms);
 virtual bool GuideEast(float ms);
 virtual bool GuideWest(float ms);

 // Joystick
 virtual void processNSWE(double mag, double angle);
 virtual void processJoystick(const char * joystick_n, double mag, double angle);
 virtual void processButton(const char * button_n, ISState state);

private:

  /**
  * @brief getStartupData Get initial mount info on startup.
  */
 void getStartupData();

 /**
  * @brief setSlewRate Set N/W/E/S hand controller slew rates
  * @param rate 1 to 9
  */
 bool setSlewRate(IEQ_SLEW_RATE rate);

 /* Firmware */
 IText   FirmwareT[5];
 ITextVectorProperty FirmwareTP;

 /* Park Coords */
 INumber ParkN[2];
 INumberVectorProperty ParkNP;

 /* Slew Rate */
 ISwitchVectorProperty SlewRateSP;
 ISwitch SlewRateS[9];

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

 INDI::Controller *controller;

 unsigned int DBG_SCOPE;
 bool sim;
 bool timeUpdated, locationUpdated;
 double currentRA, currentDEC;
 double targetRA,targetDEC;

 IEQInfo scopeInfo;
 FirmwareInfo firmwareInfo;

};

#endif
 
