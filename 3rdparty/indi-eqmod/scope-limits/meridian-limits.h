/* Copyright 2018 starrybird (star48b@gmail.com) */
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

#pragma once

#include "../skywatcher.h"

#include <inditelescope.h>
#include <vector>


class MeridianLimits
{
  protected:
  private:
    INDI::Telescope *telescope;
    ITextVectorProperty *MeridianLimitsDataFileTP;
    IBLOBVectorProperty *MeridianLimitsDataFitsBP;
    INumberVectorProperty *MeridianLimitsStepNP;
    ISwitchVectorProperty *MeridianLimitsSetCurrentSP;
    ISwitchVectorProperty *MeridianLimitsFileOperationSP;
    ISwitchVectorProperty *MeridianLimitsOnLimitSP;

    unsigned long raMotorEncoderEast;
    unsigned long raMotorEncoderWest;

    char *WriteDataFile(const char *filename);
    char *LoadDataFile(const char *filename);
    char errorline[128];
    char *sline;
    bool MeridianInitialized;

    Skywatcher *mount;
    
  public:
    MeridianLimits(INDI::Telescope *, Skywatcher *);
    virtual ~MeridianLimits();

    const char *getDeviceName(); // used for logger

    virtual bool initProperties();
    virtual void ISGetProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n);

    virtual void Init();
    virtual void Reset();

    virtual bool inLimits(unsigned long ra_motor_step);
    virtual bool checkLimits(unsigned long ra_motor_step, INDI::Telescope::TelescopeStatus status);
};
