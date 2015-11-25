/*
    LX200 Generic
    Copyright (C) 2003-2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef LX200GENERIC_H
#define LX200GENERIC_H

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "indicontroller.h"

#include "indidevapi.h"
#include "indicom.h"

class LX200Generic: public INDI::Telescope, public INDI::GuiderInterface
{
 public:
    LX200Generic();
    virtual ~LX200Generic();

    virtual const char *getDefaultName();
    virtual bool Connect();
    virtual bool Connect(const char *port, uint16_t baud);
    virtual bool Disconnect();
    virtual bool ReadScopeStatus();
    virtual void ISGetProperties(const char *dev);
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

    void updateFocusTimer();
    void guideTimeout();

  protected:

    virtual bool SetSlewRate(int index);
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual bool updateTime(ln_date * utc, double utc_offset);
    virtual bool updateLocation(double latitude, double longitude, double elevation);

    virtual IPState GuideNorth(float ms);
    virtual IPState GuideSouth(float ms);
    virtual IPState GuideEast(float ms);
    virtual IPState GuideWest(float ms);

    virtual bool Goto(double,double);
    virtual bool Park();
    virtual bool Sync(double ra, double dec);

    virtual bool isSlewComplete();
    virtual bool checkConnection();

    virtual void debugTriggered(bool enable);    

    virtual void getBasicData();
    void slewError(int slewCode);
    void getAlignment();
    void sendScopeTime();
    void sendScopeLocation();
    void mountSim();

    static void updateFocusHelper(void *p);
    static void guideTimeoutHelper(void *p);

    int    GuideNSTID;
    int    GuideWETID;

    uint32_t updatePeriodMS;                        // Period in milliseconds to call ReadScopeStatus()

    int timeFormat;
    int currentSiteNum;
    int trackingMode;
    long guide_direction;

    unsigned int DBG_SCOPE;

    double JD;
    double targetRA, targetDEC;
    double currentRA, currentDEC;
    int MaxReticleFlashRate;

  /* Telescope Alignment Mode */
  ISwitchVectorProperty AlignmentSP;
  ISwitch AlignmentS[3];

  /* Tracking Mode */
  ISwitchVectorProperty TrackModeSP;
  ISwitch TrackModeS[4];

  /* Tracking Frequency */
  INumberVectorProperty TrackingFreqNP;
  INumber TrackFreqN[1];

  /* Use pulse-guide commands */
  ISwitchVectorProperty UsePulseCmdSP;
  ISwitch UsePulseCmdS[2];

  /* Site Management */
  ISwitchVectorProperty SiteSP;
  ISwitch SiteS[4];

  /* Site Name */
  ITextVectorProperty SiteNameTP;
  IText   SiteNameT[1];

  /* Focus motion */
  ISwitchVectorProperty	FocusMotionSP;
  ISwitch  FocusMotionS[2];

  /* Focus Timer */
  INumberVectorProperty FocusTimerNP;
  INumber  FocusTimerN[1];

  /* Focus Mode */
  ISwitchVectorProperty FocusModeSP;
  ISwitch  FocusModeS[3];

};

#endif
