/*
 ASI ST4 Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "USB2ST4_Conv.h"

#include <defaultdevice.h>
#include <indiguiderinterface.h>

class ASIST4 : public INDI::DefaultDevice, public INDI::GuiderInterface
{
  public:
    explicit ASIST4(int ID);
    ~ASIST4() override = default;

    virtual const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;

  protected:    
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    // Guide Port
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

  private:        

    //##########################################################################
    //######################### North/South Controls ###########################
    static void TimerHelperNS(void *context);
    void TimerNS();
    void stopTimerNS();
    IPState guidePulseNS(uint32_t ms, USB2ST4_DIRECTION dir, const char *dirName);

    double NSPulseRequest {0};
    struct timeval NSPulseStart;
    int NStimerID {0};
    USB2ST4_DIRECTION NSDir;
    const char *NSDirName;
    //##########################################################################

    //##########################################################################
    //########################### West/East Controls ###########################
    static void TimerHelperWE(void *context);
    void TimerWE();
    void stopTimerWE();
    IPState guidePulseWE(uint32_t ms, USB2ST4_DIRECTION dir, const char *dirName);

    double WEPulseRequest;
    struct timeval WEPulseStart;
    int WEtimerID;
    USB2ST4_DIRECTION WEDir;
    const char *WEDirName;
    //##########################################################################

    /** Calculate time left in seconds after start_time */
    double calcTimeLeft(double duration, timeval *start_time);

    char name[MAXINDIDEVICE];
    int ID {0};

    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);
};
