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


/************************************************************************************
This simulator is useful for simulating the images sent to a guiding program such as the
Ekos guider or PHD2.

There are many adjustments to the image needing comments.  An important one is:

  Seeing: make this reasonably large so that single pixels stars aren't generated and the
          guider can track sub-pixel. 5 is a good start.

Use the below INDI properties to simulate imperfections in real-world mounts.

  RA drift: Simulates a drift in arcseconds/second of the RA angle, e.g. due to bad tracking
            or refraction
  DEC drift: Similar, useful to simulate polar alignment error, etc.

  Periodic error period (secs):
  Periodic error maxval (arcsecs): These add a sinusoid of the given period, going from
            -maxval to maxval arcseconds onto the RA.

  Max random RA add (arcsecs):
  Max random DEC add (arcsecs): These add random RA or DEC offsets each frame to the RA or
           dec values. The random values are linearly distributed between -value to + value.

Another interesting guide hardware simulation is found in the telescope simulator,
which can simulate backlash to the guiding pulses. See its Dec Backlash parameter.
************************************************************************************/

#include "guide_simulator.h"
#include "indicom.h"
#include "stream/streammanager.h"

#include <libnova/julian_day.h>
#include <libastro.h>

#include <cmath>
#include <unistd.h>

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

constexpr double DEGREES_TO_RADIANS = 0.0174532925;

// We declare an auto pointer to GuideSim.
static std::unique_ptr<GuideSim> ccd(new GuideSim());

GuideSim::GuideSim()
{
    m_CurrentRA  = RA;
    m_CurrentDEC = Dec;

    m_StreamPredicate = 0;
    m_TerminateThread = false;
}

bool GuideSim::SetupParms()
{
    int nbuf;
    SetCCDParams(SimulatorSettingsNP[SIM_XRES].getValue(), SimulatorSettingsNP[SIM_YRES].getValue(), 16,
                 SimulatorSettingsNP[SIM_XSIZE].getValue(),
                 SimulatorSettingsNP[SIM_YSIZE].getValue());

    //  Random number added to each pixel up to this value.
    m_MaxNoise      = SimulatorSettingsNP[SIM_NOISE].getValue();
    // A "glow" added to all frames, stronger at the center and less so further from the center.
    m_SkyGlow       = SimulatorSettingsNP[SIM_SKYGLOW].getValue();
    // Clipping ADU value. Nothing is allowed to get brighter.
    m_MaxVal        = SimulatorSettingsNP[SIM_MAXVAL].getValue();
    //  Fixed bias added to each pixel. Useful when negative and half of m_MaxNoise.
    // This only gets added if m_MaxNoise is > 0.
    m_Bias          = SimulatorSettingsNP[SIM_BIAS].getValue();
    //  A saturation mag star saturates in one second
    //  and a limiting mag produces a one adu level in one second
    m_LimitingMag   = SimulatorSettingsNP[SIM_LIMITINGMAG].getValue();
    m_SaturationMag = SimulatorSettingsNP[SIM_SATURATION].getValue();
    // offset the dec (in minutes) by the guide head offset
    m_OAGoffset = SimulatorSettingsNP[SIM_OAGOFFSET].getValue();
    // Doesn't make much sense to me.
    // The dec is offset by (m_PolarError * polarDrift * cos(dec)) / 3.81
    // This (locally at least) is a constant offset to dec, so won't show
    // up much in guiding error.
    m_PolarError = SimulatorSettingsNP[SIM_POLAR].getValue();
    m_PolarDrift = SimulatorSettingsNP[SIM_POLARDRIFT].getValue();
    //  Kwiq++
    m_KingGamma = SimulatorSettingsNP[SIM_KING_GAMMA].getValue() * DEGREES_TO_RADIANS;
    m_KingTheta = SimulatorSettingsNP[SIM_KING_THETA].getValue() * DEGREES_TO_RADIANS;
    // Reduce the simulator "wait time" for exposures by this factor.
    // That is, we can say exposure duration = 10s, and m_TimeFactor = 0.05
    // and the system will simulate a 10s exposure, but it will only take 0.5 seconds.
    m_TimeFactor = SimulatorSettingsNP[SIM_TIME_FACTOR].getValue();
    // This is the rotation offset of the simulated camera respective to North.
    // Because the simulated star field is calculated with their RA/DEC-coordinates
    // (see DrawCcdFrame()) the origin angle of star field points north. So this value
    // for EQ mounts normally simulate a certain camera offset and is a constant.
    // For ALTAZ-mount this variable is altered consecutively by the value of the parallactic
    // angle (transfered through a signal from KStars/skymapdrawabstract.cpp) and this way used
    // to simulate the deviation of the camera orientation from N.
    m_RotationOffset = SimulatorSettingsNP[SIM_ROTATION].getValue();
    m_Seeing = SimulatorSettingsNP[SIM_SEEING].getValue();
    m_RaTimeDrift = SimulatorSettingsNP[SIM_RA_DRIFT].getValue();
    m_DecTimeDrift = SimulatorSettingsNP[SIM_DEC_DRIFT].getValue();
    m_RaRand = SimulatorSettingsNP[SIM_RA_RAND].getValue();
    m_DecRand = SimulatorSettingsNP[SIM_DEC_RAND].getValue();
    m_PEPeriod = SimulatorSettingsNP[SIM_PE_PERIOD].getValue();
    m_PEMax = SimulatorSettingsNP[SIM_PE_MAX].getValue();
    m_TemperatureRequest = SimulatorSettingsNP[SIM_TEMPERATURE].getValue();
    TemperatureNP[0].setValue(m_TemperatureRequest);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    //nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    Streamer->setPixelFormat(INDI_MONO, 16);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    return true;
}

