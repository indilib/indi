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
#include "indifilterinterface.h"
#include "indipropertyswitch.h"
#include "fitskeyword.h"

/**
 * @brief The GuideSim class provides an advanced simulator for a CCD that includes a dedicated on-board guide chip.
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
class GuideSim : public INDI::CCD
{
    public:

        GuideSim();
        virtual ~GuideSim() override = default;

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

        int DrawImageStar(INDI::CCDChip *targetChip, float, float, float, float ExposureTime,
                          double zeroPointK, double zeroPointZ);
        int AddToPixel(INDI::CCDChip *targetChip, int, int, int);

        virtual IPState GuideNorth(uint32_t) override;
        virtual IPState GuideSouth(uint32_t) override;
        virtual IPState GuideEast(uint32_t) override;
        virtual IPState GuideWest(uint32_t) override;

        virtual bool saveConfigItems(FILE *fp) override;
        virtual void addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;
        virtual void activeDevicesUpdated() override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int hor, int ver) override;

        virtual bool StartStreaming() override;
        virtual bool StopStreaming() override;

    private:

        float CalcTimeLeft(timeval, float);
        bool SetupParms();

        // Turns on/off Bayer RGB simulation.
        void setRGB(bool onOff);

        double m_TemperatureRequest { 0 };

        float m_ExposureRequest { 0 };
        struct timeval m_ExpStart
        {
            0, 0
        };


        int m_TestValue { 0 };
        bool m_ShowStarField { true };
        int m_Bias { 1500 };
        int m_MaxNoise { 20 };
        int m_MaxVal { 65000 };
        int m_MaxPix { 0 };
        int m_MinPix { 65000 };
        float m_SkyGlow { 40 };
        float m_LimitingMag { 11.5 };
        float m_SaturationMag { 2 };
        float m_Seeing { 3.5 };
        float m_ImageScaleX { 1.0 };
        float m_ImageScaleY { 1.0 };
        //  An oag is offset this much from center of scope position (arcminutes)
        float m_OAGoffset { 0 };
        double m_RotationCW { 0 };
        float m_TimeFactor { 1 };
        double m_RotationOffset { 0 };

        bool m_SimulateRGB { false };

        bool m_AbortPrimaryFrame { false };

        /// Guide rate is 7 arcseconds per second
        float m_GuideRate { 7 };

        float m_PEPeriod { 8 * 60 };
        float m_PEMax { 11 };

        // Random values added to ra and dec
        float m_RaRand { 0 };
        float m_DecRand { 0 };

        // linear drift (multiplied by seconds since start) in arcsec/sec.
        float m_RaTimeDrift { 0 };
        float m_DecTimeDrift { 0 };

        double m_CurrentRA { 0 };
        double m_CurrentDEC { 0 };
        bool m_UsePE { false };
        time_t m_RunStart;
        time_t m_LastSim;
        bool m_RunStartInitialized { false };

        float m_GuideNSOffset {0};
        float m_GuideWEOffset {0};

        float m_PolarError { 0 };
        float m_PolarDrift { 0 };
        float m_KingGamma = { 0 };
        float m_KingTheta = { 0 };

        int m_StreamPredicate;
        pthread_t m_PrimaryThread;
        bool m_TerminateThread;

        //  And this lives in our simulator settings page

        enum
        {
            SIM_XRES = 0,
            SIM_YRES,
            SIM_XSIZE,
            SIM_YSIZE,
            SIM_MAXVAL,
            SIM_BIAS,
            SIM_SATURATION,
            SIM_LIMITINGMAG,
            SIM_NOISE,
            SIM_SKYGLOW,
            SIM_OAGOFFSET,
            SIM_POLAR,
            SIM_POLARDRIFT,
            SIM_ROTATION,
            SIM_KING_GAMMA,
            SIM_KING_THETA,
            SIM_TIME_FACTOR,
            SIM_SEEING,
            SIM_RA_DRIFT,
            SIM_DEC_DRIFT,
            SIM_RA_RAND,
            SIM_DEC_RAND,
            SIM_PE_PERIOD,
            SIM_PE_MAX,
            SIM_TEMPERATURE,
            SIM_NUM_PROPERTIES
        };
        INDI::PropertyNumber SimulatorSettingsNP {SIM_NUM_PROPERTIES};


        INDI::PropertySwitch SimulateRgbSP {2};
        enum
        {
            SIMULATE_YES,
            SIMULATE_NO
        };

        INDI::PropertyNumber EqPENP {2};
        enum
        {
            RA_PE,
            DEC_PE
        };
        
        INDI::PropertyNumber GainNP {1};

        INDI::PropertySwitch ToggleTimeoutSP {2};

        static constexpr const char *SIMULATOR_TAB {"Simulator Settings"};
};
