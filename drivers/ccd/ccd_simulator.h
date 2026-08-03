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
#include <vector>
#include <map>
#include <string>

#include <indiccd.h>
#include "sky_renderer.h"
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
 * Planet simulation mode uses xplanet to render solar system bodies at the correct angular scale based on
 * focal length, pixel size, and planet angular diameter.
 *
m * @author Gerry Rozema
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

        virtual IPState GuideNorth(uint32_t) override;
        virtual IPState GuideSouth(uint32_t) override;
        virtual IPState GuideEast(uint32_t) override;
        virtual IPState GuideWest(uint32_t) override;

        virtual bool saveConfigItems(FILE *fp) override;
        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword) override;
        virtual void activeDevicesUpdated() override;
        virtual int SetTemperature(double temperature, bool enableCooler = false) override;
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

        bool watchDirectory();
        bool loadNextImage();
        bool setupParameters();
        bool renderPlanet();

        // Turns on/off Bayer RGB simulation.
        void setBayerEnabled(bool onOff);

        float CalcTimeLeft(timeval start, float req);

        SkyRenderer m_Renderer;

        // ADU ceiling and magnitude calibration -- updated from INDI properties each frame.
        int   m_MaxVal        {65000};
        float m_LimitingMag   {11.5f};
        float m_SaturationMag {2.0f};
        float m_Seeing        {3.5f};  // arcsec FWHM, updated by focus simulation

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
        float m_SkyGlow { 40 };
        //  An oag is offset this much from center of scope position (arcminutes)
        float m_OAGOffset { 0 };
        float m_TimeFactor { 1 };
        // With a rotator device "RotatorAngle" is snooped and defined, so the
        // resulting camera rotation is the addition of offset and rotator angle.
        // Without a rotator device ("Manual Rotator") the rotator angle is
        // considered fixed to 0° and the camera rotation is equal to offset
        float m_RotationOffset { 0 };

        bool m_SimulateBayer { false };
        bool m_SimulateTimeout { false };

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
        double raPE  { 0 };   // J2000 RA  from snooped EQUATORIAL_PE (hours)
        double decPE { 0 };   // J2000 Dec from snooped EQUATORIAL_PE (degrees)
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

        INDI::PropertySwitch SimTestsSP {3};
        enum
        {
            SIM_TESTS_BAYER,
            SIM_TESTS_CRASH,
            SIM_TESTS_TIMEOUT
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

        INDI::PropertyNumber TiltSimulationNP {2};
        enum
        {
            SIM_TILT_LR,
            SIM_TILT_TB
        };

        INDI::PropertyNumber EqPENP {2};

        INDI::PropertySwitch CoolerSP {2};

        INDI::PropertyNumber GainNP {1};

        INDI::PropertyNumber OffsetNP {1};

        INDI::PropertyText DirectoryTP {1};
        INDI::PropertySwitch DirectorySP {2};

        INDI::PropertySwitch ResolutionSP {3};
        inline static const std::vector<std::pair<uint32_t, uint32_t>> Resolutions =
        {
            {1280, 1024},
            {6000, 4000},
            {0, 0} // Custom
        };

        // ==================== Planet Simulation ====================

        // Lookup table: average angular diameter at opposition (arcsec)
        // Moon and Sun are mean apparent diameters
        inline static const std::map<std::string, float> PlanetAngularDiameter =
        {
            {"mercury", 11.0f},
            {"venus",   35.0f},
            {"mars",    18.0f},
            {"jupiter", 42.0f},
            {"saturn",  42.0f},   // including rings
            {"uranus",   3.7f},
            {"neptune",  2.3f},
            {"moon",   1860.0f},  // ~0.5°
            {"sun",    1920.0f}   // ~0.53°
        };

        // Planet mode toggle
        INDI::PropertySwitch PlanetModeSP {2};
        enum
        {
            PLANET_ENABLED,
            PLANET_DISABLED
        };

        // Planet selection switch (one per body)
        INDI::PropertySwitch PlanetSelectSP {9};
        enum
        {
            PLANET_MERCURY,
            PLANET_VENUS,
            PLANET_MARS,
            PLANET_JUPITER,
            PLANET_SATURN,
            PLANET_URANUS,
            PLANET_NEPTUNE,
            PLANET_MOON,
            PLANET_SUN
        };

        // Planet refresh interval in minutes, and optional size override in arcsec (0 = use lookup table)
        INDI::PropertyNumber PlanetSettingsNP {2};
        enum
        {
            PLANET_REFRESH,
            PLANET_SIZE
        };

        // xplanet executable path and config file path
        INDI::PropertyText PlanetPathsTP {2};
        enum
        {
            PLANET_XPLANET_PATH,
            PLANET_XPLANET_CONFIG
        };

        // Planet simulation state
        bool m_PlanetMode {false};
        bool m_IsRGBFormat {false};
        std::string m_SelectedPlanet {"jupiter"};
        float m_PlanetRefreshMinutes {60.0f};
        float m_PlanetSizeOverride {0.0f};  // 0 = use lookup table
        std::string m_XPlanetBinary {"xplanet"};
        std::string m_XPlanetConfig {"/usr/share/xplanet/config/default"};
        time_t m_LastPlanetRenderTime {0};
        bool m_LastRenderWasCacheHit {false};
        std::vector<uint8_t> m_CachedPlanetBuffer;
        std::vector<uint8_t> m_CachedStreamBuffer;   // raw xplanet PNG bytes for direct streaming
        uint32_t m_CachedPlanetWidth {0};
        uint32_t m_CachedPlanetHeight {0};
        float m_CachedFocalLength {0};
        float m_CachedPixelSizeX {0};
        uint32_t m_CachedWidth {0};
        std::string m_CachedPlanet;

        static const constexpr char* SIMULATOR_TAB = "Simulator Config";
};