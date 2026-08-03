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

#include "ccd_simulator.h"
#include "indicom.h"
#include "stream/streammanager.h"

#include <libnova/julian_day.h>
#include <libastro.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

static std::unique_ptr<CCDSim> ccdsim(new CCDSim());

CCDSim::CCDSim() : INDI::FilterInterface(this)
{
    currentRA  = RA;
    currentDE = Dec;

    terminateThread = false;

    time(&RunStart);

    // Filter stuff
    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(8);
}

bool CCDSim::setupParameters()
{
    SetCCDParams(SimulatorSettingsNP[SIM_XRES].getValue(),
                 SimulatorSettingsNP[SIM_YRES].getValue(),
                 16,
                 SimulatorSettingsNP[SIM_XSIZE].getValue(),
                 SimulatorSettingsNP[SIM_YSIZE].getValue());

    m_MaxNoise      = SimulatorSettingsNP[SIM_NOISE].getValue();
    m_SkyGlow       = SimulatorSettingsNP[SIM_SKYGLOW].getValue();
    m_MaxVal        = SimulatorSettingsNP[SIM_MAXVAL].getValue();
    m_Bias          = OffsetNP[0].getValue();
    m_LimitingMag   = SimulatorSettingsNP[SIM_LIMITINGMAG].getValue();
    m_SaturationMag = SimulatorSettingsNP[SIM_SATURATION].getValue();
    //  An oag is offset this much from center of scope position (arcminutes);
    m_OAGOffset = SimulatorSettingsNP[SIM_OAGOFFSET].getValue();
    m_PolarError = SimulatorSettingsNP[SIM_POLAR].getValue();
    m_PolarDrift = SimulatorSettingsNP[SIM_POLARDRIFT].getValue();
    m_PEPeriod = SimulatorSettingsNP[SIM_PE_PERIOD].getValue();
    m_PEMax = SimulatorSettingsNP[SIM_PE_MAX].getValue();
    m_TimeFactor = SimulatorSettingsNP[SIM_TIME_FACTOR].getValue();
    // This is the rotation of the simulated camera respective to North.
    // Because the simulated star field is calculated with their RA/DEC-coordinates
    // (see DrawCcdFrame()) the origin angle of star field points north. So this value
    // for EQ mounts normally simulate a certain camera offset and is a constant.
    // For ALTAZ-mount this variable is altered consecutively by the value of the parallactic
    // angle (transfered through a signal from KStars/skymapdrawabstract.cpp) and this way used
    // to simulate the deviation of the camera orientation from N.
    m_RotationOffset = SimulatorSettingsNP[SIM_ROTATION].getValue();

    uint32_t nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    if (!m_PlanetMode)
        Streamer->setPixelFormat(INDI_MONO, 16);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    return true;
}