bool GuideSim::Connect()
{
    m_StreamPredicate = 0;
    m_TerminateThread = false;
    pthread_create(&m_PrimaryThread, nullptr, &streamVideoHelper, this);
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool GuideSim::Disconnect()
{
    pthread_mutex_lock(&condMutex);
    m_StreamPredicate = 1;
    m_TerminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

const char * GuideSim::getDefaultName()
{
    return "Guide Simulator";
}

bool GuideSim::initProperties()
{
    //  Most hardware layers wont actually have indi properties defined
    //  but the simulators are a special case
    INDI::CCD::initProperties();

    CaptureFormat format = {"INDI_MONO", "Mono", 16, true};
    addCaptureFormat(format);

    SimulatorSettingsNP[SIM_XRES].fill("SIM_XRES", "X resolution", "%4.0f", 0, 8192, 0, 1280);
    SimulatorSettingsNP[SIM_YRES].fill("SIM_YRES", "Y resolution", "%4.0f", 0, 8192, 0, 1024);
    SimulatorSettingsNP[SIM_XSIZE].fill("SIM_XSIZE", "X Pixel Size", "%4.2f", 0, 60, 0, 2.4);
    SimulatorSettingsNP[SIM_YSIZE].fill("SIM_YSIZE", "Y Pixel Size", "%4.2f", 0, 60, 0, 2.4);
    SimulatorSettingsNP[SIM_MAXVAL].fill("SIM_MAXVAL", "Maximum ADU", "%4.0f", 0, 65000, 0, 65000);
    SimulatorSettingsNP[SIM_BIAS].fill("SIM_BIAS", "Bias", "%4.0f", 0, 6000, 0, 10);
    SimulatorSettingsNP[SIM_SATURATION].fill("SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 0, 1.0);
    SimulatorSettingsNP[SIM_LIMITINGMAG].fill("SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 0, 17.0);
    SimulatorSettingsNP[SIM_NOISE].fill("SIM_NOISE", "Noise", "%4.0f", 0, 6000, 0, 10);
    SimulatorSettingsNP[SIM_SKYGLOW].fill("SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 0, 19.5);
    SimulatorSettingsNP[SIM_OAGOFFSET].fill("SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 0, 0);
    SimulatorSettingsNP[SIM_POLAR].fill("SIM_POLAR", "PAE (arcminutes)", "%4.3f", -600, 600, 0,
                                        0); /* PAE = Polar Alignment Error */
    SimulatorSettingsNP[SIM_POLARDRIFT].fill("SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.3f", 0, 6000, 0, 0);
    SimulatorSettingsNP[SIM_ROTATION].fill("SIM_ROTATION", "Rotation Offset", "%4.1f", -180, 180, 0, 0);
    SimulatorSettingsNP[SIM_KING_GAMMA].fill("SIM_KING_GAMMA", "(CP,TCP), deg", "%4.1f", 0, 10, 0, 0);
    SimulatorSettingsNP[SIM_KING_THETA].fill("SIM_KING_THETA", "hour hangle, deg", "%4.1f", 0, 360, 0, 0);
    SimulatorSettingsNP[SIM_TIME_FACTOR].fill("SIM_TIME_FACTOR", "Time Factor (x)", "%.2f", 0.01, 100, 0, 1);

    SimulatorSettingsNP[SIM_SEEING].fill("SIM_SEEING", "Seeing (a-s)", "%4.1f", 0, 20, 0, 6);
    SimulatorSettingsNP[SIM_RA_DRIFT].fill("SIM_RA_DRIFT", "RA drift (a-s/second)", "%5.3f", -2, 2, 0, 0.05);
    SimulatorSettingsNP[SIM_DEC_DRIFT].fill("SIM_DEC_DRIFT", "DEC drift (a-s/second)", "%5.3f", -2, 2, 0, -0.05);
    SimulatorSettingsNP[SIM_RA_RAND].fill("SIM_RA_RAND", "Max random RA add (a-s)", "%5.3f", -2, 2, 0, 0.2);
    SimulatorSettingsNP[SIM_DEC_RAND].fill("SIM_DEC_RAND", "Max random DEC add (a-s)", "%5.3f", -2, 2, 0,
                                           0.3);
    SimulatorSettingsNP[SIM_PE_PERIOD].fill("SIM_PE_PERIOD", "Periodic error period (secs)", "%4.1f", 0, 1000, 0, 120);
    SimulatorSettingsNP[SIM_PE_MAX].fill("SIM_PE_MAX", "Periodic error maxval (a-s)", "%4.1f", 0, 100, 0, 3);
    SimulatorSettingsNP[SIM_TEMPERATURE].fill("SIM_TEMPERATURE", "Temperature (°C)", "%4.1f", -100, 100, 0, 25);

    SimulatorSettingsNP.fill(getDeviceName(), "SIMULATOR_SETTINGS",
                             "Config", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);
    // load() is important to fill all editfields with saved values also, so ISNewNumber() of one field
    // doesn't update the other fields of the group with the "old" contents.
    SimulatorSettingsNP.load();
    // RGB Simulation
    SimulateRgbSP[SIMULATE_YES].fill("SIMULATE_YES", "Yes", ISS_OFF);
    SimulateRgbSP[SIMULATE_NO].fill("SIMULATE_NO", "No", ISS_ON);
    SimulateRgbSP.fill(getDeviceName(), "SIMULATE_RGB", "Simulate RGB",
                       SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // CCD Gain
    GainNP[0].fill("GAIN", "Gain", "%.f", 0, 100, 10, 50);
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    EqPENP[RA_PE].fill("RA_PE", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    EqPENP[DEC_PE].fill("DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    EqPENP.fill(getDeviceName(), "EQUATORIAL_PE", "EQ PE", SIMULATOR_TAB, IP_RW, 60,
                IPS_IDLE);

    // Timeout
    ToggleTimeoutSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    ToggleTimeoutSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    ToggleTimeoutSP.fill(getDeviceName(), "CCD_TIMEOUT", "Timeout", SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceTP[0].getText(), "EQUATORIAL_PE");
#else
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_EOD_COORD");
#endif

    TemperatureNP.setPermission(IP_RO);
    TemperatureNP[0].setValue(25);

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_BIN;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_SHUTTER;
    cap |= CCD_HAS_ST4_PORT;
    cap |= CCD_HAS_STREAMING;

    SetCCDCapability(cap);

    // This should be called after the initial SetCCDCapability (above)
    // as it modifies the capabilities.
    setRGB(m_SimulateRGB);

    addDebugControl();

    setDriverInterface(getDriverInterface());

    return true;
}

void GuideSim::setRGB(bool onOff)
{
    if (onOff)
    {
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        BayerTP[CFA_OFFSET_X].setText("0");
        BayerTP[CFA_OFFSET_Y].setText("0");
        BayerTP[CFA_TYPE].setText("RGGB");
    }
    else
    {
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }
}

void GuideSim::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineProperty(SimulatorSettingsNP);
    defineProperty(EqPENP);
    defineProperty(SimulateRgbSP);
    defineProperty(ToggleTimeoutSP);
}

bool GuideSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(GainNP);

        SetupParms();

        if (HasGuideHead())
        {
            SetGuiderParams(500, 290, 16, 9.8, 12.6);
            GuideCCD.setFrameBufferSize(GuideCCD.getXRes() * GuideCCD.getYRes() * 2);
        }
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(GainNP);
    }

    return true;
}

bool GuideSim::StartExposure(float duration)
{
    //  for the simulator, we can just draw the frame now
    //  and it will get returned at the right time
    //  by the timer routines
    m_AbortPrimaryFrame = false;
    m_ExposureRequest   = duration;

    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&m_ExpStart, nullptr);
    //  Leave the proper time showing for the draw routines
    DrawCcdFrame(&PrimaryCCD);
    //  Now compress the actual wait time
    m_ExposureRequest = duration * m_TimeFactor;
    InExposure      = true;

    return true;
}

bool GuideSim::AbortExposure()
{
    if (!InExposure)
        return true;

    m_AbortPrimaryFrame = true;

    return true;
}

void GuideSim::TimerHit()
{
    uint32_t nextTimer = getCurrentPollingPeriod();

    //  No need to reset timer if we are not connected anymore
    if (!isConnected())
        return;

    if (InExposure && ToggleTimeoutSP.findOnSwitchIndex() == INDI_DISABLED)
    {
        if (m_AbortPrimaryFrame)
        {
            InExposure        = false;
            m_AbortPrimaryFrame = false;
        }
        else
        {
            float timeleft;
            timeleft = CalcTimeLeft(m_ExpStart, m_ExposureRequest);

            //IDLog("CCD Exposure left: %g - Request: %g\n", timeleft, m_ExposureRequest);
            if (timeleft < 0)
                timeleft = 0;

            PrimaryCCD.setExposureLeft(timeleft);

            if (timeleft < 1.0)
            {
                if (timeleft <= 0.001)
                {
                    InExposure = false;
                    PrimaryCCD.binFrame();
                    ExposureComplete(&PrimaryCCD);
                }
                else
                {
                    //  set a shorter timer
                    nextTimer = timeleft * 1000;
                }
            }
        }
    }

    SetTimer(nextTimer);
}

int GuideSim::DrawCcdFrame(INDI::CCDChip * targetChip)
{
    double exposure_time;

    uint16_t * ptr = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

    if (Streamer->isStreaming())
        exposure_time = (m_ExposureRequest < 1) ? (m_ExposureRequest * 100) : m_ExposureRequest * 2;
    else
        exposure_time = m_ExposureRequest;

    exposure_time *= (1 + sqrt(GainNP[0].getValue()));

    auto targetFocalLength = ScopeInfoNP[FOCAL_LENGTH].getValue() > 0 ? ScopeInfoNP[FOCAL_LENGTH].getValue() :
                             snoopedFocalLength;

    if (m_ShowStarField)
    {
        double PEOffset = 0;
        double rad;   // telescope RA in degrees
        double rar;   // telescope RA in radians
        double decr;  // telescope Dec in radians

        time_t now;
        time(&now);
        if (!m_RunStartInitialized || difftime(now, m_LastSim) > 30)
        {
            m_RunStartInitialized = true;
            time(&m_RunStart);
        }
        m_LastSim = now;

        const double timesince = difftime(now, m_RunStart);

        if (m_PEPeriod != 0 && m_PEMax != 0)
        {
            const double PESpot = 2.0 * 3.14159 * timesince / m_PEPeriod;
            PEOffset = m_PEMax * std::sin(PESpot) / 3600.0;
        }

        // Camera rotation: manual offset + snooped rotator + pier flip
        m_RotationOffset = SimulatorSettingsNP[SIM_ROTATION].getValue();
        double theta = m_RotationOffset;
        if (!std::isnan(RotatorAngle))
            theta += RotatorAngle;
        if (pierSide == 1)
            theta -= 180;
        theta = range360(theta);
        LOGF_DEBUG("Rotator Angle: %f, Camera Rotation: %f", RotatorAngle, theta);

#ifdef USE_EQUATORIAL_PE
        if (m_UsePE)
        {
            m_CurrentRA  = raPE + m_GuideWEOffset;
            m_CurrentDEC = decPE + m_GuideNSOffset;
        }
        else
        {
            static bool s_warnedNoSnoop = false;
            if (!s_warnedNoSnoop)
            {
                LOG_WARN("EQUATORIAL_PE not yet snooped -- rendering at raw EQUATORIAL_EOD_COORD (mount errors not active)");
                s_warnedNoSnoop = true;
            }
#endif
            m_CurrentRA  = RA;
            m_CurrentDEC = Dec;

            if (std::isnan(m_CurrentRA))
            {
                m_CurrentRA = 0;
                m_CurrentDEC = 0;
            }

            INDI::IEquatorialCoordinates epochPos { m_CurrentRA, m_CurrentDEC }, J2000Pos { 0, 0 };
            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            m_CurrentRA  = J2000Pos.rightascension;
            m_CurrentDEC = J2000Pos.declination;
            m_CurrentDEC += m_GuideNSOffset;
            m_CurrentRA += m_GuideWEOffset;
#ifdef USE_EQUATORIAL_PE
        }
#endif

        // Linear drift in degrees (drift rate in arcsec/sec * elapsed seconds / 3600)
        const float raTDrift  = timesince * m_RaTimeDrift  / 3600.0f;
        const float decTDrift = timesince * m_DecTimeDrift / 3600.0f;

        // Random offsets for RA and DEC in degrees
        double raRandomDrift  = 0;
        double decRandomDrift = 0;
        constexpr int scale = 1000000;
        if (m_RaRand > 0)
        {
            const int raScale = scale * m_RaRand;
            raRandomDrift = ((random() % (2 * raScale)) - raScale) / (3600.0 * scale);
        }
        if (m_DecRand > 0)
        {
            const int decScale = scale * m_DecRand;
            decRandomDrift = ((random() % (2 * decScale)) - decScale) / (3600.0 * scale);
        }

        rad = m_CurrentRA * 15.0 + PEOffset + raTDrift + raRandomDrift;
        rar = rad * DEGREES_TO_RADIANS;

        float cameradec = m_CurrentDEC + m_OAGoffset / 60.0f;
        decr = cameradec * DEGREES_TO_RADIANS;

        const double decDrift = (m_PolarDrift * m_PolarError * cos(decr)) / 3.81;
        decr += (decRandomDrift + decTDrift + decDrift / 3600.0) * DEGREES_TO_RADIANS;
        cameradec = static_cast<float>(decr / DEGREES_TO_RADIANS);

        if (m_KingGamma > 0.)
        {
            // King cone-error transform -- see E.S. King (1902AnHar..41..153K)
            // Differential correction to RA and Dec for alt-az polar alignment error.
            double J2decr = m_CurrentDEC * DEGREES_TO_RADIANS;
            double sid    = get_local_sidereal_time(this->Longitude);
            double JnHAr  = get_local_hour_angle(sid, RA) * 15.0 * DEGREES_TO_RADIANS;

            double J2_mnt_d_rar  = m_KingGamma * sin(J2decr) * sin(JnHAr - m_KingTheta) / cos(J2decr);
            double J2_mnt_rar    = rar - J2_mnt_d_rar;

            double J2_mnt_d_decr = m_KingGamma * cos(JnHAr - m_KingTheta);
            double J2_mnt_decr   = decr + J2_mnt_d_decr;

            if (J2_mnt_decr > M_PI / 2.)
            {
                J2_mnt_decr = M_PI / 2. - (J2_mnt_decr - M_PI / 2.);
                J2_mnt_rar -= M_PI;
            }
            J2_mnt_rar = fmod(J2_mnt_rar, 2. * M_PI);

            PEOffset  = 0.;
            rar       = J2_mnt_rar;
            rad       = rar / DEGREES_TO_RADIANS;
            decr      = J2_mnt_decr;
            cameradec = static_cast<float>(decr / DEGREES_TO_RADIANS);
        }

        // King mode needs a wider GSC search to ensure the plate solver has enough stars.
        double const kingMinRadius = (m_KingGamma > 0.) ? 60.0 : 0.0;

        INDI::CCDChip::CCD_FRAME ftype = targetChip->getFrameType();
        bool const isLight = (ftype == INDI::CCDChip::LIGHT_FRAME);
        bool const isFlat  = (ftype == INDI::CCDChip::FLAT_FRAME);

        RenderConfig cfg;
        cfg.maxVal        = m_MaxVal;
        cfg.limitingMag   = m_LimitingMag;
        cfg.saturationMag = m_SaturationMag;
        cfg.seeing        = m_Seeing;
        // Guide sim sky glow: no extra boost for light frames (guide images are typically short)
        cfg.skyGlow = isLight ? m_SkyGlow : m_SkyGlow / 10.0f;
        m_Renderer.setConfig(cfg);

        std::unique_lock<std::mutex> guard(ccdBufferLock);

        if (isLight || isFlat)
        {
            int drawn = m_Renderer.renderFrame(targetChip, rad, cameradec,
                                               targetFocalLength, theta,
                                               static_cast<float>(exposure_time), isLight,
                                               kingMinRadius);
            if (isLight && drawn < 0)
                LOG_ERROR("Error launching gsc, is it installed with appropriate environment variables set?");
            else if (isLight && drawn == 0)
                LOG_WARN("No stars found in field -- check gsc catalog coverage for this region.");
        }
        else
        {
            memset(targetChip->getFrameBuffer(), 0, targetChip->getFrameBufferSize());
        }

        m_Renderer.applyReadoutNoise(targetChip, m_Bias, m_MaxNoise);
    }
    else
    {
        m_TestValue++;
        if (m_TestValue > 255)
            m_TestValue = 0;
        uint16_t val = m_TestValue;

        int nbuf = targetChip->getSubW() * targetChip->getSubH();

        for (int x = 0; x < nbuf; x++)
        {
            *ptr = val++;
            ptr++;
        }
    }
    return 0;
}

float GuideSim::CalcTimeLeft(timeval start, float req)
{
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);
    double const timesince = (now.tv_sec  * 1000.0 + now.tv_usec  / 1000.0)
                             - (start.tv_sec * 1000.0 + start.tv_usec / 1000.0);
    return static_cast<float>(req - timesince / 1000.0);
}

IPState GuideSim::GuideNorth(uint32_t v)
{
    m_GuideNSOffset    += v / 1000.0 * m_GuideRate / 3600;
    return IPS_OK;
}

IPState GuideSim::GuideSouth(uint32_t v)
{
    m_GuideNSOffset    += v / -1000.0 * m_GuideRate / 3600;
    return IPS_OK;
}

IPState GuideSim::GuideEast(uint32_t v)
{
    float c   = v / 1000.0 * m_GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(m_CurrentDEC * DEGREES_TO_RADIANS));

    m_GuideWEOffset += c;

    return IPS_OK;
}

IPState GuideSim::GuideWest(uint32_t v)
{
    float c   = v / -1000.0 * m_GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(m_CurrentDEC * DEGREES_TO_RADIANS));

    m_GuideWEOffset += c;

    return IPS_OK;
}

bool GuideSim::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (GainNP.isNameMatch(name))
        {
            GainNP.update(values, names, n);
            GainNP.setState(IPS_OK);
            GainNP.apply();
            return true;
        }

        if (strcmp(name, "SIMULATOR_SETTINGS") == 0)
        {
            SimulatorSettingsNP.update(values, names, n);
            SimulatorSettingsNP.setState(IPS_OK);

            //  Reset our parameters now
            SetupParms();
            SimulatorSettingsNP.apply();

            m_MaxNoise      = SimulatorSettingsNP[SIM_NOISE].getValue();
            m_SkyGlow       = SimulatorSettingsNP[SIM_SKYGLOW].getValue();
            m_MaxVal        = SimulatorSettingsNP[SIM_MAXVAL].getValue();
            m_Bias          = SimulatorSettingsNP[SIM_BIAS].getValue();
            m_LimitingMag   = SimulatorSettingsNP[SIM_LIMITINGMAG].getValue();
            m_SaturationMag = SimulatorSettingsNP[SIM_SATURATION].getValue();
            m_OAGoffset = SimulatorSettingsNP[SIM_OAGOFFSET].getValue();
            m_PolarError = SimulatorSettingsNP[SIM_POLAR].getValue();
            m_PolarDrift = SimulatorSettingsNP[SIM_POLARDRIFT].getValue();
            m_RotationOffset = SimulatorSettingsNP[SIM_ROTATION].getValue();
            //  Kwiq++
            m_KingGamma = SimulatorSettingsNP[SIM_KING_GAMMA].getValue() * DEGREES_TO_RADIANS;
            m_KingTheta = SimulatorSettingsNP[SIM_KING_THETA].getValue() * DEGREES_TO_RADIANS;
            m_TimeFactor = SimulatorSettingsNP[SIM_TIME_FACTOR].getValue();

            m_Seeing       = SimulatorSettingsNP[SIM_SEEING].getValue();
            m_RaTimeDrift  = SimulatorSettingsNP[SIM_RA_DRIFT].getValue();
            m_DecTimeDrift = SimulatorSettingsNP[SIM_DEC_DRIFT].getValue();
            m_RaRand       = SimulatorSettingsNP[SIM_RA_RAND].getValue();
            m_DecRand      = SimulatorSettingsNP[SIM_DEC_RAND].getValue();
            m_PEPeriod     = SimulatorSettingsNP[SIM_PE_PERIOD].getValue();
            m_PEMax        = SimulatorSettingsNP[SIM_PE_MAX].getValue();
            m_TemperatureRequest = SimulatorSettingsNP[SIM_TEMPERATURE].getValue();
            TemperatureNP[0].setValue(m_TemperatureRequest);
            TemperatureNP.apply();

            return true;
        }

        // Record PE EQ to simulate different position in the sky than actual mount coordinate
        // This can be useful to simulate Periodic Error or cone error or any arbitrary error.
        if (EqPENP.isNameMatch(name))
        {
            EqPENP.update(values, names, n);
            EqPENP.setState(IPS_OK);

            INDI::IEquatorialCoordinates epochPos { EqPENP[AXIS_RA].getValue(), EqPENP[AXIS_DE].getValue() }, J2000Pos { 0, 0 };
            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            m_CurrentRA  = J2000Pos.rightascension;
            m_CurrentDEC = J2000Pos.declination;
            m_UsePE = true;
            EqPENP.apply();
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool GuideSim::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (SimulateRgbSP.isNameMatch(name))
        {
            SimulateRgbSP.update(states, names, n);
            int index = SimulateRgbSP.findOnSwitchIndex();
            if (index == -1)
            {
                SimulateRgbSP.setState(IPS_ALERT);
                LOG_INFO("Cannot determine whether RGB simulation should be switched on or off.");
                SimulateRgbSP.apply();
                return false;
            }

            m_SimulateRGB = index == 0;
            setRGB(m_SimulateRGB);

            SimulateRgbSP[SIMULATE_YES].setState(m_SimulateRGB ? ISS_ON : ISS_OFF);
            SimulateRgbSP[SIMULATE_NO].setState(m_SimulateRGB ? ISS_OFF : ISS_ON);
            SimulateRgbSP.setState(IPS_OK);
            SimulateRgbSP.apply();

            return true;
        }

        if (ToggleTimeoutSP.isNameMatch(name))
        {
            ToggleTimeoutSP.update(states, names, n);
            ToggleTimeoutSP.setState(IPS_OK);
            ToggleTimeoutSP.apply();
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

void GuideSim::activeDevicesUpdated()
{
#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_PE");
#endif
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_EOD_COORD");
}

bool GuideSim::ISSnoopDevice(XMLEle * root)
{
    // We try to snoop EQUATORIAL_PE first (true pointing with mount errors injected);
    // if not found we fall through to the regular EQUATORIAL_EOD_COORD snoop below.
#ifdef USE_EQUATORIAL_PE
    const char * propName = findXMLAttValu(root, "name");
    if (!strcmp(propName, "EQUATORIAL_PE"))
    {
        XMLEle * ep = nullptr;
        int rc_ra = -1, rc_de = -1;
        double newra = 0, newdec = 0;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");

            if (!strcmp(elemName, "RA_PE"))
                rc_ra = f_scansexa(pcdataXMLEle(ep), &newra);
            else if (!strcmp(elemName, "DEC_PE"))
                rc_de = f_scansexa(pcdataXMLEle(ep), &newdec);
        }

        if (rc_ra == 0 && rc_de == 0)
        {
            INDI::IEquatorialCoordinates epochPos { newra, newdec }, J2000Pos { 0, 0 };
            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            raPE  = J2000Pos.rightascension;
            decPE = J2000Pos.declination;

            // Only activate PE rendering once the telescope reports a valid (Ok) state.
            // The initial defNumberVector arrives with state Idle and values 0/0.
            const char * state = findXMLAttValu(root, "state");
            if (!m_UsePE && strcmp(state, "Ok") == 0)
            {
                LOGF_INFO("EQUATORIAL_PE snoop active: JNow RA %.4f Dec %.4f -> J2000 RA %.4f Dec %.4f", newra, newdec, raPE, decPE);
                m_UsePE = true;
            }

            EqPENP[AXIS_RA].setValue(newra);
            EqPENP[AXIS_DE].setValue(newdec);
            EqPENP.apply();

            LOGF_DEBUG("Snooped EQUATORIAL_PE JNow RA %g Dec %g -> J2000 RA %g Dec %g", newra, newdec, raPE, decPE);

            return true;
        }
    }
#endif

    return INDI::CCD::ISSnoopDevice(root);
}

bool GuideSim::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);


    // Save CCD Simulator Config
    SimulatorSettingsNP.save(fp);

    // Gain
    GainNP.save(fp);

    // RGB
    SimulateRgbSP.save(fp);

    return true;
}

