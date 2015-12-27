/*
    Celestron GPS
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

/*
    Version with experimental pulse guide support. GC 04.12.2015
*/

#ifndef CELESTRONGPS_H
#define CELESTRONGPS_H

#include <inditelescope.h>
#include "indidevapi.h"
#include "indicom.h"
#include "indicontroller.h"

#include "celestrondriver.h"

#define	POLLMS		1000		/* poll period, ms */

//GUIDE: guider header
#include "indiguiderinterface.h"

//GUIDE: guider parent
class CelestronGPS : public INDI::Telescope, public INDI::GuiderInterface
{
 public:
 CelestronGPS();
 virtual ~CelestronGPS() {}

 virtual const char *getDefaultName();
 virtual bool Connect();
 virtual bool Connect(const char * port, uint32_t baud);
 virtual bool Disconnect();
 virtual bool ReadScopeStatus();
 virtual void ISGetProperties(const char *dev);
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool initProperties();
 virtual bool updateProperties();

 //GUIDE guideTimeout() funcion
 void guideTimeout(CELESTRON_DIRECTION calldir);
 
protected:

 // Goto, Sync, and Motion
 bool Goto(double ra,double dec);
 bool GotoAzAlt(double az, double alt);
 bool Sync(double ra, double dec);
 virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
 virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
 virtual bool Abort();

 // Time and Location
 virtual bool updateLocation(double latitude, double longitude, double elevation);
 virtual bool updateTime(ln_date *utc, double utc_offset);

 //GUIDE: guiding functions
 virtual IPState GuideNorth(float ms);
 virtual IPState GuideSouth(float ms);
 virtual IPState GuideEast(float ms);
 virtual IPState GuideWest(float ms);

 //GUIDE guideTimeoutHelper() function
 static void guideTimeoutHelperN(void *p);
 static void guideTimeoutHelperS(void *p);
 static void guideTimeoutHelperW(void *p);
 static void guideTimeoutHelperE(void *p);
 
 // Parking
 virtual bool Park();
 virtual bool UnPark();
 virtual void SetCurrentPark();
 virtual void SetDefaultPark();

 virtual bool saveConfigItems(FILE *fp);

 virtual void simulationTriggered(bool enable);

 void mountSim();

 //GUIDE variables.
 int    GuideNSTID;
 int    GuideWETID;
 CELESTRON_DIRECTION guide_direction;
 
 /* Firmware */
 IText   FirmwareT[5];
 ITextVectorProperty FirmwareTP;

 INumberVectorProperty HorizontalCoordsNP;
 INumber HorizontalCoordsN[2];

 ISwitch TrackS[4];
 ISwitchVectorProperty TrackSP;

 //GUIDE Pulse guide switch
 ISwitchVectorProperty UsePulseCmdSP;
 ISwitch UsePulseCmdS[2];

 ISwitchVectorProperty UseHibernateSP;
 ISwitch UseHibernateS[2];
 
private:  

 bool setTrackMode(CELESTRON_TRACK_MODE mode);

  int PortFD;
  double currentRA, currentDEC, currentAZ, currentALT;
  double targetRA, targetDEC, targetAZ, targetALT;

  FirmwareInfo fwInfo;

};

#endif

