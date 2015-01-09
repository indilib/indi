#ifndef QSI_CCD_H
#define QSI_CCD_H

#if 0
    QSI CCD
    INDI Interface for Quantum Scientific Imaging CCDs
    Based on FLI Indi Driver by Jasem Mutlaq.
    Copyright (C) 2009 Sami Lehti (sami.lehti@helsinki.fi)

    (2011-12-10) Updated by Jasem Mutlaq:
        + Driver completely rewritten to be based on INDI::CCD
        + Fixed compiler warnings.
        + Reduced message traffic.
        + Added filter name property.
        + Added snooping on telescopes.
        + Added Guider support.
        + Added Readout speed option.

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
#include <indiguiderinterface.h>
#include <indifilterinterface.h>
#include <iostream>

using namespace std;

#define MAX_FILTERS_SIZE 6


class QSICCD : public INDI::CCD, public INDI::FilterInterface
{
        protected:
        private:

    QSICamera QSICam;

    INumber CoolerN[1];
    INumberVectorProperty CoolerNP;

    ISwitch CoolerS[2];
    ISwitchVectorProperty CoolerSP;

    ISwitch ShutterS[2];
    ISwitchVectorProperty ShutterSP;

    ISwitch FilterS[2];
    ISwitchVectorProperty FilterSP;

    ISwitch ReadOutS[2];
    ISwitchVectorProperty ReadOutSP;

    ISwitch GainS[3];
    ISwitchVectorProperty GainSP;

    ISwitch FanS[3];
    ISwitchVectorProperty FanSP;

    bool canAbort, canSetGain, canControlFan, canChangeReadoutSpeed;
    short targetFilter;
    double ccdTemp, targetTemperature;
    double minDuration;
    unsigned short *imageBuffer;
    double ExposureRequest;
    int imageWidth, imageHeight;
    int timerID;
    CCDChip::CCD_FRAME imageFrameType;
    struct timeval ExpStart;
    std::string filterDesignation[MAX_FILTERS_SIZE];

    float CalcTimeLeft(timeval,float);
    int grabImage();
    bool setupParams();
    bool manageDefaults();
    void activateCooler(bool enable);
    void shutterControl();
    void turnWheel();

    virtual bool GetFilterNames(const char* groupName);
    virtual bool SetFilterNames();
    virtual bool SelectFilter(int);
    virtual int QueryFilter();

public:

    QSICCD();
    virtual ~QSICCD();

    const char *getDefaultName();

    bool initProperties();
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    int  SetTemperature(double temperature);
    bool StartExposure(float duration);
    bool AbortExposure();

    void TimerHit();

    virtual bool UpdateCCDFrame(int x, int y, int w, int h);
    virtual bool UpdateCCDBin(int binx, int biny);
    virtual void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

    virtual bool GuideNorth(float);
    virtual bool GuideSouth(float);
    virtual bool GuideEast(float);
    virtual bool GuideWest(float);

};

#endif // QSI_CCD_H
