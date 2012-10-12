#ifndef GENERIC_CCD_H
#define GENERIC_CCD_H

#if 0
    Generic CCD
    CCD Template for INDI Developers
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#include <indiccd.h>
#include <iostream>

using namespace std;


class GenericCCD : public INDI::CCD
{
public:

    GenericCCD();
    virtual ~GenericCCD();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    int StartExposure(float duration);
    bool AbortExposure();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    void TimerHit();
    virtual bool updateCCDFrame(int x, int y, int w, int h);
    virtual bool updateCCDBin(int binx, int biny);
    virtual void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);
    virtual bool updateCCDFrameType(CCDChip::CCD_FRAME fType);

    // Guide Port
    virtual bool GuideNorth(float);
    virtual bool GuideSouth(float);
    virtual bool GuideEast(float);
    virtual bool GuideWest(float);

    private:

    ISwitch ResetS[1];
    ISwitchVectorProperty ResetSP;

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    double ccdTemp;
    double minDuration;
    unsigned short *imageBuffer;

    int timerID;

    CCDChip::CCD_FRAME imageFrameType;

    struct timeval ExpStart;

    float ExposureRequest;
    float TemperatureRequest;

    float CalcTimeLeft();
    int grabImage();
    bool setupParams();
    void resetFrame();

    bool sim;

};

#endif // GENERIC_CCD_H
