/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indiccd.h"

/**
 * @brief The GuideSim class provides a simple Guide CCD simulator driver.
 *
 * It can stream video and generate images based on General-Star-Catalog tool (gsc). It simulates guiding pulses.
 */
class GuideSim : public INDI::CCD
{
  public:

    GuideSim();
    virtual ~GuideSim() = default;

    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;

    void ISGetProperties(const char *dev) override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

    static void *streamVideoHelper(void *context);
    void *streamVideo();

protected:

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;    

    bool AbortExposure() override;

    void TimerHit() override;

    int DrawCcdFrame(INDI::CCDChip *targetChip);

    int DrawImageStar(INDI::CCDChip *targetChip, float, float, float, float ExposureTime);
    int AddToPixel(INDI::CCDChip *targetChip, int, int, int);

    IPState GuideNorth(uint32_t) override;
    IPState GuideSouth(uint32_t) override;
    IPState GuideEast(uint32_t) override;
    IPState GuideWest(uint32_t) override;

    virtual bool saveConfigItems(FILE *fp) override;
    virtual void activeDevicesUpdated() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;

    virtual bool StartStreaming() override;
    virtual bool StopStreaming() override;

  private:

    float CalcTimeLeft(timeval, float);
    bool SetupParms();

    float ExposureRequest { 0 };
    struct timeval ExpStart { 0, 0 };

    int testvalue { 0 };
    bool ShowStarField { true };
    int bias { 1500 };
    int maxnoise { 20 };
    int maxval { 65000 };
    int maxpix { 0 };
    int minpix { 65000 };
    float skyglow { 40 };
    float limitingmag { 11.5 };
    float saturationmag { 2 };
    float seeing { 3.5 };
    float ImageScalex { 1.0 };
    float ImageScaley { 1.0 };
    //  An oag is offset this much from center of scope position (arcminutes)
    float OAGoffset { 0 };
    float rotationCW { 0 };
    float TimeFactor { 1 };
    //  our zero point calcs used for drawing stars
    float k { 0 };
    float z { 0 };

    float guideNSOffset {0};
    float guideWEOffset {0};

    bool AbortPrimaryFrame { false };

    /// Guide rate is 7 arcseconds per second
    float GuideRate { 7 };

    /// Our PEPeriod is 8 minutes and we have a 22 arcsecond swing
    float PEPeriod { 8*60 };
    float PEMax { 11 };

    double currentRA { 0 };
    double currentDE { 0 };
    bool usePE { false };
    time_t RunStart;

    float polarError { 0 };
    float polarDrift { 0 };

    int streamPredicate;
    pthread_t primary_thread;
    bool terminateThread;

    //  And this lives in our simulator settings page

    INumberVectorProperty *SimulatorSettingsNV;
    INumber SimulatorSettingsN[14];

    ISwitch TimeFactorS[3];
    ISwitchVectorProperty *TimeFactorSV;

    //  We are going to snoop these from focuser
    INumberVectorProperty FWHMNP;
    INumber FWHMN[1];

    INumberVectorProperty EqPENP;
    INumber EqPEN[2];

};
