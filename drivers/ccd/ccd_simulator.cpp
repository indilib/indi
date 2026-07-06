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

    CaptureFormat format = {"INDI_MONO", "Mono", 16, true};
    addCaptureFormat(format);

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

int CCDSim::SetTemperature(double temperature)
{
    TemperatureRequest = temperature;
    if (std::abs(temperature - TemperatureNP[0].getValue()) < 0.1)
    {
        TemperatureNP[0].setValue(temperature);
        return 1;
    }

    auto isCooling = TemperatureRequest < temperature;
    CoolerSP[INDI_ENABLED].setState(isCooling ? ISS_ON : ISS_OFF);
    CoolerSP[INDI_DISABLED].setState(isCooling ? ISS_OFF : ISS_ON);
    CoolerSP.setState(isCooling ? IPS_BUSY : IPS_IDLE);
    CoolerSP.apply();
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
    //  Leave the proper time showing for the draw routines
    if (PrimaryCCD.getFrameType() == INDI::CCDChip::LIGHT_FRAME && DirectorySP[INDI_ENABLED].getState() == ISS_ON)
    {
        if (loadNextImage() == false)
            return false;
    }
    else
        DrawCcdFrame(&PrimaryCCD);
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
                    // We don't bin for raw images.
                    if (DirectorySP[INDI_DISABLED].getState() == ISS_ON)
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

    if (TemperatureNP.getState() == IPS_BUSY)
    {
        if (TemperatureRequest < TemperatureNP[0].getValue())
            TemperatureNP[0].setValue(std::max(TemperatureRequest, TemperatureNP[0].getValue() - 0.5));
        else
            TemperatureNP[0].setValue(std::min(TemperatureRequest, TemperatureNP[0].getValue() + 0.5));

        if (std::abs(TemperatureNP[0].getValue() - m_LastTemperature) > 0.1)
        {
            m_LastTemperature = TemperatureNP[0].getValue();
            TemperatureNP.apply();
        }

        // Above 20, cooler is off
        if (TemperatureNP[0].getValue() >= 20)
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
                m_TargetTemperature = 20;
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
            }
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


        // 16 bit
        DrawCcdFrame(&PrimaryCCD);

        PrimaryCCD.binFrame();

        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;

        if (elapsed.count() < ExposureRequest)
            usleep(fabs(ExposureRequest - elapsed.count()) * 1e6);

        uint32_t size = PrimaryCCD.getFrameBufferSize() / (PrimaryCCD.getBinX() * PrimaryCCD.getBinY());
        Streamer->newFrame(PrimaryCCD.getFrameBuffer(), size);

        start = std::chrono::high_resolution_clock::now();
    }

    pthread_mutex_unlock(&condMutex);
    return nullptr;
}

void CCDSim::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeyword)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeyword);

    fitsKeyword.push_back({"GAIN", GainNP[0].getValue(), 3, "Gain"});
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
    INDI_UNUSED(index);
    return true;
}
