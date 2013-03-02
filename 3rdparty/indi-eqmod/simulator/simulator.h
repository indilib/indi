/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EQMOD_SIMULATOR_H
#define EQMOD_SIMULATOR_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <indicom.h>
#include <inditelescope.h>
#include <sys/time.h>

#include "config.h"
#include "skywatcher-simulator.h"

class EQModSimulator 
{
 protected:
 private:
  INDI::Telescope *telescope;
  SkywatcherSimulator *sksim;

  INumberVectorProperty *SimWormNP;
  INumberVectorProperty *SimRatioNP;
  INumberVectorProperty *SimMotorNP;
  ISwitchVectorProperty *SimModeSP;
  ISwitchVectorProperty *SimHighSpeedSP;
  ITextVectorProperty *SimMCVersionTP;

 public:
  EQModSimulator(INDI::Telescope *);
  void Connect();
  void receive_cmd(const char *cmd, int *received);
  void send_reply(char *buf, int *sent);
  bool updateProperties(bool enable);
  bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) ;
};

#endif
