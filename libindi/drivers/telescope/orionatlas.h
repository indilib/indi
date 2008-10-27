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

#ifndef ORIONATLAS_H
#define ORIONATLAS_H

#include <sys/types.h>

#include "indidevapi.h"
#include "indicom.h"

#define RADEC 1
#define AZALT 2

class OrionAtlas
{
 typedef fd_set telfds;

 public:
 OrionAtlas();
 virtual ~OrionAtlas() {}

 virtual void ISGetProperties (const char *dev);
 virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual void ISPoll ();
 virtual void getBasicData();

 int checkPower(INumberVectorProperty *np);
 int checkPower(ISwitchVectorProperty *sp);
 int checkPower(ITextVectorProperty *tp);
 void ConnectTel();
 void log(const char *fmt,...);
// void slewError(int slewCode);
// int handleCoordSet();
// int getOnSwitch(ISwitchVectorProperty *sp);

 private:
  int timeFormat;

  int CheckConnectTel(void);
  int ConnectTel(char *port);
  void DisconnectTel(void);
  int GetCoords(int System=RADEC|AZALT); // Retrieve coordinates from scope
  void UpdateCoords(int System=RADEC|AZALT); // Retrieve coordinates and update INumbers
  int MoveScope(int System, double Coord1, double Coord2); // move the scope

  int TelPortFD;
  int TelConnectFlag;

  double returnRA;    /* Last update of RA */
  double returnDec;   /* Last update of Dec */
  double returnAz;
  double returnAlt;
  //double lat,lon;

  int readn(int fd, unsigned char *ptr, int nbytes, int sec);
  int writen(int fd, unsigned char *ptr, int nbytes);
  int telstat(int fd,int sec,int usec);

  bool Updating;
};

#endif