bool CCDSim::Connect()
{
    streamPredicate = 0;
    terminateThread = false;
    pthread_create(&primary_thread, nullptr, &streamVideoHelper, this);
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool CCDSim::Disconnect()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    terminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

const char * CCDSim::getDefaultName()
{
    return "CCD Simulator";
}

bool CCDSim::initProperties()
{
    INDI::CCD::initProperties();

    // Constrain temperature range to match TEC physics: -15°C to ambient (25°C)
    TemperatureNP[0].setMin(-15);
    TemperatureNP[0].setMax(25);
    TemperatureNP[0].setValue(0);

    CaptureFormat format = {"INDI_MONO", "Mono", 16, true};
    addCaptureFormat(format);

    // Planet mode uses RGB capture format
    CaptureFormat rgbFormat = {"INDI_RGB", "RGB", 16, false};
    addCaptureFormat(rgbFormat);

    SimulatorSettingsNP[SIM_XRES].fill("SIM_XRES", "X resolution", "%4.0f", 512, 8192, 512, 1280);
    SimulatorSettingsNP[SIM_YRES].fill("SIM_YRES", "Y resolution", "%4.0f", 512, 8192, 512, 1024);
    SimulatorSettingsNP[SIM_XSIZE].fill("SIM_XSIZE", "X Pixel Size", "%4.2f", 1, 30, 5, 5.2);
    SimulatorSettingsNP[SIM_YSIZE].fill("SIM_YSIZE", "Y Pixel Size", "%4.2f", 1, 30, 5, 5.2);
    SimulatorSettingsNP[SIM_MAXVAL].fill("SIM_MAXVAL", "Maximum ADU", "%4.0f", 255, 65000, 1000, 65000);
    SimulatorSettingsNP[SIM_SATURATION].fill("SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 1, 1.0);
    SimulatorSettingsNP[SIM_LIMITINGMAG].fill("SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 1, 17.0);
    SimulatorSettingsNP[SIM_NOISE].fill("SIM_NOISE", "Noise", "%4.0f", 0, 6000, 500, 10);
    SimulatorSettingsNP[SIM_SKYGLOW].fill("SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 500, 19.5);
    SimulatorSettingsNP[SIM_OAGOFFSET].fill("SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 500, 0);
    SimulatorSettingsNP[SIM_POLAR].fill("SIM_POLAR", "PAE (arcminutes)", "%4.1f", -600, 600, 100, 0);
    SimulatorSettingsNP[SIM_POLARDRIFT].fill("SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.1f", 0, 60, 5, 0);
    SimulatorSettingsNP[SIM_PE_PERIOD].fill("SIM_PEPERIOD", "PE Period (seconds)", "%4.1f", 0, 60, 5, 0);
    SimulatorSettingsNP[SIM_PE_MAX].fill("SIM_PEMAX", "PE Max (arcsec)", "%4.1f", 0, 6000, 500, 0);
    SimulatorSettingsNP[SIM_TIME_FACTOR].fill("SIM_TIME_FACTOR", "Time Factor (x)", "%.2f", 0.01, 100, 10, 1);
    SimulatorSettingsNP[SIM_ROTATION].fill("SIM_ROTATION", "Rotation Offset", "%.2f", -180, 180, 10, 0);

    SimulatorSettingsNP.fill(getDeviceName(), "SIMULATOR_SETTINGS",
                             "Settings", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);
    // load() is important to fill all editfields with saved values also, so ISNewNumber() of one field
    // doesn't update the other fields of the group with the "old" contents.
    SimulatorSettingsNP.load();

    // Sim Tests
    SimTestsSP[SIM_TESTS_BAYER].fill("BAYER", "Bayer", ISS_OFF);
    SimTestsSP[SIM_TESTS_CRASH].fill("CRASH", "Crash", ISS_OFF);
    SimTestsSP[SIM_TESTS_TIMEOUT].fill("TIMEOUT", "Timeout", ISS_OFF);
    SimTestsSP.fill(getDeviceName(), "SIM_TESTS", "Sim Tests", SIMULATOR_TAB, IP_RW,
                    ISR_NOFMANY, 60, IPS_IDLE);

    // Simulate focusing
    FocusSimulationNP[SIM_FOCUS_POSITION].fill("SIM_FOCUS_POSITION", "Focus", "%.f", 0.0, 100000.0, 1.0, 36700.0);
    FocusSimulationNP[SIM_FOCUS_MAX].fill("SIM_FOCUS_MAX", "Max. Position", "%.f", 0.0, 100000.0, 1.0, 100000.0);
    FocusSimulationNP[SIM_SEEING].fill("SIM_SEEING", "Seeing (arcsec)", "%4.2f", 0, 60, 0, 3.5);
    FocusSimulationNP.fill(getDeviceName(), "SIM_FOCUSING", "Focus Simulation",
                           SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);

    // Simulate sensor tilt (extra defocus in arcsec at sensor edge)
    TiltSimulationNP[SIM_TILT_LR].fill("SIM_TILT_LR", "Left-Right (arcsec)", "%4.2f", 0, 20, 0.5, 0);
    TiltSimulationNP[SIM_TILT_TB].fill("SIM_TILT_TB", "Top-Bottom (arcsec)", "%4.2f", 0, 20, 0.5, 0);
    TiltSimulationNP.fill(getDeviceName(), "SIM_TILT", "Tilt Simulation",
                          SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);

    // Periodic Error
    EqPENP[AXIS_RA].fill("RA_PE", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    EqPENP[AXIS_DE].fill("DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    EqPENP.fill(getDeviceName(), "EQUATORIAL_PE", "EQ PE", SIMULATOR_TAB, IP_RW, 60,
                IPS_IDLE);

    // FWHM
    auto focuser = ActiveDeviceTP[ACTIVE_FOCUSER].getText() ? ActiveDeviceTP[ACTIVE_FOCUSER].getText() : "";
    IUFillNumber(&FWHMN[0], "SIM_FWHM", "FWHM (arcseconds)", "%4.2f", 0, 60, 0, 7.5);
    IUFillNumberVector(&FWHMNP, FWHMN, 1, focuser, "FWHM", "FWHM", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    // Cooler
    CoolerSP[INDI_ENABLED].fill("COOLER_ON", "ON", ISS_OFF);
    CoolerSP[INDI_DISABLED].fill("COOLER_OFF", "OFF", ISS_ON);
    CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                  ISR_1OFMANY, 0, IPS_IDLE);

    // Gain
    GainNP[0].fill("GAIN", "value", "%.f", 0, 300, 10, 90);
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Offset
    OffsetNP[0].fill("OFFSET", "value", "%.f", 0, 6000, 500, 0);
    OffsetNP.fill(getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Directory to read images from. This is useful to test real images captured by camera
    // For each capture, one file is read (sorted by name) and is sent to client.
    DirectoryTP[0].fill("LOCATION", "Location", getenv("HOME"));
    DirectoryTP.fill(getDeviceName(), "CCD_DIRECTORY_LOCATION", "Directory", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);
    DirectoryTP.load();

    // Toggle Directory Reading. If enabled. The simulator will just read images from the directory and not generate them.
    DirectorySP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    DirectorySP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    DirectorySP.fill(getDeviceName(), "CCD_DIRECTORY_TOGGLE", "Use Dir.", SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Resolution
    for (uint8_t i = 0; i < Resolutions.size(); i++)
    {
        std::ostringstream ss;
        if (Resolutions[i].first > 0)
            ss << Resolutions[i].first << " x " << Resolutions[i].second;
        else
            ss << "Custom";
        ResolutionSP[i].fill(ss.str().c_str(), ss.str().c_str(), i == 0 ? ISS_ON : ISS_OFF);
    }
    ResolutionSP.fill(getDeviceName(), "CCD_RESOLUTION", "Resolution", SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    auto mount = ActiveDeviceTP[ACTIVE_TELESCOPE].getText() ? ActiveDeviceTP[ACTIVE_TELESCOPE].getText() : "";

#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(mount, "EQUATORIAL_PE");
#else
    IDSnoopDevice(mount, "EQUATORIAL_EOD_COORD");
#endif

    IDSnoopDevice(focuser, "FWHM");

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_BIN;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_COOLER;
    cap |= CCD_HAS_GUIDE_HEAD;
    cap |= CCD_HAS_SHUTTER;
    cap |= CCD_HAS_ST4_PORT;
    cap |= CCD_HAS_STREAMING;
    cap |= CCD_HAS_DSP;

    SetCCDCapability(cap);

    // This should be called after the initial SetCCDCapability (above)
    // as it modifies the capabilities.
    setBayerEnabled(m_SimulateBayer);

    INDI::FilterInterface::initProperties(FILTER_TAB);

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(8);

    // ===================== Planet Simulation =====================

    // Planet mode toggle
    PlanetModeSP[PLANET_ENABLED].fill("PLANET_ENABLED", "Enabled", ISS_OFF);
    PlanetModeSP[PLANET_DISABLED].fill("PLANET_DISABLED", "Disabled", ISS_ON);
    PlanetModeSP.fill(getDeviceName(), "PLANET_MODE", "Planet Mode", SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Planet selection
    PlanetSelectSP[PLANET_MERCURY].fill("PLANET_MERCURY", "Mercury", ISS_OFF);
    PlanetSelectSP[PLANET_VENUS].fill("PLANET_VENUS", "Venus", ISS_OFF);
    PlanetSelectSP[PLANET_MARS].fill("PLANET_MARS", "Mars", ISS_OFF);
    PlanetSelectSP[PLANET_JUPITER].fill("PLANET_JUPITER", "Jupiter", ISS_ON);
    PlanetSelectSP[PLANET_SATURN].fill("PLANET_SATURN", "Saturn", ISS_OFF);
    PlanetSelectSP[PLANET_URANUS].fill("PLANET_URANUS", "Uranus", ISS_OFF);
    PlanetSelectSP[PLANET_NEPTUNE].fill("PLANET_NEPTUNE", "Neptune", ISS_OFF);
    PlanetSelectSP[PLANET_MOON].fill("PLANET_MOON", "Moon", ISS_OFF);
    PlanetSelectSP[PLANET_SUN].fill("PLANET_SUN", "Sun", ISS_OFF);
    PlanetSelectSP.fill(getDeviceName(), "PLANET_SELECT", "Planet", SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Planet settings: refresh interval (minutes) and size override (arcsec)
    PlanetSettingsNP[PLANET_REFRESH].fill("PLANET_REFRESH", "Refresh (min)", "%4.0f", 1, 1440, 10, 60);
    PlanetSettingsNP[PLANET_SIZE].fill("PLANET_SIZE", "Size (arcsec, 0=auto)", "%4.1f", 0, 3600, 10, 0);
    PlanetSettingsNP.fill(getDeviceName(), "PLANET_SETTINGS", "Planet Settings", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);
    PlanetSettingsNP.load();

    // xplanet executable and config paths
    PlanetPathsTP[PLANET_XPLANET_PATH].fill("XPLANET_PATH", "xplanet binary", "xplanet");
    PlanetPathsTP[PLANET_XPLANET_CONFIG].fill("XPLANET_CONFIG", "Config file", "/usr/share/xplanet/config/default");
    PlanetPathsTP.fill(getDeviceName(), "PLANET_PATHS", "xplanet Paths", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);
    PlanetPathsTP.load();

    addDebugControl();

    setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

    return true;
}

void CCDSim::setBayerEnabled(bool onOff)
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

void CCDSim::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineProperty(SimulatorSettingsNP);
    defineProperty(EqPENP);
    defineProperty(FocusSimulationNP);
    defineProperty(TiltSimulationNP);
    defineProperty(SimTestsSP);

    // Planet simulation properties
    defineProperty(PlanetModeSP);
    defineProperty(PlanetSelectSP);
    defineProperty(PlanetSettingsNP);
    defineProperty(PlanetPathsTP);
}

bool CCDSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
            defineProperty(CoolerSP);

        defineProperty(GainNP);
        defineProperty(OffsetNP);

        defineProperty(DirectoryTP);
        defineProperty(DirectorySP);
        defineProperty(ResolutionSP);

        setupParameters();

        if (HasGuideHead())
        {
            SetGuiderParams(500, 290, 16, 9.8, 12.6);
            GuideCCD.setFrameBufferSize(GuideCCD.getXRes() * GuideCCD.getYRes() * 2);
        }

        // Define the Filter Slot and name properties
        INDI::FilterInterface::updateProperties();
    }
    else
    {
        if (HasCooler())
            deleteProperty(CoolerSP);

        deleteProperty(GainNP);
        deleteProperty(OffsetNP);
        deleteProperty(DirectoryTP);
        deleteProperty(DirectorySP);
        deleteProperty(ResolutionSP);

        INDI::FilterInterface::updateProperties();
    }

    return true;
}

int CCDSim::SetTemperature(double temperature, bool enableCooler)
{
    TemperatureRequest = temperature;
    if (std::abs(temperature - TemperatureNP[0].getValue()) < 0.1)
    {
        TemperatureNP[0].setValue(temperature);
        return 1;
    }

    if (enableCooler)
    {
        auto isCooling = temperature < TemperatureNP[0].getValue();
        CoolerSP[INDI_ENABLED].setState(isCooling ? ISS_ON : ISS_OFF);
        CoolerSP[INDI_DISABLED].setState(isCooling ? ISS_OFF : ISS_ON);
        CoolerSP.setState(isCooling ? IPS_BUSY : IPS_IDLE);
        CoolerSP.apply();
    }

    return 0;
}

bool CCDSim::StartExposure(float duration)
{
    //  for the simulator, we can just draw the frame now
    //  and it will get returned at the right time
    //  by the timer routines
    AbortPrimaryFrame = false;
    ExposureRequest   = duration;

    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart, nullptr);

    // Planet mode: use cached xplanet rendering
    if (m_PlanetMode && PrimaryCCD.getFrameType() == INDI::CCDChip::LIGHT_FRAME)
    {
        time_t now;
        time(&now);
        double elapsedMinutes = difftime(now, m_LastPlanetRenderTime) / 60.0;
        if (m_CachedPlanetBuffer.empty() || elapsedMinutes >= m_PlanetRefreshMinutes)
        {
            if (renderPlanet() == false)
            {
                // Fallback to blank frame on failure
                memset(PrimaryCCD.getFrameBuffer(), 0, PrimaryCCD.getFrameBufferSize());
            }
        }
        else
        {
            // Ensure buffer is sized correctly (NAXIS=3 already set by SetCaptureFormat)
            uint32_t rgbBufSize = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3 * 2;
            if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(rgbBufSize))
                PrimaryCCD.setFrameBufferSize(rgbBufSize);
            // Use cached planet buffer
            size_t copySize = std::min(m_CachedPlanetBuffer.size(),
                                       static_cast<size_t>(PrimaryCCD.getFrameBufferSize()));
            memcpy(PrimaryCCD.getFrameBuffer(), m_CachedPlanetBuffer.data(), copySize);
            LOGF_DEBUG("Using cached planet render (%.1f minutes since last render)", elapsedMinutes);
        }
    }
    else if (PrimaryCCD.getFrameType() == INDI::CCDChip::LIGHT_FRAME && DirectorySP[INDI_ENABLED].getState() == ISS_ON)
    {
        if (loadNextImage() == false)
            return false;
    }
    else
    {
        // Reset to mono (NAXIS=2) if coming from planet mode
        if (PrimaryCCD.getNAxis() == 3)
        {
            PrimaryCCD.setNAxis(2);
            uint32_t nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
            PrimaryCCD.setFrameBufferSize(nbuf);
            // Re-enable Bayer if previously enabled (not applicable during planet mode)
            setBayerEnabled(m_SimulateBayer);
        }
        DrawCcdFrame(&PrimaryCCD);
    }

    //  Now compress the actual wait time
    ExposureRequest = duration * m_TimeFactor;
    InExposure      = true;

    return true;
}

bool CCDSim::StartGuideExposure(float n)
{
    GuideExposureRequest = n;
    AbortGuideFrame      = false;
    GuideCCD.setExposureDuration(n);
    DrawCcdFrame(&GuideCCD);
    gettimeofday(&GuideExpStart, nullptr);
    InGuideExposure = true;
    return true;
}

bool CCDSim::AbortExposure()
{
    if (!InExposure)
        return true;

    AbortPrimaryFrame = true;

    return true;
}

bool CCDSim::AbortGuideExposure()
{
    //IDLog("Enter AbortGuideExposure\n");
    if (!InGuideExposure)
        return true; //  no need to abort if we aren't doing one
    AbortGuideFrame = true;
    return true;
}

void CCDSim::TimerHit()
{
    uint32_t nextTimer = getCurrentPollingPeriod();

    //  No need to reset timer if we are not connected anymore
    if (!isConnected())
        return;

    if (InExposure && !m_SimulateTimeout)
    {
        if (AbortPrimaryFrame)
        {
            InExposure        = false;
            AbortPrimaryFrame = false;
        }
        else
        {
            float timeleft;
            timeleft = CalcTimeLeft(ExpStart, ExposureRequest);

            //IDLog("CCD Exposure left: %g - Request: %g\n", timeleft, ExposureRequest);
            if (timeleft < 0)
                timeleft = 0;

            PrimaryCCD.setExposureLeft(timeleft);

            if (timeleft < 1.0)
            {
                if (timeleft <= 0.001)
                {
                    InExposure = false;
                    // Skip binning for planet mode (RGB data) and directory mode; only bin for generated star fields
                    if (!m_PlanetMode && DirectorySP[INDI_DISABLED].getState() == ISS_ON)
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

    if (InGuideExposure)
    {
        double timeleft = CalcTimeLeft(GuideExpStart, GuideExposureRequest);
        if (timeleft < 0)
            timeleft = 0;

        GuideCCD.setExposureLeft(timeleft);

        if (timeleft < 1.0)
        {
            if (timeleft <= 0.001)
            {
                InGuideExposure = false;
                if (!AbortGuideFrame)
                {
                    GuideCCD.binFrame();
                    ExposureComplete(&GuideCCD);
                    if (InGuideExposure)
                    {
                        //  the call to complete triggered another exposure
                        timeleft = CalcTimeLeft(GuideExpStart, GuideExposureRequest);
                        if (timeleft < 1.0)
                        {
                            nextTimer = timeleft * 1000;
                        }
                    }
                }
                else
                {
                    //IDLog("Not sending guide frame cuz of abort\n");
                }
                AbortGuideFrame = false;
            }
            else
            {
                nextTimer = timeleft * 1000; //  set a shorter timer
            }
        }
    }

    // Simulate realistic TEC cooler with ambient temperature of 25°C.
    // Minimum achievable temperature: 40°C below ambient (-15°C).
    // Warming up beyond ambient is not possible (TEC cannot heat).
    if (TemperatureNP.getState() == IPS_BUSY)
    {
        constexpr double AMBIENT_TEMPERATURE = 25.0;
        constexpr double MIN_TEC_TEMPERATURE = -15.0; // 40°C below ambient

        double currentTemp = TemperatureNP[0].getValue();
        double targetTemp = TemperatureRequest;

        // Clamp target to achievable range
        if (targetTemp > AMBIENT_TEMPERATURE)
            targetTemp = AMBIENT_TEMPERATURE;
        if (targetTemp < MIN_TEC_TEMPERATURE)
            targetTemp = MIN_TEC_TEMPERATURE;

        if (targetTemp < currentTemp)
        {
            // Cooling: approach target at -0.5°C per TimerHit cycle
            currentTemp = std::max(targetTemp, currentTemp - 0.5);
        }
        else if (targetTemp > currentTemp)
        {
            // Warming: approach target naturally (ambient), slowing as we get closer
            // Simulate exponential decay toward ambient
            double deltaT = AMBIENT_TEMPERATURE - currentTemp;
            // When below ambient, warm up by ~20% of remaining gap per cycle
            // (roughly 0.4°C at 10°C delta, slowing to ~0.02°C near ambient)
            double warmingStep = deltaT * 0.2;
            if (warmingStep < 0.02)
                warmingStep = 0.02;
            if (warmingStep > 0.5)
                warmingStep = 0.5;
            currentTemp = std::min(targetTemp, currentTemp + warmingStep);
        }

        TemperatureNP[0].setValue(currentTemp);

        if (std::abs(currentTemp - m_LastTemperature) > 0.1)
        {
            m_LastTemperature = currentTemp;
            TemperatureNP.apply();
        }

        // Cooler is effectively off when at or near ambient
        if (currentTemp >= AMBIENT_TEMPERATURE - 0.1)
        {
            CoolerSP[INDI_ENABLED].setState(ISS_OFF);
            CoolerSP[INDI_DISABLED].setState(ISS_ON);
            CoolerSP.setState(IPS_IDLE);
            CoolerSP.apply();
        }
    }


    SetTimer(nextTimer);
}

int CCDSim::DrawCcdFrame(INDI::CCDChip * targetChip)
{
    float exposure_time;

    uint16_t * ptr = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

    if (targetChip->getXRes() == 500)
        exposure_time = GuideExposureRequest * 4;
    else if (Streamer->isStreaming())
        exposure_time = (ExposureRequest < 1) ? (ExposureRequest * 100) : ExposureRequest * 2;
    else
        exposure_time = ExposureRequest;

    exposure_time *= (1 + sqrt(GainNP[0].getValue()));

    auto targetFocalLength = ScopeInfoNP[FOCAL_LENGTH].getValue() > 0 ? ScopeInfoNP[FOCAL_LENGTH].getValue() :
                             snoopedFocalLength;

    if (ShowStarField)
    {
        float PEOffset {0};

        if (m_PEPeriod > 0)
        {
            time_t now;
            time(&now);
            double timesince = difftime(now, RunStart);
            double PESpot = timesince / m_PEPeriod * 2.0 * 3.14159;
            PEOffset = m_PEMax * std::sin(PESpot) / 3600.0f;
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
        if (usePE)
        {
            currentRA = raPE + guideWEOffset;
            currentDE = decPE + guideNSOffset;
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
            currentRA  = RA;
            currentDE = Dec;

            if (std::isnan(currentRA))
            {
                currentRA = 0;
                currentDE = 0;
            }

            INDI::IEquatorialCoordinates epochPos { currentRA, currentDE }, J2000Pos { 0, 0 };
            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            currentRA = J2000Pos.rightascension;
            currentDE = J2000Pos.declination;

            currentDE += guideNSOffset;
            currentRA += guideWEOffset;
#ifdef USE_EQUATORIAL_PE
        }
#endif

        // Field center: RA in degrees (with PE), Dec with OAG offset and polar drift
        double ra_deg     = currentRA * 15.0 + PEOffset;
        double cameradec  = currentDE + m_OAGOffset / 60.0;
        double decr       = cameradec * 0.0174532925;
        double decDrift   = (m_PolarDrift * m_PolarError * cos(decr)) / 3.81;
        double dec_deg    = cameradec + decDrift / 3600.0;

        INDI::CCDChip::CCD_FRAME ftype = targetChip->getFrameType();
        bool const isLight = (ftype == INDI::CCDChip::LIGHT_FRAME);
        bool const isFlat  = (ftype == INDI::CCDChip::FLAT_FRAME);

        RenderConfig cfg;
        cfg.maxVal        = m_MaxVal;
        cfg.limitingMag   = m_LimitingMag;
        cfg.saturationMag = m_SaturationMag;
        cfg.seeing        = m_Seeing;
        cfg.tiltLR        = TiltSimulationNP[SIM_TILT_LR].getValue();
        cfg.tiltTB        = TiltSimulationNP[SIM_TILT_TB].getValue();
        // Sky glow: 30% boost for realistic light-frame backgrounds; flat frames use diffuser daylight level
        cfg.skyGlow = isLight ? m_SkyGlow * 1.3f : m_SkyGlow / 10.0f;
        m_Renderer.setConfig(cfg);

        std::unique_lock<std::mutex> guard(ccdBufferLock);

        if (isLight || isFlat)
        {
            int drawn = m_Renderer.renderFrame(targetChip, ra_deg, dec_deg,
                                               targetFocalLength, theta, exposure_time, isLight);
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
        testvalue++;
        if (testvalue > 255)
            testvalue = 0;
        uint16_t val = testvalue;

        int nbuf = targetChip->getSubW() * targetChip->getSubH();

        for (int x = 0; x < nbuf; x++)
        {
            *ptr = val++;
            ptr++;
        }
    }
    return 0;
}

float CCDSim::CalcTimeLeft(timeval start, float req)
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

IPState CCDSim::GuideNorth(uint32_t v)
{
    guideNSOffset    += v / 1000.0 * GuideRate / 3600;
    return IPS_OK;
}

IPState CCDSim::GuideSouth(uint32_t v)
{
    guideNSOffset    += v / -1000.0 * GuideRate / 3600;
    return IPS_OK;
}

IPState CCDSim::GuideEast(uint32_t v)
{
    float c   = v / 1000.0 * GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(currentDE * 0.0174532925));

    guideWEOffset += c;

    return IPS_OK;
}

IPState CCDSim::GuideWest(uint32_t v)
{
    float c   = v / -1000.0 * GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(currentDE * 0.0174532925));

    guideWEOffset += c;

    return IPS_OK;
}

bool CCDSim::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (INDI::FilterInterface::processText(dev, name, texts, names, n))
            return true;

        if (DirectoryTP.isNameMatch(name))
        {
            DirectoryTP.update(texts, names, n);
            if (DirectorySP[INDI_ENABLED].getState() == ISS_OFF || (DirectorySP[INDI_ENABLED].getState() == ISS_ON && watchDirectory()))
                DirectoryTP.setState(IPS_OK);
            else
                DirectoryTP.setState(IPS_ALERT);
            DirectoryTP.apply();
            saveConfig(DirectoryTP);
            return true;
        }

        if (PlanetPathsTP.isNameMatch(name))
        {
            PlanetPathsTP.update(texts, names, n);
            m_XPlanetBinary = PlanetPathsTP[PLANET_XPLANET_PATH].getText();
            m_XPlanetConfig = PlanetPathsTP[PLANET_XPLANET_CONFIG].getText();
            // Invalidate cache to force re-render with new paths
            m_LastPlanetRenderTime = 0;
            PlanetPathsTP.setState(IPS_OK);
            PlanetPathsTP.apply();
            saveConfig(PlanetPathsTP);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool CCDSim::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (INDI::FilterInterface::processNumber(dev, name, values, names, n))
            return true;

        if (GainNP.isNameMatch(name))
        {
            GainNP.update(values, names, n);
            GainNP.setState(IPS_OK);
            GainNP.apply();
            return true;
        }
        if (OffsetNP.isNameMatch(name))
        {
            OffsetNP.update(values, names, n);
            OffsetNP.setState(IPS_OK);
            OffsetNP.apply();
            m_Bias = OffsetNP[0].getValue();
            return true;
        }
        else if (SimulatorSettingsNP.isNameMatch(name))
        {
            SimulatorSettingsNP.update(values, names, n);
            SimulatorSettingsNP.setState(IPS_OK);

            //  Reset our parameters now
            setupParameters();
            // Invalidate planet cache since FOV/pixel scale may have changed
            m_LastPlanetRenderTime = 0;
            SimulatorSettingsNP.apply();
            saveConfig(true, SimulatorSettingsNP.getName());
            return true;
        }
        // Record PE EQ to simulate different position in the sky than actual mount coordinate
        // This can be useful to simulate Periodic Error or cone error or any arbitrary error.
        else if (EqPENP.isNameMatch(name))
        {
            EqPENP.update(values, names, n);
            EqPENP.setState(IPS_OK);

            INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
            epochPos.rightascension  = EqPENP[AXIS_RA].getValue();
            epochPos.declination = EqPENP[AXIS_DE].getValue();

            RA = EqPENP[AXIS_RA].getValue();
            Dec = EqPENP[AXIS_DE].getValue();

            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            currentRA  = J2000Pos.rightascension;
            currentDE = J2000Pos.declination;
            usePE = true;

            EqPENP.apply();
            return true;
        }
        else if (FocusSimulationNP.isNameMatch(name))
        {
            // update focus simulation parameters
            FocusSimulationNP.update(values, names, n);
            FocusSimulationNP.setState(IPS_OK);
            FocusSimulationNP.apply();
        }
        else if (TiltSimulationNP.isNameMatch(name))
        {
            TiltSimulationNP.update(values, names, n);
            TiltSimulationNP.setState(IPS_OK);
            TiltSimulationNP.apply();
        }
        else if (PlanetSettingsNP.isNameMatch(name))
        {
            PlanetSettingsNP.update(values, names, n);
            m_PlanetRefreshMinutes = PlanetSettingsNP[PLANET_REFRESH].getValue();
            m_PlanetSizeOverride = PlanetSettingsNP[PLANET_SIZE].getValue();
            // Invalidate cache so new settings take effect next exposure
            m_LastPlanetRenderTime = 0;
            PlanetSettingsNP.setState(IPS_OK);
            PlanetSettingsNP.apply();
            saveConfig(true, PlanetSettingsNP.getName());
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool CCDSim::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Sim Tests
        if (SimTestsSP.isNameMatch(name))
        {
            SimTestsSP.update(states, names, n);

            // Bayer
            if (SimTestsSP[SIM_TESTS_BAYER].getState() == ISS_ON)
            {
                m_SimulateBayer = true;
                SimTestsSP[SIM_TESTS_BAYER].setState(ISS_ON);
            }
            else
            {
                m_SimulateBayer = false;
                SimTestsSP[SIM_TESTS_BAYER].setState(ISS_OFF);
            }
            setBayerEnabled(m_SimulateBayer);

            // Crash
            if (SimTestsSP[SIM_TESTS_CRASH].getState() == ISS_ON)
            {
                abort();
            }

            // Timeout
            if (SimTestsSP[SIM_TESTS_TIMEOUT].getState() == ISS_ON)
            {
                m_SimulateTimeout = true;
                SimTestsSP[SIM_TESTS_TIMEOUT].setState(ISS_ON);
            }
            else
            {
                m_SimulateTimeout = false;
                SimTestsSP[SIM_TESTS_TIMEOUT].setState(ISS_OFF);
            }

            SimTestsSP.setState(IPS_OK);
            SimTestsSP.apply();
            return true;
        }
        else if (CoolerSP.isNameMatch(name))
        {
            CoolerSP.update(states, names, n);

            if (CoolerSP[INDI_ENABLED].getState() == ISS_ON)
                CoolerSP.setState(IPS_BUSY);
            else
            {
                CoolerSP.setState(IPS_IDLE);
                // When cooler is turned off, warm up to ambient (25°C)
                constexpr double AMBIENT_TEMPERATURE = 25.0;
                TemperatureRequest = AMBIENT_TEMPERATURE;
                m_TargetTemperature = AMBIENT_TEMPERATURE;
                TemperatureNP.setState(IPS_BUSY);
                m_TemperatureCheckTimer.start();
                m_TemperatureElapsedTimer.start();
            }

            CoolerSP.apply();

            return true;
        }
        else if (DirectorySP.isNameMatch(name))
        {
            DirectorySP.update(states, names, n);
            m_AllFiles.clear();
            m_RemainingFiles.clear();
            if (DirectorySP[INDI_ENABLED].getState() == ISS_ON)
            {
                if (watchDirectory() == false)
                {
                    DirectorySP[INDI_ENABLED].setState(ISS_OFF);
                    DirectorySP[INDI_DISABLED].setState(ISS_ON);
                    DirectorySP.setState(IPS_ALERT);
                }
            }
            else
            {
                m_RemainingFiles.clear();
                DirectorySP.setState(IPS_OK);
                setBayerEnabled(SimTestsSP[SIM_TESTS_BAYER].getState() == ISS_ON);
                LOG_INFO("Directory-based images are disabled.");
            }
            DirectorySP.apply(nullptr);
            return true;
        }
        else if (ResolutionSP.isNameMatch(name))
        {
            ResolutionSP.update(states, names, n);
            ResolutionSP.setState(IPS_OK);
            ResolutionSP.apply();

            int index = ResolutionSP.findOnSwitchIndex();
            if (index >= 0 && index < static_cast<int>(Resolutions.size() - 1))
            {
                SimulatorSettingsNP[SIM_XRES].setValue(Resolutions[index].first);
                SimulatorSettingsNP[SIM_YRES].setValue(Resolutions[index].second);
                SetCCDParams(SimulatorSettingsNP[SIM_XRES].getValue(),
                             SimulatorSettingsNP[SIM_YRES].getValue(),
                             16,
                             SimulatorSettingsNP[SIM_XSIZE].getValue(),
                             SimulatorSettingsNP[SIM_YSIZE].getValue());
                UpdateCCDFrame(0, 0, SimulatorSettingsNP[SIM_XRES].getValue(), SimulatorSettingsNP[SIM_YRES].getValue());
                uint32_t nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
                PrimaryCCD.setFrameBufferSize(nbuf);

                SimulatorSettingsNP.apply();

                // Invalidate planet cache since resolution changed
                m_LastPlanetRenderTime = 0;
                if (m_PlanetMode)
                {
                    SetCaptureFormat(1); // Re-apply RGB format with new buffer size
                }
            }
            return true;
        }
        // Planet mode toggle
        else if (PlanetModeSP.isNameMatch(name))
        {
            PlanetModeSP.update(states, names, n);
            m_PlanetMode = (PlanetModeSP[PLANET_ENABLED].getState() == ISS_ON);
            if (m_PlanetMode)
            {
                // Switch to RGB capture format
                int rgbIndex = -1;
                for (size_t i = 0; i < m_CaptureFormats.size(); i++)
                {
                    if (m_CaptureFormats[i].name == "INDI_RGB")
                    {
                        rgbIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (rgbIndex >= 0 && CaptureFormatSP[rgbIndex].getState() != ISS_ON)
                {
                    CaptureFormatSP.reset();
                    CaptureFormatSP[rgbIndex].setState(ISS_ON);
                    CaptureFormatSP.apply();
                    SetCaptureFormat(rgbIndex);
                }
                // Disable directory mode when planet mode is active
                DirectorySP[INDI_ENABLED].setState(ISS_OFF);
                DirectorySP[INDI_DISABLED].setState(ISS_ON);
                DirectorySP.apply();
                m_AllFiles.clear();
                m_RemainingFiles.clear();
                // Invalidate cached planet so it re-renders on next exposure
                m_LastPlanetRenderTime = 0;
                LOGF_INFO("Planet simulation enabled for %s", m_SelectedPlanet.c_str());
            }
            else
            {
                // Switch back to Mono capture format
                int monoIndex = -1;
                for (size_t i = 0; i < m_CaptureFormats.size(); i++)
                {
                    if (m_CaptureFormats[i].name == "INDI_MONO")
                    {
                        monoIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (monoIndex >= 0 && CaptureFormatSP[monoIndex].getState() != ISS_ON)
                {
                    CaptureFormatSP.reset();
                    CaptureFormatSP[monoIndex].setState(ISS_ON);
                    CaptureFormatSP.apply();
                    SetCaptureFormat(monoIndex);
                }
                // Restore star field settings: pixel format, buffer, Bayer, streamer
                setupParameters();
                setBayerEnabled(m_SimulateBayer);
                LOG_INFO("Planet simulation disabled. Returning to star field mode.");
            }
            PlanetModeSP.setState(IPS_OK);
            PlanetModeSP.apply(nullptr);
            return true;
        }
        // Planet selection
        else if (PlanetSelectSP.isNameMatch(name))
        {
            PlanetSelectSP.update(states, names, n);
            int index = PlanetSelectSP.findOnSwitchIndex();
            static const char* planetNames[] =
            {
                "mercury", "venus", "mars", "jupiter", "saturn",
                "uranus", "neptune", "moon", "sun"
            };
            if (index >= 0 && index < 9)
            {
                m_SelectedPlanet = planetNames[index];
                // Invalidate cache to re-render with new planet
                m_LastPlanetRenderTime = 0;
                LOGF_INFO("Selected planet: %s", m_SelectedPlanet.c_str());
            }
            PlanetSelectSP.setState(IPS_OK);
            PlanetSelectSP.apply(nullptr);
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool CCDSim::watchDirectory()
{
    DIR* dirp = opendir(DirectoryTP[0].getText());
    if (dirp == nullptr)
    {
        LOGF_ERROR("Cannot monitor invalid directory %s", DirectoryTP[0].getText());
        return false;
    }

    struct dirent * dp;
    std::string d_dir = std::string(DirectoryTP[0].getText());
    auto directory = DirectoryTP[0].getText();
    if (directory[std::string(directory).length() - 1] != '/')
        d_dir += "/";
    while ((dp = readdir(dirp)) != NULL)
    {
        // For now, just FITS.
        if (strstr(dp->d_name, ".fits"))
            m_AllFiles.push_back(d_dir + dp->d_name);
    }
    closedir(dirp);

    if (m_AllFiles.empty())
    {
        LOGF_ERROR("No FITS files found in directory %s", directory);
        return false;
    }
    else
    {
        std::sort(m_AllFiles.begin(), m_AllFiles.end());
        m_RemainingFiles = m_AllFiles;
        LOGF_INFO("Directory-based images are enabled. Subsequent exposures will be loaded from directory %s",
                  directory);
    }

    return true;
}

void CCDSim::activeDevicesUpdated()
{
#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_PE");
#endif
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_FOCUSER].getText(), "FWHM");

    snprintf(FWHMNP.device, MAXINDIDEVICE, "%s", ActiveDeviceTP[ACTIVE_FOCUSER].getText());
}

bool CCDSim::ISSnoopDevice(XMLEle * root)
{
    if (IUSnoopNumber(root, &FWHMNP) == 0)
    {
        // we calculate the FWHM and do not snoop it from the focus simulator
        // m_Seeing = FWHMNP.np[0].value;
        return true;
    }

    XMLEle * ep           = nullptr;
    const char * propName = findXMLAttValu(root, "name");

    if (!strcmp(propName, "ABS_FOCUS_POSITION"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "FOCUS_ABSOLUTE_POSITION"))
            {
                FocuserPos = atol(pcdataXMLEle(ep));
                LOGF_DEBUG("Snooped FocuserPosition %g", FocuserPos);
                // calculate FWHM
                double focus       = FocusSimulationNP[SIM_FOCUS_POSITION].getValue();
                double max         = FocusSimulationNP[SIM_FOCUS_MAX].getValue();
                double optimalFWHM = FocusSimulationNP[SIM_SEEING].getValue();

                // limit to +/- 10
                double ticks = 20 * (FocuserPos - focus) / max;

                m_Seeing = 0.5625 * ticks * ticks + optimalFWHM;
                return true;
            }
        }
    }
    else if (!strcmp(propName, "ABS_ROTATOR_ANGLE"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "ANGLE"))
            {
                RotatorAngle = atof(pcdataXMLEle(ep));
                LOGF_DEBUG("Snooped RotatorAngle %f", RotatorAngle);
                return true;
            }
        }
    }
    // We try to snoop EQUATORIAL_PE first (true pointing with mount errors injected);
    // if not found we fall through to the regular EQUATORIAL_EOD_COORD snoop below.
#ifdef USE_EQUATORIAL_PE
    if (!strcmp(propName, "EQUATORIAL_PE"))
    {
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
            if (!usePE && strcmp(state, "Ok") == 0)
            {
                LOGF_INFO("EQUATORIAL_PE snoop active: JNow RA %.4f Dec %.4f -> J2000 RA %.4f Dec %.4f", newra, newdec, raPE, decPE);
                usePE = true;
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

bool CCDSim::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    // Save Filter Wheel Config
    INDI::FilterInterface::saveConfigItems(fp);

    // Save CCD Simulator Config
    SimulatorSettingsNP.save(fp);

    // Gain
    GainNP.save(fp);
    OffsetNP.save(fp);

    // Directory
    DirectoryTP.save(fp);

    // Resolution
    ResolutionSP.save(fp);

    // Focus simulation
    FocusSimulationNP.save(fp);

    // Tilt simulation
    TiltSimulationNP.save(fp);

    // Planet simulation
    PlanetSettingsNP.save(fp);
    PlanetPathsTP.save(fp);

    return true;
}

bool CCDSim::SelectFilter(int f)
{
    // Sleep randomly between 1500 and 2000ms to simulate filter selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1500, 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));

    CurrentFilter = f;
    SelectFilterDone(f);
    return true;
}

int CCDSim::QueryFilter()
{
    return CurrentFilter;
}

bool CCDSim::StartStreaming()
{
    ExposureRequest = 1.0 / Streamer->getTargetExposure();

    // Planet mode: stream pre-compressed JPEG directly (INDI_JPG format, like gphoto_ccd)
    if (m_PlanetMode)
    {
        Streamer->setPixelFormat(INDI_JPG, 8);
        Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
    }
    else
    {
        Streamer->setPixelFormat(INDI_MONO, 16);
        Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
    }

    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool CCDSim::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 0;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool CCDSim::UpdateCCDFrame(int x, int y, int w, int h)
{
    long bin_width  = w / PrimaryCCD.getBinX();
    long bin_height = h / PrimaryCCD.getBinY();

    bin_width  = bin_width - (bin_width % 2);
    bin_height = bin_height - (bin_height % 2);

    Streamer->setSize(bin_width, bin_height);

    return INDI::CCD::UpdateCCDFrame(x, y, w, h);
}

bool CCDSim::UpdateCCDBin(int hor, int ver)
{
    // Binning is not supported in planet mode (RGB data)
    if (m_PlanetMode && (hor != 1 || ver != 1))
    {
        LOG_ERROR("Binning is not supported in planet mode.");
        return false;
    }

    if (PrimaryCCD.getSubW() % hor != 0 || PrimaryCCD.getSubH() % ver != 0)
    {
        LOGF_ERROR("%dx%d binning is not supported.", hor, ver);
        return false;
    }

    uint32_t bin_width  = PrimaryCCD.getSubW() / hor;
    uint32_t bin_height = PrimaryCCD.getSubH() / ver;
    Streamer->setSize(bin_width, bin_height);

    return INDI::CCD::UpdateCCDBin(hor, ver);
}

bool CCDSim::UpdateGuiderBin(int hor, int ver)
{
    if (GuideCCD.getSubW() % hor != 0 || GuideCCD.getSubH() % ver != 0)
    {
        LOGF_ERROR("%dx%d binning is not supported.", hor, ver);
        return false;
    }

    return INDI::CCD::UpdateGuiderBin(hor, ver);
}

void * CCDSim::streamVideoHelper(void * context)
{
    return static_cast<CCDSim *>(context)->streamVideo();
}

void * CCDSim::streamVideo()
{
    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        pthread_mutex_lock(&condMutex);

        while (streamPredicate == 0)
        {
            pthread_cond_wait(&cv, &condMutex);
            ExposureRequest = Streamer->getTargetExposure();
        }

        if (terminateThread)
            break;

        // release condMutex
        pthread_mutex_unlock(&condMutex);


        // Planet mode: stream cached PNG directly (no encoding, like gphoto_ccd JPEG)
        if (m_PlanetMode)
        {
            renderPlanet(); // updates both caches if needed
            if (!m_CachedStreamBuffer.empty())
            {
                auto finish = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = finish - start;
                if (elapsed.count() < ExposureRequest)
                    usleep(fabs(ExposureRequest - elapsed.count()) * 1e6);
                Streamer->newFrame(m_CachedStreamBuffer.data(), m_CachedStreamBuffer.size());
            }
        }
        else
        {
            DrawCcdFrame(&PrimaryCCD);
            // Software binning for mono star field mode only
            if (DirectorySP[INDI_DISABLED].getState() == ISS_ON)
                PrimaryCCD.binFrame();

            auto finish = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = finish - start;
            if (elapsed.count() < ExposureRequest)
                usleep(fabs(ExposureRequest - elapsed.count()) * 1e6);

            uint32_t size = PrimaryCCD.getFrameBufferSize();
            Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);
        }

        start = std::chrono::high_resolution_clock::now();
    }

    pthread_mutex_unlock(&condMutex);
    return nullptr;
}

bool CCDSim::renderPlanet()
{
    // Get current CCD dimensions
    uint32_t width  = PrimaryCCD.getXRes();
    uint32_t height = PrimaryCCD.getYRes();

    // Compute pixel scale: arcsec per pixel = 206.265 * pixel_size_um / focal_length_mm
    float pixelSizeX = SimulatorSettingsNP[SIM_XSIZE].getValue();
    float pixelSizeY = SimulatorSettingsNP[SIM_YSIZE].getValue();
    float focalLength = ScopeInfoNP[FOCAL_LENGTH].getValue() > 0 ?
                        ScopeInfoNP[FOCAL_LENGTH].getValue() : snoopedFocalLength;
    if (focalLength <= 0)
        focalLength = 1000.0f; // default fallback
    float arcsecPerPixelX = 206.265f * pixelSizeX / focalLength;
    float arcsecPerPixelY = 206.265f * pixelSizeY / focalLength;

    // Determine planet angular diameter
    float planetDiameterArcsec = 0;
    if (m_PlanetSizeOverride > 0)
    {
        planetDiameterArcsec = m_PlanetSizeOverride;
    }
    else
    {
        auto it = PlanetAngularDiameter.find(m_SelectedPlanet);
        if (it != PlanetAngularDiameter.end())
            planetDiameterArcsec = it->second;
        else
            planetDiameterArcsec = 42.0f; // fallback
    }

    // How many sensor pixels the planet spans (use average of X and Y pixel scales)
    float planetPixelsX = planetDiameterArcsec / arcsecPerPixelX;
    float planetPixelsY = planetDiameterArcsec / arcsecPerPixelY;
    float avgPlanetPixels = (planetPixelsX + planetPixelsY) / 2.0f;

    // Render xplanet at high resolution (1024x1024) with visible black margin.
    // Use -range (camera distance in degrees) - NOT -radius which is angular FOV
    // and causes planet to fill entire frame under orthographic projection.
    // Range of 0.05° gives ~40% black border for Jupiter (~0.0117° angular diam).
    const int renderSize = 1024;
    const float planetRange = 0.05f; // degrees

    // Compute planet disk pixel radius from angular size and range
    float planetDiamDeg = planetDiameterArcsec / 3600.0f;
    float planetFracOfFrame = planetDiamDeg / planetRange;
    float srcPlanetRadius = planetFracOfFrame * renderSize / 2.0f;
    if (srcPlanetRadius < 10.0f) srcPlanetRadius = 10.0f;

    // Build xplanet command. We output PNG — same file used for both FITS rendering
    // and direct streaming (cached as-is, like gphoto_ccd sends JPEG).
    std::string searchDir;
    size_t configPos = m_XPlanetConfig.rfind("/config/");
    if (configPos != std::string::npos)
        searchDir = m_XPlanetConfig.substr(0, configPos);

    const char *pngPath = "/tmp/indi_planet.png";
    char cmd[2048];
    const bool useCustomConfig = (m_XPlanetConfig != "/usr/share/xplanet/config/default");

    if (!searchDir.empty() && useCustomConfig)
    {
        snprintf(cmd, sizeof(cmd),
                 "%s -body %s -geometry %dx%d -range %.4f "
                 "-num_times 1 -projection orthographic "
                 "-config %s -searchdir %s "
                 "-output %s 2>/dev/null",
                 m_XPlanetBinary.c_str(), m_SelectedPlanet.c_str(),
                 renderSize, renderSize, static_cast<double>(planetRange),
                 m_XPlanetConfig.c_str(), searchDir.c_str(), pngPath);
    }
    else if (!searchDir.empty())
    {
        snprintf(cmd, sizeof(cmd),
                 "%s -body %s -geometry %dx%d -range %.4f "
                 "-num_times 1 -projection orthographic "
                 "-searchdir %s "
                 "-output %s 2>/dev/null",
                 m_XPlanetBinary.c_str(), m_SelectedPlanet.c_str(),
                 renderSize, renderSize, static_cast<double>(planetRange),
                 searchDir.c_str(), pngPath);
    }
    else
    {
        snprintf(cmd, sizeof(cmd),
                 "%s -body %s -geometry %dx%d -range %.4f "
                 "-num_times 1 -projection orthographic "
                 "-output %s 2>/dev/null",
                 m_XPlanetBinary.c_str(), m_SelectedPlanet.c_str(),
                 renderSize, renderSize, static_cast<double>(planetRange), pngPath);
    }

    // Ensure buffer is sized for 3-channel 16-bit planar RGB
    uint32_t rgbBufSize = width * height * 3 * 2;
    if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(rgbBufSize))
        PrimaryCCD.setFrameBufferSize(rgbBufSize);

    // Check if cache needs invalidation before spawning any external processes
    m_LastRenderWasCacheHit = !m_CachedPlanetBuffer.empty();
    if (m_LastRenderWasCacheHit &&
            (m_CachedPlanet != m_SelectedPlanet ||
             m_CachedFocalLength != focalLength ||
             m_CachedPixelSizeX != pixelSizeX ||
             m_CachedWidth != width))
    {
        LOG_DEBUG("Cache params changed, re-rendering");
        m_CachedPlanetBuffer.clear();
        m_LastRenderWasCacheHit = false;
    }

    // Only spawn xplanet if cache is empty (fresh render needed)
    if (m_CachedPlanetBuffer.empty())
    {
        m_LastRenderWasCacheHit = false;
        LOGF_DEBUG("Running: %s", cmd);
        int ret = system(cmd);
        if (ret != 0)
        {
            LOGF_ERROR("xplanet failed with exit code %d. Is xplanet installed?", ret);
            return false;
        }

        // Generate CCD-resolution JPEG for streaming: resize planet to correct pixel size,
        // center on black background matching CCD dimensions
        {
            char jpgCmd[2048];
            int planetRenderPixels = static_cast<int>(avgPlanetPixels);
            if (planetRenderPixels < 1) planetRenderPixels = 1;
            snprintf(jpgCmd, sizeof(jpgCmd),
                     "convert %s -resize %dx%d -background black "
                     "-gravity center -extent %dx%d -quality 90 /tmp/indi_planet.jpg 2>/dev/null",
                     pngPath, planetRenderPixels, planetRenderPixels,
                     static_cast<int>(width), static_cast<int>(height));
            ret = system(jpgCmd);
            if (ret != 0)
            {
                LOG_ERROR("Failed to generate CCD-resolution JPEG. Is ImageMagick installed?");
                return false;
            }
        }

        // Cache the CCD-resolution JPEG for direct streaming
        FILE *jpgFile = fopen("/tmp/indi_planet.jpg", "rb");
        if (jpgFile)
        {
            fseek(jpgFile, 0, SEEK_END);
            long jpgSize = ftell(jpgFile);
            fseek(jpgFile, 0, SEEK_SET);
            m_CachedStreamBuffer.resize(jpgSize);
            size_t bytesRead = fread(m_CachedStreamBuffer.data(), 1, jpgSize, jpgFile);
            INDI_UNUSED(bytesRead);
            fclose(jpgFile);
        }

        ret = system("convert /tmp/indi_planet.png /tmp/indi_planet.ppm 2>/dev/null");
        if (ret != 0)
        {
            LOG_ERROR("Failed to convert PNG to PPM. Is ImageMagick installed?");
            return false;
        }

        // Parse the PPM file
        FILE *fp = fopen("/tmp/indi_planet.ppm", "rb");
        if (!fp)
        {
            LOG_ERROR("Failed to open /tmp/indi_planet.ppm");
            return false;
        }

        char magic[3];
        if (fscanf(fp, "%2s\n", magic) != 1 || strcmp(magic, "P6") != 0)
        {
            LOG_ERROR("PPM file is not P6 format (binary RGB)");
            fclose(fp);
            return false;
        }

        int ppmWidth, ppmHeight, maxVal;
        // Skip comments
        int c = fgetc(fp);
        while (c == '#')
        {
            while (fgetc(fp) != '\n');
            c = fgetc(fp);
        }
        ungetc(c, fp);
        if (fscanf(fp, "%d %d\n%d\n", &ppmWidth, &ppmHeight, &maxVal) != 3)
        {
            LOG_ERROR("Failed to parse PPM header");
            fclose(fp);
            return false;
        }

        std::vector<uint8_t> rgbData(ppmWidth * ppmHeight * 3);
        size_t bytesRead = fread(rgbData.data(), 1, rgbData.size(), fp);
        fclose(fp);

        if (bytesRead != rgbData.size())
        {
            LOGF_ERROR("PPM read error: expected %zu bytes, got %zu", rgbData.size(), bytesRead);
            return false;
        }

        // Allocate cached planet buffer at CCD resolution (16-bit RGB, 3-channel PLANAR layout for FITS)
        m_CachedPlanetBuffer.resize(rgbBufSize);
        m_CachedPlanetWidth = width;
        m_CachedPlanetHeight = height;
        uint16_t *buffer = reinterpret_cast<uint16_t *>(m_CachedPlanetBuffer.data());

        // Clear to black
        memset(buffer, 0, rgbBufSize);

        // Plane offsets within buffer (FITS NAXIS=3 planar: R_plane | G_plane | B_plane)
        size_t planeSize = static_cast<size_t>(width) * static_cast<size_t>(height);
        uint16_t *rPlane = buffer;
        uint16_t *gPlane = buffer + planeSize;
        uint16_t *bPlane = buffer + planeSize * 2;

        // Center the planet in the CCD frame
        float centerX = width / 2.0f;
        float centerY = height / 2.0f;

        // Scale factor from xplanet render to planet pixel size
        float dstPlanetRadius = avgPlanetPixels / 2.0f;
        if (dstPlanetRadius < 1.0f)
            dstPlanetRadius = 1.0f;
        float scale = srcPlanetRadius / dstPlanetRadius;

        // Map each CCD pixel to the source PPM, writing planar 16-bit data.
        // Use simple linear scaling (8-bit × 256 → 16-bit) so the planet's
        // 8-bit perceptual brightness maps to the upper 16-bit range, making
        // auto-stretch work naturally without crushing mid-tones.
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                float sx = (x - centerX) * scale + ppmWidth / 2.0f;
                float sy = (y - centerY) * scale + ppmHeight / 2.0f;

                float dx = (sx - ppmWidth / 2.0f);
                float dy = (sy - ppmHeight / 2.0f);
                if (sqrtf(dx * dx + dy * dy) > srcPlanetRadius)
                    continue;

                int ix = static_cast<int>(sx);
                int iy = static_cast<int>(sy);
                if (ix < 0 || ix >= ppmWidth || iy < 0 || iy >= ppmHeight)
                    continue;

                int srcIdx = (iy * ppmWidth + ix) * 3;
                uint8_t r8 = rgbData[srcIdx];
                uint8_t g8 = rgbData[srcIdx + 1];
                uint8_t b8 = rgbData[srcIdx + 2];

                // Linear 8→16-bit scaling: perceptual brightness preserved
                uint16_t r16 = static_cast<uint16_t>(r8) * 256;
                uint16_t g16 = static_cast<uint16_t>(g8) * 256;
                uint16_t b16 = static_cast<uint16_t>(b8) * 256;

                if (r16 > static_cast<uint32_t>(m_MaxVal)) r16 = m_MaxVal;
                if (g16 > static_cast<uint32_t>(m_MaxVal)) g16 = m_MaxVal;
                if (b16 > static_cast<uint32_t>(m_MaxVal)) b16 = m_MaxVal;

                size_t px = y * width + x;
                rPlane[px] = r16;
                gPlane[px] = g16;
                bPlane[px] = b16;
            }
        }

        // Store FOV params so we can detect changes on cache check
        m_CachedPlanet = m_SelectedPlanet;
        m_CachedFocalLength = focalLength;
        m_CachedPixelSizeX = pixelSizeX;
        m_CachedWidth = width;
    }
    else
    {
        // Cache hit: PPM already parsed and scaled, just copy to frame buffer
        // (NAXIS and buffer size already set by SetCaptureFormat when planet mode enabled)
    }

    // Copy the cached buffer into PrimaryCCD's frame buffer
    size_t copySize = std::min(m_CachedPlanetBuffer.size(), static_cast<size_t>(rgbBufSize));
    memcpy(PrimaryCCD.getFrameBuffer(), m_CachedPlanetBuffer.data(), copySize);

    time(&m_LastPlanetRenderTime);
    // Only log on fresh render, not cache hits
    if (!m_LastRenderWasCacheHit)
    {
        LOGF_INFO("Rendered planet %s: diameter=%.1f arcsec, pixels=%.1f, scale=%.1f arcsec/px",
                  m_SelectedPlanet.c_str(), planetDiameterArcsec, planetPixelsX, arcsecPerPixelX);
    }

    return true;
}

void CCDSim::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeyword);

    fitsKeyword.push_back({"GAIN", GainNP[0].getValue(), 3, "Gain"});

    if (m_PlanetMode)
    {
        fitsKeyword.emplace_back("OBJECT", m_SelectedPlanet.c_str(), "Target planet");
        fitsKeyword.emplace_back("XPLANET", static_cast<int64_t>(1), "Generated by xplanet");
        fitsKeyword.emplace_back("COLORSPC", "RGB", "Color space");
    }
}

bool CCDSim::loadNextImage()
{
    if (m_RemainingFiles.empty())
        m_RemainingFiles = m_AllFiles;
    const std::string filename = m_RemainingFiles[0];
    m_RemainingFiles.pop_front();
    char comment[512] = {0}, bayer_pattern[16] = {0};
    int status = 0, anynull = 0;
    double pixel_size = 5.2;
    int ndim {2}, bitpix {8};
    long naxes[3];
    fitsfile *fptr = nullptr;

    if (fits_open_diskfile(&fptr, filename.c_str(), READONLY, &status))
    {
        char error_status[512] = {0};
        fits_get_errstatus(status, error_status);
        LOGF_WARN("Error opening file %s due to error %s", filename.c_str(), error_status);
        return false;
    }

    fits_get_img_param(fptr, 3, &bitpix, &ndim, naxes, &status);

    if (ndim < 3)
        naxes[2] = 1;
    else
        PrimaryCCD.setNAxis(3);
    int samples_per_channel = naxes[0] * naxes[1];
    int channels = naxes[2];
    int elements = samples_per_channel * channels;
    int size =  elements * bitpix / 8;
    PrimaryCCD.setFrameBufferSize(size);

    if (fits_read_img(fptr, bitpix == 8 ? TBYTE : TUSHORT, 1, elements, 0, PrimaryCCD.getFrameBuffer(), &anynull, &status))
    {
        char error_status[512] = {0};
        fits_get_errstatus(status, error_status);
        LOGF_WARN("Error reading file %s due to error %s", filename.c_str(), error_status);
        return false;
    }

    if (fits_read_key_dbl(fptr, "PIXSIZE1", &pixel_size, comment, &status))
    {
        char error_status[512] = {0};
        fits_get_errstatus(status, error_status);
        LOGF_WARN("Error reading file %s due to error %s", filename.c_str(), error_status);
        return false;
    }

    if (fits_read_key_str(fptr, "BAYERPAT", bayer_pattern, comment, &status))
    {
        char error_status[512] = {0};
        fits_get_errstatus(status, error_status);
        LOGF_DEBUG("No BAYERPAT keyword found in %s (%s)", filename.c_str(), error_status);
    }
    SetCCDParams(naxes[0], naxes[1], bitpix, pixel_size, pixel_size);

    // Check if MONO or Bayer
    if (channels == 1 && strlen(bayer_pattern)  == 4)
    {
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        BayerTP[CFA_OFFSET_X].setText("0");
        BayerTP[CFA_OFFSET_Y].setText("0");
        BayerTP[CFA_TYPE].setText(bayer_pattern);
    }
    else
    {
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }

    fits_close_file(fptr, &status);
    return true;
}

bool CCDSim::SetCaptureFormat(uint8_t index)
{
    // Get the selected format
    if (index >= m_CaptureFormats.size())
        return false;

    const auto &format = m_CaptureFormats[index];
    bool isRGB = (format.name == "INDI_RGB");

    // Set NAXIS and buffer size for RGB vs Mono
    // The base CCD class handles CaptureFormatSP state, we just need to configure the chip
    uint32_t nbuf;
    if (isRGB)
    {
        PrimaryCCD.setNAxis(3);
        nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3 * (PrimaryCCD.getBPP() / 8);
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }
    else
    {
        PrimaryCCD.setNAxis(2);
        nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * (PrimaryCCD.getBPP() / 8);
        setBayerEnabled(m_SimulateBayer);
    }
    PrimaryCCD.setFrameBufferSize(nbuf);
    Streamer->setPixelFormat(isRGB ? INDI_RGB : INDI_MONO, 16);
    m_IsRGBFormat = isRGB;
    return true;
}
