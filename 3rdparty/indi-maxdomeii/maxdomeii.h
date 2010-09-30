/*
    MaxDome II 
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

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

#include "indidevapi.h"
#include "indicom.h"

#define MD_AZIMUTH_IDLE 0
#define MD_AZIMUTH_MOVING 1
#define MD_AZIMUTH_HOMING 2

class MaxDomeII
{
 public:
 MaxDomeII();
 ~MaxDomeII();

 virtual void ISGetProperties (const char *dev);
 virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual void ISPoll ();

 //virtual void connection_lost();
 //virtual void connection_resumed();

private:

  /* Switches */
  ISwitch ConnectS[2];
  ISwitch OnMovementTypeS[2];
  ISwitch AbortS[1];
  ISwitch ShutterS[3];
  ISwitch ParkOnShutterS[2];
  ISwitch ParkS[1];
  ISwitch HomeS[1];
  
  /* Texts */
  IText PortT[1];
  
  /* Numbers */
  INumber AzimuthWN[1];
  INumber AzimuthRN[1];
  INumber TicksPerTurnN[1];
  INumber ParkPositionN[1];
  INumber HomeAzimuthN[1];
    
  /* Switch Vectors */
  ISwitchVectorProperty ConnectSP;
  ISwitchVectorProperty OnMovementTypeSP;
  ISwitchVectorProperty AbortSP;
  ISwitchVectorProperty ShutterSP;
  ISwitchVectorProperty ParkOnShutterSP;
  ISwitchVectorProperty ParkSP;
  ISwitchVectorProperty HomeSP;
  
   /* Number Vectors */
  INumberVectorProperty AzimuthWNP;
  INumberVectorProperty AzimuthRNP;
  INumberVectorProperty TicksPerTurnNP;
  INumberVectorProperty ParkPositionNP;
  INumberVectorProperty HomeAzimuthNP;
  
  /* Text Vectors */
  ITextVectorProperty PortTP;
  
 /*******************************************************/
 /* Connection Routines
 ********************************************************/
 void init_properties();
 void get_initial_data();
 void connect_dome();
 bool is_connected(void);
 
 /*******************************************************/
 /* Misc routines
 ********************************************************/
 int get_switch_index(ISwitchVectorProperty *sp);
 int AzimuthDistance(int nPos1, int nPos2);
 int GotoAzimuth(double newAZ);

 /*******************************************************/
 /* Error handling routines
 ********************************************************/
 void slew_error(int slewCode);
 void reset_all_properties();
 void handle_error(INumberVectorProperty *nvp, int err, const char *msg);
 void correct_fault();

 protected:

  int nTicksPerTurn;	// Number of ticks per turn of azimuth dome
  unsigned nCurrentTicks;	// Position as reported by the MaxDome II
  int nCloseShutterBeforePark;  // 0 no close shutter
  double nParkPosition;			// Park position
  double nHomeAzimuth;			// Azimuth of home position
  int nHomeTicks;				// Ticks from 0 azimuth to home
  int fd;				/* Telescope tty file descriptor */
  int nTimeSinceShutterStart;	// Timer since shutter movement has started, in order to check timeouts
  int nTimeSinceAzimuthStart;	// Timer since azimuth movement has started, in order to check timeouts
  int nTargetAzimuth;
};

#endif
