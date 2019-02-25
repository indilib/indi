/*
 Fishcamp INDI CCD Driver
 Copyright (C) 2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#ifndef FISHCAMP_CCD_H
#define FISHCAMP_CCD_H

#include <indiccd.h>
#include <iostream>

#include <fishcamp.h>

using namespace std;

class FishCampCCD : public INDI::CCD
{
  public:
    FishCampCCD(int CamNum);
    virtual ~FishCampCCD();

    const char *getDefaultName() override;

    bool initProperties() override;
    void ISGetProperties(const char *dev) override;
    bool updateProperties() override;

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool AbortExposure() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

  protected:
    void TimerHit() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int binx, int biny) override;
    virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;
    virtual int SetTemperature(double temperature) override;

    virtual void simulationTriggered(bool enable) override;

    // Guide Port
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

  private:
    char name[MAXINDINAME];

    INumber GainN[1];
    INumberVectorProperty GainNP;

    INumber CoolerN[1];
    INumberVectorProperty CoolerNP;

    IText CamInfoT[6] {};
    ITextVectorProperty CamInfoTP;

    int cameraNum;
    double ccdTemp;
    double minDuration;
    unsigned short *imageBuffer;
    fc_camInfo camInfo;
    int timerID;

    INDI::CCDChip::CCD_FRAME imageFrameType;

    struct timeval ExpStart;

    float ExposureRequest;
    float TemperatureRequest;

    float CalcTimeLeft();
    int grabImage();
    bool setupParams();
    bool setGain(double gain);

    bool sim;

    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                            char *formats[], char *names[], int n);
};

#endif // FISHCAMP_CCD_H
