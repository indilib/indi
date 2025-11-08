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

#include "locale_compat.h"

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

    SimulatorSettingsNP[SIM_XRES].fill("SIM_XRES", "CCD X resolution", "%4.0f", 0, 8192, 0, 1280);
    SimulatorSettingsNP[SIM_YRES].fill("SIM_YRES", "CCD Y resolution", "%4.0f", 0, 8192, 0, 1024);
    SimulatorSettingsNP[SIM_XSIZE].fill("SIM_XSIZE", "CCD X Pixel Size", "%4.2f", 0, 60, 0, 2.4);
    SimulatorSettingsNP[SIM_YSIZE].fill("SIM_YSIZE", "CCD Y Pixel Size", "%4.2f", 0, 60, 0, 2.4);
    SimulatorSettingsNP[SIM_MAXVAL].fill("SIM_MAXVAL", "CCD Maximum ADU", "%4.0f", 0, 65000, 0, 65000);
    SimulatorSettingsNP[SIM_BIAS].fill("SIM_BIAS", "CCD Bias", "%4.0f", 0, 6000, 0, 10);
    SimulatorSettingsNP[SIM_SATURATION].fill("SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 0, 1.0);
    SimulatorSettingsNP[SIM_LIMITINGMAG].fill("SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 0, 17.0);
    SimulatorSettingsNP[SIM_NOISE].fill("SIM_NOISE", "CCD Noise", "%4.0f", 0, 6000, 0, 10);
    SimulatorSettingsNP[SIM_SKYGLOW].fill("SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 0, 19.5);
    SimulatorSettingsNP[SIM_OAGOFFSET].fill("SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 0, 0);
    SimulatorSettingsNP[SIM_POLAR].fill("SIM_POLAR", "PAE (arcminutes)", "%4.3f", -600, 600, 0,
                                        0); /* PAE = Polar Alignment Error */
    SimulatorSettingsNP[SIM_POLARDRIFT].fill("SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.3f", 0, 6000, 0, 0);
    SimulatorSettingsNP[SIM_ROTATION].fill("SIM_ROTATION", "Rotation Offset", "%4.1f", -360, 360, 0, 0);
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

float GuideSim::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
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
    //  CCD frame is 16 bit data
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
        double rad;  //  telescope ra in degrees
        double rar;  //  telescope ra in radians
        double decr; //  telescope dec in radians;
        int nwidth = 0, nheight = 0;

        time_t now;
        time(&now);
        if (!m_RunStartInitialized || difftime(now, m_LastSim) > 30)
        {
            // Start the clock when the first image is produced or if we haven't sim'd in a while.
            m_RunStartInitialized = true;
            time(&m_RunStart);
        }
        m_LastSim = now;

        //  Lets figure out where we are on the pe curve
        const double timesince = difftime(now, m_RunStart);

        //  This is our spot in the periodic error curve
        if (m_PEPeriod != 0 && m_PEMax != 0)
        {
            const double PESpot = 2.0 * 3.14159 * timesince / m_PEPeriod;
            PEOffset = m_PEMax * std::sin(PESpot) / 3600.0; //  convert to degrees
        }

        //  Spin up a set of plate constants that will relate
        //  ra/dec of stars, to our fictitious ccd layout

        //  to account for various rotations etc
        //  we should spin up some plate constants here
        //  then we can use these constants to rotate and offset
        //  the standard co-ordinates on each star for drawing
        //  a ccd frame;
        double pa, pb, pc, pd, pe, pf;
        // Pixels per radian
        double pprx, ppry;
        // Scale in arcsecs per pixel
        double Scalex;
        double Scaley;
        // CCD width in pixels
        double ccdW = targetChip->getXRes();

        // Pixels per radian
        pprx = targetFocalLength / targetChip->getPixelSizeX() * 1000;
        ppry = targetFocalLength / targetChip->getPixelSizeY() * 1000;

        //  we do a simple scale for x and y locations
        //  based on the focal length and pixel size
        //  focal length in mm, pixels in microns
        // JM: 2015-03-17: Using a simpler formula, Scalex and Scaley are in arcsecs/pixel
        Scalex = (targetChip->getPixelSizeX() / targetFocalLength) * 206.3;
        Scaley = (targetChip->getPixelSizeY() / targetFocalLength) * 206.3;

#if 0
        DEBUGF(
            INDI::Logger::DBG_DEBUG,
            "pprx: %g pixels per radian ppry: %g pixels per radian ScaleX: %g arcsecs/pixel ScaleY: %g arcsecs/pixel",
            pprx, ppry, Scalex, Scaley);
#endif

        m_RotationOffset = SimulatorSettingsNP[SIM_ROTATION].getValue();
        double theta = m_RotationOffset;
        if (!std::isnan(RotatorAngle))
            theta += RotatorAngle;
        if (pierSide == 1)
            theta -= 180;       // rotate 180 if on East
        theta = range360(theta);
        LOGF_DEBUG("Rotator Angle: %f, Camera Rotation: %f", RotatorAngle, theta);

        // JM: 2015-03-17: Next we do a rotation assuming CW for angle theta
        // TS: 2025-06-09: Below we have "Invert horizontally" and in the end
        // this produces a rotation CCW with origin N (TODO: adjust matrix?)
        pa = pprx * cos(theta * M_PI / 180.0);
        pb = ppry * sin(theta * M_PI / 180.0);

        pd = pprx * -sin(theta * M_PI / 180.0);
        pe = ppry * cos(theta * M_PI / 180.0);

        nwidth = targetChip->getXRes();
        pc     = nwidth / 2;

        nheight = targetChip->getYRes();
        pf      = nheight / 2;

        m_ImageScaleX = Scalex;
        m_ImageScaleY = Scaley;

#ifdef USE_EQUATORIAL_PE
        if (!m_UsePE)
        {
#endif
            m_CurrentRA  = RA;
            m_CurrentDEC = Dec;

            if (std::isnan(m_CurrentRA))
            {
                m_CurrentRA = 0;
                m_CurrentDEC = 0;
            }

            INDI::IEquatorialCoordinates epochPos { m_CurrentRA, m_CurrentDEC }, J2000Pos { 0, 0 };
            // Convert from JNow to J2000
            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            m_CurrentRA  = J2000Pos.rightascension;
            m_CurrentDEC = J2000Pos.declination;
            m_CurrentDEC += m_GuideNSOffset;
            m_CurrentRA += m_GuideWEOffset;
#ifdef USE_EQUATORIAL_PE
        }
#endif

        // Linear drift, number of seconds multiplied by drift/sec in arcsec.
        const float raTDrift = timesince * m_RaTimeDrift / 3600.0;
        const float decTDrift = timesince * m_DecTimeDrift / 3600.0;

        // Random offsets for RA and DEC. The random drifts will be small, so multiply by
        // scale as random() produces an integer. Drifts are in degrees.
        double raRandomDrift = 0;
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

        //  calc this now, we will use it a lot later
        rad = m_CurrentRA * 15.0  + PEOffset + raTDrift + raRandomDrift;
        rar = rad * DEGREES_TO_RADIANS;

        //  offsetting the dec by the guide head offset
        float cameradec = m_CurrentDEC + m_OAGoffset / 60;
        decr = cameradec * DEGREES_TO_RADIANS;

        const double decDrift = (m_PolarDrift * m_PolarError * cos(decr)) / 3.81;

        // Add declination drift, if any.
        decr += (decRandomDrift + decTDrift + decDrift / 3600.0) * DEGREES_TO_RADIANS;

        //  Calculate the radius we need to fetch
        float radius = sqrt((Scalex * Scalex * targetChip->getXRes() / 2.0 * targetChip->getXRes() / 2.0) +
                            (Scaley * Scaley * targetChip->getYRes() / 2.0 * targetChip->getYRes() / 2.0));
        //  we have radius in arcseconds now
        radius = radius / 60; //  convert to arcminutes
#if 0
        LOGF_DEBUG("Lookup radius %4.2f", radius);
#endif

        //  A m_SaturationMag star saturates in one second
        //  and a limitingmag produces a one adu level in one second
        //  solve for zero point and system gain

        double zeroPointK = (m_SaturationMag - m_LimitingMag) / ((-2.5 * log(m_MaxVal)) - (-2.5 * log(1.0 / 2.0)));
        double zeroPointZ = m_SaturationMag - zeroPointK * (-2.5 * log(m_MaxVal));
        //zeroPointZ = zeroPointZ + m_SaturationMag;

        //IDLog("K=%4.2f  Z=%4.2f\n",zeroPointK,zeroPointZ);

        //  Should probably do some math here to figure out the dimmest
        //  star we can see on this exposure
        //  and only fetch to that magnitude
        //  for now, just use the limiting mag number with some room to spare
        float lookuplimit = m_LimitingMag;

        if (radius > 60)
            lookuplimit = 11;

        if (m_KingGamma > 0.)
        {
            // wildi, make sure there are always stars, e.g. in case where m_KingGamma is set to 1 degree.
            // Otherwise the solver will fail.
            radius = 60.;

            // wildi, transform to telescope coordinate system, differential form
            // see E.S. King based on Chauvenet:
            // https://ui.adsabs.harvard.edu/link_gateway/1902AnHar..41..153K/ADS_PDF
            // Currently it is not possible to enable the logging in simulator devices (tested with ccd and telescope)
            // Replace LOGF_DEBUG by IDLog
            //IDLog("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++", m_KingGamma); // without variable, macro expansion fails
            char JnRAStr[64] = {0};
            fs_sexa(JnRAStr, RA, 2, 360000);
            char JnDecStr[64] = {0};
            fs_sexa(JnDecStr, Dec, 2, 360000);
            //            IDLog("Longitude      : %8.3f, Latitude    : %8.3f\n", this->Longitude, this->Latitude);
            //            IDLog("King gamma     : %8.3f, King theta  : %8.3f\n", m_KingGamma / DEGREES_TO_RADIANS, m_KingTheta / DEGREES_TO_RADIANS);
            //            IDLog("Jnow RA        : %11s,       dec: %11s\n", JnRAStr, JnDecStr );
            //            IDLog("Jnow RA        : %8.3f, Dec         : %8.3f\n", RA * 15., Dec);
            //            IDLog("J2000    Pos.ra: %8.3f,      Pos.dec: %8.3f\n", J2000Pos.ra, J2000Pos.dec);
            // Since the catalog is J2000, we  are going back in time
            // tra, tdec are at the center of the projection center for the simulated
            // images
            //double J2ra = J2000Pos.ra;  // J2000Pos: 0,360, RA: 0,24
            double J2dec = J2000Pos.declination;

            //double J2rar = J2ra * DEGREES_TO_RADIANS;
            double J2decr = J2dec * DEGREES_TO_RADIANS;
            double sid  = get_local_sidereal_time(this->Longitude);
            // HA is what is observed, that is Jnow
            // ToDo check if mean or apparent
            double JnHAr  = get_local_hour_angle(sid, RA) * 15. * DEGREES_TO_RADIANS;

            char sidStr[64] = {0};
            fs_sexa(sidStr, sid, 2, 3600);
            char JnHAStr[64] = {0};
            fs_sexa(JnHAStr, JnHAr / 15. / DEGREES_TO_RADIANS, 2, 360000);

            //            IDLog("sid            : %s\n", sidStr);
            //            IDLog("Jnow                               JnHA: %8.3f degree\n", JnHAr / DEGREES_TO_RADIANS);
            //            IDLog("                                JnHAStr: %11s hms\n", JnHAStr);
            // m_KingTheta is the HA of the great circle where the HA axis is in.
            // RA is a right and HA a left handed coordinate system.
            // apparent or J2000? apparent, since we live now :-)

            // Transform to the mount coordinate system
            // remember it is the center of the simulated image
            double J2_mnt_d_rar = m_KingGamma * sin(J2decr) * sin(JnHAr - m_KingTheta) / cos(J2decr);
            double J2_mnt_rar = rar - J2_mnt_d_rar
                                ; // rad = m_CurrentRA * 15.0; rar = rad * DEGREES_TO_RADIANS; m_CurrentRA  = J2000Pos.ra / 15.0;

            // Imagine the HA axis points to HA=0, dec=89deg, then in the mount's coordinate
            // system a star at true dec = 88 is seen at 89 deg in the mount's system
            // Or in other words: if one uses the setting circle, that is the mount system,
            // and set it to 87 deg then the real location is at 88 deg.
            double J2_mnt_d_decr = m_KingGamma * cos(JnHAr - m_KingTheta);
            double J2_mnt_decr = decr + J2_mnt_d_decr
                                 ; // decr      = cameradec * DEGREES_TO_RADIANS; cameradec = m_CurrentDEC + m_OAGoffset / 60; m_CurrentDEC = J2000Pos.dec;
            //            IDLog("raw mod ra     : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / DEGREES_TO_RADIANS, J2_mnt_decr / DEGREES_TO_RADIANS );
            if (J2_mnt_decr > M_PI / 2.)
            {
                J2_mnt_decr = M_PI / 2. - (J2_mnt_decr - M_PI / 2.);
                J2_mnt_rar -= M_PI;
            }
            J2_mnt_rar = fmod(J2_mnt_rar, 2. * M_PI) ;
            //            IDLog("mod sin        : %8.3f,          cos: %8.3f\n", sin(JnHAr - m_KingTheta), cos(JnHAr - m_KingTheta));
            //            IDLog("mod dra        : %8.3f,         ddec: %8.3f (degree)\n", J2_mnt_d_rar / DEGREES_TO_RADIANS, J2_mnt_d_decr / DEGREES_TO_RADIANS );
            //            IDLog("mod ra         : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / DEGREES_TO_RADIANS, J2_mnt_decr / DEGREES_TO_RADIANS );
            //IDLog("mod ra         : %11s,       dec: %11s\n",  );
            char J2RAStr[64] = {0};
            fs_sexa(J2RAStr, J2_mnt_rar / 15. / DEGREES_TO_RADIANS, 2, 360000);
            char J2DecStr[64] = {0};
            fs_sexa(J2DecStr, J2_mnt_decr / DEGREES_TO_RADIANS, 2, 360000);
            //            IDLog("mod ra         : %s,       dec: %s\n", J2RAStr, J2DecStr );
            //            IDLog("PEOffset       : %10.5f setting it to ZERO\n", PEOffset);
            PEOffset = 0.;
            // feed the result to the original variables
            rar = J2_mnt_rar ;
            rad = rar / DEGREES_TO_RADIANS;
            decr = J2_mnt_decr;
            cameradec = decr / DEGREES_TO_RADIANS;
            //            IDLog("mod ra      rad: %8.3f (degree)\n", rad);
        }
        //  if this is a light frame, we need a star field drawn
        INDI::CCDChip::CCD_FRAME ftype = targetChip->getFrameType();

        std::unique_lock<std::mutex> guard(ccdBufferLock);

        //  Start by clearing the frame buffer
        memset(targetChip->getFrameBuffer(), 0, targetChip->getFrameBufferSize());

        if (ftype == INDI::CCDChip::LIGHT_FRAME)
        {
            AutoCNumeric locale;
            char gsccmd[250];
            FILE * pp;
            int drawn = 0;

            sprintf(gsccmd, "gsc -c %8.6f %+8.6f -r %4.1f -m 0 %4.2f -n 3000",
                    range360(rad),
                    rangeDec(cameradec),
                    radius,
                    lookuplimit);

            if (!Streamer->isStreaming() || (m_KingGamma > 0.))
                LOGF_DEBUG("GSC Command: %s", gsccmd);

            pp = popen(gsccmd, "r");
            if (pp != nullptr)
            {
                char line[256];

                while (fgets(line, 256, pp) != nullptr)
                {
                    //  ok, lets parse this line for specifics we want
                    char id[20];
                    char plate[6];
                    char ob[6];
                    float mag;
                    float mage;
                    float ra;
                    float dec;
                    float pose;
                    int band;
                    float dist;
                    int dir;
                    int c;

                    int rc = sscanf(line, "%10s %f %f %f %f %f %d %d %4s %2s %f %d", id, &ra, &dec, &pose, &mag, &mage,
                                    &band, &c, plate, ob, &dist, &dir);
                    if (rc == 12)
                    {
                        //  Convert the ra/dec to standard co-ordinates
                        double sx;    //  standard co-ords
                        double sy;    //
                        double srar;  //  star ra in radians
                        double sdecr; //  star dec in radians;
                        double ccdx;
                        double ccdy;

                        srar  = ra * DEGREES_TO_RADIANS;
                        sdecr = dec * DEGREES_TO_RADIANS;
                        //  Handbook of astronomical image processing
                        //  page 253
                        //  equations 9.1 and 9.2
                        //  convert ra/dec to standard co-ordinates

                        sx = cos(sdecr) * sin(srar - rar) /
                             (cos(decr) * cos(sdecr) * cos(srar - rar) + sin(decr) * sin(sdecr));
                        sy = (sin(decr) * cos(sdecr) * cos(srar - rar) - cos(decr) * sin(sdecr)) /
                             (cos(decr) * cos(sdecr) * cos(srar - rar) + sin(decr) * sin(sdecr));

                        //  now convert to pixels
                        ccdx = pa * sx + pb * sy + pc;
                        ccdy = pd * sx + pe * sy + pf;

                        // Invert horizontally and transform CW to CCW (see above)
                        ccdx = ccdW - ccdx;

                        rc = DrawImageStar(targetChip, mag, ccdx, ccdy, exposure_time, zeroPointK, zeroPointZ);
                        drawn += rc;
                        if (rc == 1)
                        {
                            //LOGF_DEBUG("star %s scope %6.4f %6.4f star %6.4f %6.4f ccd %6.2f %6.2f",id,rad,decPE,ra,dec,ccdx,ccdy);
                            //LOGF_DEBUG("star %s ccd %6.2f %6.2f",id,ccdx,ccdy);
                        }
                    }
                }
                pclose(pp);
            }
            else
            {
                LOG_ERROR("Error looking up stars, is gsc installed with appropriate environment variables set ??");
            }
            if (drawn == 0)
            {
                LOG_ERROR("Got no stars, is gsc installed with appropriate environment variables set ??");
            }
        }
        //fprintf(stderr,"Got %d stars from %d lines drew %d\n",stars,lines,drawn);

        //  now we need to add background sky glow, with vignetting
        //  this is essentially the same math as drawing a dim star with
        //  fwhm equivalent to the full field of view

        if (ftype == INDI::CCDChip::LIGHT_FRAME || ftype == INDI::CCDChip::FLAT_FRAME)
        {
            float skyflux;
            //  calculate flux from our zero point and gain values
            float glow = m_SkyGlow;

            if (ftype == INDI::CCDChip::FLAT_FRAME)
            {
                //  Assume flats are done with a diffuser
                //  in broad daylight, so, the sky magnitude
                //  is much brighter than at night
                glow = m_SkyGlow / 10;
            }

            //fprintf(stderr,"Using glow %4.2f\n",glow);

            skyflux = pow(10, ((glow - zeroPointZ) * zeroPointK / -2.5));
            //  ok, flux represents one second now
            //  scale up linearly for exposure time
            skyflux = skyflux * exposure_time;
            //IDLog("SkyFlux = %g m_ExposureRequest %g\n",skyflux,exposure_time);

            uint16_t * pt = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

            nheight = targetChip->getSubH();
            nwidth  = targetChip->getSubW();

            for (int y = 0; y < nheight; y++)
            {
                for (int x = 0; x < nwidth; x++)
                {
                    float dc; //  distance from center
                    float fp; //  flux this pixel;
                    float sx, sy;
                    float vig;

                    sx = nwidth / 2 - x;
                    sy = nheight / 2 - y;

                    vig = nwidth;
                    vig = vig * m_ImageScaleX;
                    //  need to make this account for actual pixel size
                    dc = std::sqrt(sx * sx * m_ImageScaleX * m_ImageScaleX + sy * sy * m_ImageScaleY * m_ImageScaleY);
                    //  now we have the distance from center, in arcseconds
                    //  now lets plot a gaussian falloff to the edges
                    //
                    float fa;
                    fa = exp(-2.0 * 0.7 * (dc * dc) / vig / vig);

                    //  get the current value
                    fp = pt[0];

                    //  Add the sky glow
                    fp += skyflux;

                    //  now scale it for the vignetting
                    fp = fa * fp;

                    //  clamp to limits
                    if (fp > m_MaxVal)
                        fp = m_MaxVal;
                    if (fp > m_MaxPix)
                        m_MaxPix = fp;
                    if (fp < m_MinPix)
                        m_MinPix = fp;
                    //  and put it back
                    pt[0] = fp;
                    pt++;
                }
            }
        }

        //  Now we add some bias and read noise
        int subX = targetChip->getSubX();
        int subY = targetChip->getSubY();
        int subW = targetChip->getSubW() + subX;
        int subH = targetChip->getSubH() + subY;

        if (m_MaxNoise > 0)
        {
            for (int x = subX; x < subW; x++)
            {
                for (int y = subY; y < subH; y++)
                {
                    int noise;

                    noise = random();
                    noise = noise % m_MaxNoise; //

                    //IDLog("noise is %d\n", noise);
                    AddToPixel(targetChip, x, y, m_Bias + noise);
                }
            }
        }
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

int GuideSim::DrawImageStar(INDI::CCDChip * targetChip, float mag, float x, float y, float exposure_time, double zeroPointK,
                            double zeroPointZ)
{
    const int subX = targetChip->getSubX();
    const int subY = targetChip->getSubY();
    const int subW = targetChip->getSubW() + subX;
    const int subH = targetChip->getSubH() + subY;

    if ((x < subX) || (x > subW || (y < subY) || (y > subH)))
    {
        //  this star is not on the ccd frame anyways
        return 0;
    }

    //  Calculate flux from our zero point and gain values
    //  Mag represents one second, scale up linearly for exposure time.
    const double flux = exposure_time * pow(10, ((mag - zeroPointZ) * zeroPointK / -2.5));

    const double seeingSquared = m_Seeing * m_Seeing;
    const double pixelPartX = x - static_cast<int>(x);
    const double pixelPartY = y - static_cast<int>(y);

    int drew     = 0;
    const int boxSize = static_cast<int>(3 * m_Seeing / m_ImageScaleY) + 1;
    for (int sy = -boxSize; sy <= boxSize; sy++)
    {
        for (int sx = -boxSize; sx <= boxSize; sx++)
        {
            //  Need to make this account for actual pixel size
            const double dx = m_ImageScaleX * (sx - pixelPartX);
            const double dy = m_ImageScaleY * (sy - pixelPartY);
            //  Distance from center (arcseconds).
            const float distanceSquared = dx * dx  + dy * dy;
            float pixelFlux = flux * exp(-2.0 * 0.7 * distanceSquared / seeingSquared);

            if (pixelFlux < 0)
                pixelFlux = 0;

            if (AddToPixel(targetChip, x + sx, y + sy, pixelFlux) != 0)
                drew = 1;
        }
    }
    return drew;
}

int GuideSim::AddToPixel(INDI::CCDChip * targetChip, int x, int y, int val)
{
    int nwidth  = targetChip->getSubW();
    int nheight = targetChip->getSubH();

    x -= targetChip->getSubX();
    y -= targetChip->getSubY();

    int drew = 0;
    if (x >= 0)
    {
        if (x < nwidth)
        {
            if (y >= 0)
            {
                if (y < nheight)
                {
                    unsigned short * pt;
                    int newval;
                    drew++;

                    pt = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

                    pt += (y * nwidth);
                    pt += x;
                    newval = pt[0];
                    newval += val;
                    if (newval > m_MaxVal)
                        newval = m_MaxVal;
                    if (newval > m_MaxPix)
                        m_MaxPix = newval;
                    if (newval < m_MinPix)
                        m_MinPix = newval;
                    pt[0] = newval;
                }
            }
        }
    }
    return drew;
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
            m_RotationCW = SimulatorSettingsNP[SIM_ROTATION].getValue();
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
    IDSnoopDevice(ActiveDeviceTP[0].getText(), "EQUATORIAL_PE");
#else
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_EOD_COORD");
#endif
}

bool GuideSim::ISSnoopDevice(XMLEle * root)
{
    // We try to snoop EQPEC first, if not found, we snoop regular EQNP
#ifdef USE_EQUATORIAL_PE
    const char * propName = findXMLAttValu(root, "name");
    if (!strcmp(propName, EqPENP.name))
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

        if (rc_ra == 0 && rc_de == 0 && ((newra != raPE) || (newdec != decPE)))
        {
            INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
            epochPos.ra  = newra * 15.0;
            epochPos.dec = newdec;
            ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);
            raPE  = J2000Pos.ra / 15.0;
            decPE = J2000Pos.dec;
            m_UsePE = true;

            EqPEN[AXIS_RA].value = newra;
            EqPEN[AXIS_DE].value = newdec;
            IDSetNumber(&EqPENP, nullptr);

            LOGF_DEBUG("raPE %g  decPE %g Snooped raPE %g  decPE %g", raPE, decPE, newra, newdec);

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