bool GuideSim::StartStreaming()
{
    m_ExposureRequest = 1.0 / Streamer->getTargetExposure();
    pthread_mutex_lock(&condMutex);
    m_StreamPredicate = 1;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool GuideSim::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    m_StreamPredicate = 0;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool GuideSim::UpdateCCDFrame(int x, int y, int w, int h)
{
    long bin_width  = w / PrimaryCCD.getBinX();
    long bin_height = h / PrimaryCCD.getBinY();

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    Streamer->setSize(bin_width, bin_height);

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);
}

bool GuideSim::UpdateCCDBin(int hor, int ver)
{
    if (hor == 3 || ver == 3)
    {
        LOG_ERROR("3x3 binning is not supported.");
        return false;
    }

    long bin_width  = PrimaryCCD.getSubW() / hor;
    long bin_height = PrimaryCCD.getSubH() / ver;

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    Streamer->setSize(bin_width, bin_height);

    return INDI::CCD::UpdateCCDBin(hor, ver);
}

void * GuideSim::streamVideoHelper(void * context)
{
    return static_cast<GuideSim *>(context)->streamVideo();
}

void * GuideSim::streamVideo()
{
    auto start = std::chrono::high_resolution_clock::now();
    auto finish = std::chrono::high_resolution_clock::now();

    while (true)
    {
        pthread_mutex_lock(&condMutex);

        while (m_StreamPredicate == 0)
        {
            pthread_cond_wait(&cv, &condMutex);
            m_ExposureRequest = Streamer->getTargetExposure();
        }

        if (m_TerminateThread)
            break;

        // release condMutex
        pthread_mutex_unlock(&condMutex);


        // 16 bit
        DrawCcdFrame(&PrimaryCCD);

        PrimaryCCD.binFrame();

        finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;

        if (elapsed.count() < m_ExposureRequest)
            usleep(fabs(m_ExposureRequest - elapsed.count()) * 1e6);

        uint32_t size = PrimaryCCD.getFrameBufferSize() / (PrimaryCCD.getBinX() * PrimaryCCD.getBinY());
        Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);

        start = std::chrono::high_resolution_clock::now();
    }

    pthread_mutex_unlock(&condMutex);
    return nullptr;
}

void GuideSim::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    fitsKeywords.push_back({"GAIN", GainNP[0].getValue(), 3, "Gain"});
}
