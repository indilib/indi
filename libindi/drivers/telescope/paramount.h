/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Driver for using TheSky6 Pro Scripted operations for mounts via the TCP server.
 While this technically can operate any mount connected to the TheSky6 Pro, it is
 intended for Paramount mounts control.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef PARAMOUNT_H
#define PARAMOUNT_H

#include "indibase/inditelescope.h"
#include "indiguiderinterface.h"
#include "indicontroller.h"

class Paramount : public INDI::Telescope, public INDI::GuiderInterface
{
public:
    Paramount();
    virtual ~Paramount();

    virtual const char *getDefaultName();
    virtual bool Handshake();
    virtual bool ReadScopeStatus();
    virtual bool initProperties();
    virtual bool updateProperties();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual bool updateLocation(double latitude, double longitude, double elevation);
    virtual bool updateTime(ln_date *utc, double utc_offset);

    bool SetParkPosition(double Axis1Value, double Axis2Value) override;
    bool Goto(double,double);
    bool Park();
    bool UnPark();
    bool Sync(double ra, double dec);

    // Parking
    virtual bool SetCurrentPark();
    virtual bool SetDefaultPark();

    // Guiding
    virtual IPState GuideNorth(float ms);
    virtual IPState GuideSouth(float ms);
    virtual IPState GuideEast(float ms);
    virtual IPState GuideWest(float ms);

    private:

    void mountSim();
    bool getMountRADE();
    bool isSlewComplete();

    bool sendTheSkyOKCommand(const char *command, const char *errorMessage);
    bool isTheSkyParked();
    bool isTheSkyTracking();
    bool startOpenLoopMotion(uint8_t motion, uint16_t rate);
    bool stopOpenLoopMotion();
    bool setTheSkyTracking(bool enable, bool isSidereal, double raRate, double deRate);

    double currentRA;
    double currentDEC;
    double targetRA;
    double targetDEC;

    ln_lnlat_posn lnobserver;
    ln_hrz_posn lnaltaz;
    unsigned int DBG_SCOPE;

    // Jog Rate
    INumber JogRateN[2];
    INumberVectorProperty JogRateNP;

    // Guide Rate
    INumber GuideRateN[2];
    INumberVectorProperty GuideRateNP;

    // Tracking Mode
    ISwitch TrackModeS[4];
    ISwitchVectorProperty TrackModeSP;

    // Tracking Rate
    INumber TrackRateN[2];
    INumberVectorProperty TrackRateNP;
};

#endif // PARAMOUNT_H
