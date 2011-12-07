#ifndef QSI_CCD_H
#define QSI_CCD_H

#if 0
    QSI CCD
    INDI Interface for Quantum Scientific Imaging CCDs
    Based on FLI Indi Driver by Jasem Mutlaq.
    Copyright (C) 2009 Sami Lehti (sami.lehti@helsinki.fi)

    (2011-12-10) Updated by Jasem Mutlaq:
        + Fixed compiler warnings.
        + Reduced message traffic.
        + Added filter name property.
        + Class based on INDI::CCD

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



class QSICCD : public INDI::CCD
{
        protected:
        private:

    QSICamera QSICam;


    ISwitch ResetS[1];
    ISwitchVectorProperty *ResetSP;

    INumber CoolerN[1];
    INumberVectorProperty *CoolerNP;

    ISwitch CoolerS[2];
    ISwitchVectorProperty *CoolerSP;

    ISwitch ShutterS[2];
    ISwitchVectorProperty *ShutterSP;

    INumber TemperatureRequestN[1];
    INumberVectorProperty *TemperatureRequestNP;

    INumber TemperatureN[1];
    INumberVectorProperty *TemperatureNP;

    bool canAbort;
    short targetFilter;
    double ccdTemp;

    unsigned short *imageBuffer;
    double imageExpose;
    int imageWidth, imageHeight;
    INDI::CCD::CCD_FRAME imageFrameType;


     struct timeval ExpStart;
     float CalcTimeLeft(timeval,float);
     int grabImage();
     bool setupParams();
     int manageDefaults(char errmsg[]);

     void activateCooler();
     void resetFrame();
     void shutterControl();
     double min();
     double max();
     void fits_update_key_s(fitsfile* fptr, int type, string name, void* p, string explanation, int* status);

public:

    QSICCD();
    virtual ~QSICCD();

    const char *getDefaultName();

    bool initProperties();
    bool updateProperties();

    bool Connect(char *msg);
    bool Disconnect();

    int StartExposure(float duration);
    bool AbortExposure();

    void TimerHit();


    virtual void updateCCDFrame();
    virtual void updateCCDBin();
    virtual void addFITSKeywords(fitsfile *fptr);

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);


};

#endif // QSI_CCD_H
