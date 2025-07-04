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

#include <deque>

#include "indiccd.h"
#include "indifilterinterface.h"

/**
 * @brief The CCDSim class provides an advanced simulator for a CCD that includes a dedicated on-board guide chip.
 *
 * The CCD driver can generate star fields given that General-Star-Catalog (gsc) tool is installed on the same machine the driver is running.
 *
 * Many simulator parameters can be configured to generate the final star field image. In addition to support guider chip and guiding pulses (ST4),
 * a filter wheel support is provided for 8 filter wheels. Cooler and temperature control is also supported.
 *
 * The driver can snoop the mount equatorial coords to draw the star field. It listens to EQUATORIAL_PE property and also defines it so that the user
 * can set it manually.
 *
 * Video streaming can be enabled from the Stream property group with several encoders and recorders supported.
 *
 * @author Gerry Rozema
 * @author Jasem Mutlaq
 */
class CCDSim : public INDI::CCD, public INDI::FilterInterface
{
public:

    enum
    {
        SIM_XRES,
        SIM_YRES,
        SIM_XSIZE,
        SIM_YSIZE,
        SIM_MAXVAL,
        SIM_SATURATION,
        SIM_LIMITINGMAG,
        SIM_NOISE,
        SIM_SKYGLOW,
        SIM_OAGOFFSET,
        SIM_POLAR,
        SIM_POLARDRIFT,
        SIM_PE_PERIOD,
        SIM_PE_MAX,
        SIM_TIME_FACTOR,
        SIM_ROTATION,
        SIM_N
    };

    CCDSim();
    virtual ~CCDSim() override = default;

    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;

    void ISGetProperties(const char *dev) override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

    static void *streamVideoHelper(void *context);
    void *streamVideo();

protected:

    bool Connect() override;
    bool Disconnect() override;

    bool StartExposure(float duration) override;
    bool StartGuideExposure(float) override;

    bool AbortExposure() override;
    bool AbortGuideExposure() override;

    void TimerHit() override;

    int DrawCcdFrame(INDI::CCDChip *targetChip);

    int DrawImageStar(INDI::CCDChip *targetChip, float, float, float, float ExposureTime);
    int AddToPixel(INDI::CCDChip *targetChip, int, int, int);

    virtual IPState GuideNorth(uint32_t) override;
    virtual IPState GuideSouth(uint32_t) override;
    virtual IPState GuideEast(uint32_t) override;
    virtual IPState GuideWest(uint32_t) override;

    virtual bool saveConfigItems(FILE *fp) override;
    virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword) override;
    virtual void activeDevicesUpdated() override;
    virtual int SetTemperature(double temperature) override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;
    virtual bool UpdateGuiderBin(int hor, int ver) override;

    virtual bool SetCaptureFormat(uint8_t index) override;

    virtual bool StartStreaming() override;
    virtual bool StopStreaming() override;

    // Filter
    bool SelectFilter(int) override;
    int QueryFilter() override;

protected:

    float CalcTimeLeft(timeval, float);
    bool watchDirectory();
    bool loadNextImage();
    bool setupParameters();

    // Turns on/off Bayer RGB simulation.
    void setBayerEnabled(bool onOff);

    double flux(double magnitude) const;

    double TemperatureRequest { 0 };

    float ExposureRequest { 0 };
    struct timeval ExpStart
    {
        0, 0
    };

    float GuideExposureRequest { 0 };
    struct timeval GuideExpStart
    {
        0, 0
    };

    int testvalue { 0 };
    bool ShowStarField { true };
    int m_Bias { 1500 };
    int m_MaxNoise { 20 };
    int m_MaxVal { 65000 };
    int maxpix { 0 };
    int minpix { 65000 };
    float m_SkyGlow { 40 };
    float m_LimitingMag { 11.5 };
    float m_SaturationMag { 2 };
    float seeing { 3.5 };
    float ImageScalex { 1.0 };
    float ImageScaley { 1.0 };
    //  An oag is offset this much from center of scope position (arcminutes)
    float m_OAGOffset { 0 };
    float m_RotationCW { 0 };
    float m_TimeFactor { 1 };
    double m_CameraRotation { 0 };

    bool m_SimulateBayer { false };

    //  our zero point calcs used for drawing stars
    //float k { 0 };
    //float z { 0 };

    bool AbortGuideFrame { false };
    bool AbortPrimaryFrame { false };

    /// Guide rate is 7 arcseconds per second
    float GuideRate { 7 };

    /// PEPeriod in minutes
    float m_PEPeriod { 0 };
    // PE max in arcsecs
    float m_PEMax { 0 };

    double currentRA { 0 };
    double currentDE { 0 };
    bool usePE { false };
    time_t RunStart;

    float guideNSOffset {0};
    float guideWEOffset {0};

    float m_PolarError { 0 };
    float m_PolarDrift { 0 };

    double m_LastTemperature {0};

    int streamPredicate {0};
    pthread_t primary_thread;
    bool terminateThread;

    std::deque<std::string> m_AllFiles, m_RemainingFiles;

    //  And this lives in our simulator settings page
    INDI::PropertyNumber SimulatorSettingsNP {16};

    INDI::PropertySwitch SimulateBayerSP {2};
    enum
    {
        INDI_ENABLED,
        INDI_DISABLED
    };
    //  We are going to snoop these from focuser
    INumberVectorProperty FWHMNP;
    INumber FWHMN[1];

    // Focuser positions for focusing simulation
    // FocuserPosition[0] is the position where the scope is in focus
    // FocuserPosition[1] is the maximal position the focuser may move to (@see FOCUS_MAX in #indifocuserinterface.cpp)
    // FocuserPosition[2] is the seeing (in arcsec)
    // We need to have these values here, since we cannot snoop it from the focuser (the focuser does not
    // publish these values)
    INDI::PropertyNumber FocusSimulationNP {3};
    enum
    {
        SIM_FOCUS_POSITION,
        SIM_FOCUS_MAX,
        SIM_SEEING
    };

    INDI::PropertyNumber EqPENP {2};

    INDI::PropertySwitch CoolerSP {2};

    INDI::PropertyNumber GainNP {1};

    INDI::PropertyNumber OffsetNP {1};

    INDI::PropertyText DirectoryTP {1};
    INDI::PropertySwitch DirectorySP {2};

    INDI::PropertySwitch CrashSP {1};

    INDI::PropertySwitch ResolutionSP {3};
    inline static const std::vector<std::pair<uint32_t, uint32_t>> Resolutions =
        {
            {1280, 1024},
            {6000, 4000},
            {0, 0} // Custom
        };

    static const constexpr char* SIMULATOR_TAB = "Simulator Config";
};
