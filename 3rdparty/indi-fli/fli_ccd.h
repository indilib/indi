/*
    FLI CCD
    INDI Interface for Finger Lakes Instrument CCDs
    Copyright (C) 2003-2016 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    2016.05.16: Added CCD Cooler Power (JM)

*/

#pragma once

#include <libfli.h>
#include <indiccd.h>
#include <iostream>

using namespace std;

class FLICCD : public INDI::CCD
{
  public:
    FLICCD();
    ~FLICCD() = default;

    const char *getDefaultName() override;

    bool initProperties() override;
    void ISGetProperties(const char *dev) override;
    bool updateProperties() override;

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool AbortExposure() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    virtual void TimerHit() override;
    virtual int SetTemperature(double temperature) override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

    virtual void debugTriggered(bool enable) override;
    virtual bool saveConfigItems(FILE *fp) override;

  private:
    // Find FLI CCD
    bool findFLICCD(flidomain_t domain);

    // Calculate Time until exposure is complete
    float calcTimeLeft();

    // Fetch image row by row from the CCD
    bool grabImage();

    // Get initial CCD values upon connection
    bool setupParams();

    typedef struct
    {
        flidomain_t domain;
        char *dname;
        char *name;
        char model[32]={0};
        long HWRevision;
        long FWRevision;
        double x_pixel_size;
        double y_pixel_size;
        long Array_Area[4];
        long Visible_Area[4];
        int width, height;
        double temperature;
    } cam_t;

    ISwitch PortS[4];
    ISwitchVectorProperty PortSP;

    IText CamInfoT[3];
    ITextVectorProperty CamInfoTP;

    INumber CoolerN[1];
    INumberVectorProperty CoolerNP;

    INumber FlushN[1];
    INumberVectorProperty FlushNP;

    ISwitch BackgroundFlushS[2];
    ISwitchVectorProperty BackgroundFlushSP;

    int timerID=0;

    // Exposure timing
    struct timeval ExpStart;
    float ExposureRequest;

    flidev_t fli_dev;
    cam_t FLICam;

    // Simulation mode
    bool sim=false;
};
