/*
    MaxDome II 
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

    Migrated to INDI::Dome by Jasem Mutlaq (2014)

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

#ifndef MAXDOMEII_H
#define MAXDOMEII_H

#include <indidevapi.h>
#include <indicom.h>
#include <indidome.h>

#define MD_AZIMUTH_IDLE 0
#define MD_AZIMUTH_MOVING 1
#define MD_AZIMUTH_HOMING 2

class MaxDomeII : public INDI::Dome
{
 public:
 MaxDomeII();
 ~MaxDomeII();

 const char *getDefaultName();
 bool initProperties();
 bool updateProperties();

 bool Connect();
 bool Disconnect();

 void TimerHit();

 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

 virtual int MoveAbsDome(double az);
 virtual int ParkDome();
 virtual int HomeDome();
 virtual bool AbortDome();

protected:

 /*******************************************************/
 /* Misc routines
 ********************************************************/
 int AzimuthDistance(int nPos1, int nPos2);
 double TicksToAzimuth(int nTicks);
 int AzimuthToTicks(double nAzimuth);
 int handle_driver_error(int *error, int *nRetry); // Handles errors returned by driver

 ISwitch OnMovementTypeS[2];
 ISwitchVectorProperty OnMovementTypeSP;

 ISwitch ShutterS[3];
 ISwitchVectorProperty ShutterSP;

 ISwitch ParkOnShutterS[2];
 ISwitchVectorProperty ParkOnShutterSP;

 INumber TicksPerTurnN[1];
 INumberVectorProperty TicksPerTurnNP;

 INumber WatchDogN[1];
 INumberVectorProperty WatchDogNP;

 INumber HomePosRN[1];
 INumberVectorProperty HomePosRNP;

private:

 int nTicksPerTurn;             // Number of ticks per turn of azimuth dome
 unsigned nCurrentTicks;        // Position as reported by the MaxDome II
 int nCloseShutterBeforePark;   // 0 no close shutter
 double nParkPosition;			// Park position
 double nHomeAzimuth;			// Azimuth of home position
 int nHomeTicks;				// Ticks from 0 azimuth to home
 int fd;                        // Telescope tty file descriptor
 int nTimeSinceShutterStart;	// Timer since shutter movement has started, in order to check timeouts
 int nTimeSinceAzimuthStart;	// Timer since azimuth movement has started, in order to check timeouts
 int nTargetAzimuth;
 int nTimeSinceLastCommunication;  // Used by Watch Dog

 double prev_az, prev_alt;

 bool SetupParms();


};

#endif
