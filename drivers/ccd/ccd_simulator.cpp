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

#include "locale_compat.h"

#include <libnova/julian_day.h>
#include <libastro.h>

#include <cmath>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

static std::unique_ptr<CCDSim> ccdsim(new CCDSim());

CCDSim::CCDSim() : INDI::FilterInterface(this)
{
    currentRA  = RA;
    currentDE = Dec;

    terminateThread = false;

    //  focal length of the telescope in millimeters
    primaryFocalLength = 900;
    guiderFocalLength  = 300;

    time(&RunStart);

    // Filter stuff
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 8;
}

bool CCDSim::setupParameters()
{
    SetCCDParams(SimulatorSettingsN[SIM_XRES].value,
                 SimulatorSettingsN[SIM_YRES].value,
                 16,
                 SimulatorSettingsN[SIM_XSIZE].value,
                 SimulatorSettingsN[SIM_YSIZE].value);

    m_MaxNoise      = SimulatorSettingsN[SIM_NOISE].value;
    m_SkyGlow       = SimulatorSettingsN[SIM_SKYGLOW].value;
    m_MaxVal        = SimulatorSettingsN[SIM_MAXVAL].value;
    m_Bias          = OffsetN[0].value;
    m_LimitingMag   = SimulatorSettingsN[SIM_LIMITINGMAG].value;
    m_SaturationMag = SimulatorSettingsN[SIM_SATURATION].value;
    //  An oag is offset this much from center of scope position (arcminutes);
    m_OAGOffset = SimulatorSettingsN[SIM_OAGOFFSET].value;
    m_PolarError = SimulatorSettingsN[SIM_POLAR].value;
    m_PolarDrift = SimulatorSettingsN[SIM_POLARDRIFT].value;
    m_PEPeriod = SimulatorSettingsN[SIM_PE_PERIOD].value;
    m_PEMax = SimulatorSettingsN[SIM_PE_MAX].value;
    m_TimeFactor = SimulatorSettingsN[SIM_TIME_FACTOR].value;
    RotatorAngle = SimulatorSettingsN[SIM_ROTATION].value;

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

    IUFillNumber(&SimulatorSettingsN[SIM_XRES], "SIM_XRES", "CCD X resolution", "%4.0f", 512, 8192, 512, 1280);
    IUFillNumber(&SimulatorSettingsN[SIM_YRES], "SIM_YRES", "CCD Y resolution", "%4.0f", 512, 8192, 512, 1024);
    IUFillNumber(&SimulatorSettingsN[SIM_XSIZE], "SIM_XSIZE", "CCD X Pixel Size", "%4.2f", 1, 30, 5, 5.2);
    IUFillNumber(&SimulatorSettingsN[SIM_YSIZE], "SIM_YSIZE", "CCD Y Pixel Size", "%4.2f", 1, 30, 5, 5.2);
    IUFillNumber(&SimulatorSettingsN[SIM_MAXVAL], "SIM_MAXVAL", "CCD Maximum ADU", "%4.0f", 255, 65000, 1000, 65000);
    IUFillNumber(&SimulatorSettingsN[SIM_SATURATION], "SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 1, 1.0);
    IUFillNumber(&SimulatorSettingsN[SIM_LIMITINGMAG], "SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 1, 17.0);
    IUFillNumber(&SimulatorSettingsN[SIM_NOISE], "SIM_NOISE", "CCD Noise", "%4.0f", 0, 6000, 500, 10);
    IUFillNumber(&SimulatorSettingsN[SIM_SKYGLOW], "SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 500, 19.5);
    IUFillNumber(&SimulatorSettingsN[SIM_OAGOFFSET], "SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 500, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_POLAR], "SIM_POLAR", "PAE (arcminutes)", "%4.1f", -600, 600, 100, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_POLARDRIFT], "SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.1f", 0, 60, 5, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_PE_PERIOD], "SIM_PEPERIOD", "PE Period (minutes)", "%4.1f", 0, 60, 5, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_PE_MAX], "SIM_PEMAX", "PE Max (arcsec)", "%4.1f", 0, 6000, 500, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_TIME_FACTOR], "SIM_TIME_FACTOR", "Time Factor (x)", "%.2f", 0.01, 100, 10, 1);
    IUFillNumber(&SimulatorSettingsN[SIM_ROTATION], "SIM_ROTATION", "CCD Rotation", "%.2f", 0, 360, 10, 0);

    IUFillNumberVector(&SimulatorSettingsNP, SimulatorSettingsN, SIM_N, getDeviceName(), "SIMULATOR_SETTINGS",
                       "Settings", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);

    // RGB Simulation
    IUFillSwitch(&SimulateBayerS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&SimulateBayerS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&SimulateBayerSP, SimulateBayerS, 2, getDeviceName(), "SIMULATE_BAYER", "Bayer", SIMULATOR_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Simulate focusing
    IUFillNumber(&FocusSimulationN[0], "SIM_FOCUS_POSITION", "Focus", "%.f", 0.0, 100000.0, 1.0, 36700.0);
    IUFillNumber(&FocusSimulationN[1], "SIM_FOCUS_MAX", "Max. Position", "%.f", 0.0, 100000.0, 1.0, 100000.0);
    IUFillNumber(&FocusSimulationN[2], "SIM_SEEING", "Seeing (arcsec)", "%4.2f", 0, 60, 0, 3.5);
    IUFillNumberVector(&FocusSimulationNP, FocusSimulationN, 3, getDeviceName(), "SIM_FOCUSING", "Focus Simulation",
                       SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);

    // Simulate Crash
    IUFillSwitch(&CrashS[0], "CRASH", "Crash driver", ISS_OFF);
    IUFillSwitchVector(&CrashSP, CrashS, 1, getDeviceName(), "CCD_SIMULATE_CRASH", "Crash", SIMULATOR_TAB, IP_WO,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Periodic Error
    IUFillNumber(&EqPEN[AXIS_RA], "RA_PE", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqPEN[AXIS_DE], "DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqPENP, EqPEN, 2, getDeviceName(), "EQUATORIAL_PE", "EQ PE", SIMULATOR_TAB, IP_RW, 60,
                       IPS_IDLE);

    // FWHM
    IUFillNumber(&FWHMN[0], "SIM_FWHM", "FWHM (arcseconds)", "%4.2f", 0, 60, 0, 7.5);
    IUFillNumberVector(&FWHMNP, FWHMN, 1, ActiveDeviceT[ACTIVE_FOCUSER].text, "FWHM", "FWHM", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    // Cooler
    IUFillSwitch(&CoolerS[INDI_ENABLED], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[INDI_DISABLED], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Gain
    IUFillNumber(&GainN[0], "GAIN", "value", "%.f", 0, 100, 10, 90);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Offset
    IUFillNumber(&OffsetN[0], "OFFSET", "value", "%.f", 0, 6000, 500, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Directory to read images from. This is useful to test real images captured by camera
    // For each capture, one file is read (sorted by name) and is sent to client.
    IUFillText(&DirectoryT[0], "LOCATION", "Location", getenv("HOME"));
    IUFillTextVector(&DirectoryTP, DirectoryT, 1, getDeviceName(), "CCD_DIRECTORY_LOCATION", "Directory", SIMULATOR_TAB, IP_RW,
                     60, IPS_IDLE);

    // Toggle Directory Reading. If enabled. The simulator will just read images from the directory and not generate them.
    IUFillSwitch(&DirectoryS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&DirectoryS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&DirectorySP, DirectoryS, 2, getDeviceName(), "CCD_DIRECTORY_TOGGLE", "Use Dir.", SIMULATOR_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_PE");
#else
    IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "EQUATORIAL_EOD_COORD");
#endif


    IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "FWHM");

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

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    // This should be called after the initial SetCCDCapability (above)
    // as it modifies the capabilities.
    setBayerEnabled(m_SimulateBayer);

    INDI::FilterInterface::initProperties(FILTER_TAB);

    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 8;

    addDebugControl();

    setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

    // Make Guide Scope ON by default
    TelescopeTypeS[TELESCOPE_PRIMARY].s = ISS_OFF;
    TelescopeTypeS[TELESCOPE_GUIDE].s = ISS_ON;

    return true;
}

void CCDSim::setBayerEnabled(bool onOff)
{
    if (onOff)
    {
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "0");
        IUSaveText(&BayerT[2], "RGGB");
    }
    else
    {
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }
}

void CCDSim::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineProperty(&SimulatorSettingsNP);
    defineProperty(&EqPENP);
    defineProperty(&FocusSimulationNP);
    defineProperty(&SimulateBayerSP);
    defineProperty(&CrashSP);
}

bool CCDSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
            defineProperty(&CoolerSP);

        defineProperty(&GainNP);
        defineProperty(&OffsetNP);

        defineProperty(&DirectoryTP);
        defineProperty(&DirectorySP);

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
            deleteProperty(CoolerSP.name);

        deleteProperty(GainNP.name);
        deleteProperty(OffsetNP.name);
        deleteProperty(DirectoryTP.name);
        deleteProperty(DirectorySP.name);

        INDI::FilterInterface::updateProperties();
    }

    return true;
}

int CCDSim::SetTemperature(double temperature)
{
    TemperatureRequest = temperature;
    if (fabs(temperature - TemperatureN[0].value) < 0.1)
    {
        TemperatureN[0].value = temperature;
        return 1;
    }

    CoolerS[0].s = ISS_ON;
    CoolerS[1].s = ISS_OFF;
    CoolerSP.s   = IPS_BUSY;
    IDSetSwitch(&CoolerSP, nullptr);
    return 0;
}

bool CCDSim::StartExposure(float duration)
{
    if (std::isnan(RA) && std::isnan(Dec))
    {
        LOG_ERROR("Telescope coordinates missing. Make sure telescope is connected and its name is set in CCD Options.");
        return false;
    }

    //  for the simulator, we can just draw the frame now
    //  and it will get returned at the right time
    //  by the timer routines
    AbortPrimaryFrame = false;
    ExposureRequest   = duration;

    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart, nullptr);
    //  Leave the proper time showing for the draw routines
    if (DirectoryS[INDI_ENABLED].s == ISS_ON)
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

float CCDSim::CalcTimeLeft(timeval start, float req)
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

void CCDSim::TimerHit()
{
    uint32_t nextTimer = getCurrentPollingPeriod();

    //  No need to reset timer if we are not connected anymore
    if (!isConnected())
        return;

    if (InExposure)
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

            //IDLog("CCD Exposure left: %g - Requset: %g\n", timeleft, ExposureRequest);
            if (timeleft < 0)
                timeleft = 0;

            PrimaryCCD.setExposureLeft(timeleft);

            if (timeleft < 1.0)
            {
                if (timeleft <= 0.001)
                {
                    InExposure = false;
                    // We don't bin for raw images.
                    if (DirectoryS[INDI_DISABLED].s == ISS_ON)
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

    if (TemperatureNP.s == IPS_BUSY)
    {
        if (TemperatureRequest < TemperatureN[0].value)
            TemperatureN[0].value = std::max(TemperatureRequest, TemperatureN[0].value - 0.5);
        else
            TemperatureN[0].value = std::min(TemperatureRequest, TemperatureN[0].value + 0.5);

        if (std::abs(TemperatureN[0].value - m_LastTemperature) > 0.1)
        {
            m_LastTemperature = TemperatureN[0].value;
            IDSetNumber(&TemperatureNP, nullptr);
        }

        // Above 20, cooler is off
        if (TemperatureN[0].value >= 20)
        {
            CoolerS[0].s = ISS_OFF;
            CoolerS[1].s = ISS_ON;
            CoolerSP.s   = IPS_IDLE;
            IDSetSwitch(&CoolerSP, nullptr);
        }
    }


    SetTimer(nextTimer);
}

double CCDSim::flux(double mag) const
{
    // The limiting magnitude provides zero ADU whatever the exposure
    // The saturation magnitude provides max ADU in one second
    double const z = m_LimitingMag;
    double const k = 2.5 * log10(m_MaxVal) / (m_LimitingMag - m_SaturationMag);
    return pow(10, (z - mag) * k / 2.5);
}

int CCDSim::DrawCcdFrame(INDI::CCDChip * targetChip)
{
    //  CCD frame is 16 bit data
    float exposure_time;
    float targetFocalLength;

    uint16_t * ptr = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

    if (targetChip->getXRes() == 500)
        exposure_time = GuideExposureRequest * 4;
    else if (Streamer->isStreaming())
        exposure_time = (ExposureRequest < 1) ? (ExposureRequest * 100) : ExposureRequest * 2;
    else
        exposure_time = ExposureRequest;

    if (GainN[0].value > 50)
        exposure_time *= sqrt(GainN[0].value - 50);
    else if (GainN[0].value < 50)
        exposure_time /= sqrt(50 - GainN[0].value);

    if (TelescopeTypeS[TELESCOPE_PRIMARY].s == ISS_ON)
        targetFocalLength = primaryFocalLength;
    else
        targetFocalLength = guiderFocalLength;

    if (ShowStarField)
    {
        float PEOffset {0};
        float decDrift {0};
        //  telescope ra in degrees
        double rad {0};
        //  telescope ra in radians
        double rar {0};
        //  telescope dec in radians;
        double decr {0};
        int nwidth = 0, nheight = 0;

        if (m_PEPeriod > 0)
        {
            double timesince;
            time_t now;
            time(&now);

            //  Lets figure out where we are on the pe curve
            timesince = difftime(now, RunStart);
            //  This is our spot in the curve
            double PESpot = timesince / m_PEPeriod;
            //  Now convert to radians
            PESpot = PESpot * 2.0 * 3.14159;

            PEOffset = m_PEMax * std::sin(PESpot);
            //  convert to degrees
            PEOffset = PEOffset / 3600;
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

        double theta = 270;
        if (!std::isnan(RotatorAngle))
            theta += RotatorAngle;
        if (pierSide == 1)
            theta -= 180;       // rotate 180 if on East
        theta = range360(theta);

        // JM: 2015-03-17: Next we do a rotation assuming CW for angle theta
        pa = pprx * cos(theta * M_PI / 180.0);
        pb = ppry * sin(theta * M_PI / 180.0);

        pd = pprx * -sin(theta * M_PI / 180.0);
        pe = ppry * cos(theta * M_PI / 180.0);

        nwidth = targetChip->getXRes();
        pc     = nwidth / 2;

        nheight = targetChip->getYRes();
        pf      = nheight / 2;

        ImageScalex = Scalex;
        ImageScaley = Scaley;

#ifdef USE_EQUATORIAL_PE
        if (!usePE)
        {
#endif
            currentRA  = RA;
            currentDE = Dec;

            INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };

            double jd = ln_get_julian_from_sys();

            epochPos.rightascension  = currentRA;
            epochPos.declination = currentDE;

            // Convert from JNow to J2000
            INDI::ObservedToJ2000(&epochPos, jd, &J2000Pos);

            currentRA  = J2000Pos.rightascension;
            currentDE = J2000Pos.declination;

            //LOGF_DEBUG("DrawCcdFrame JNow %f, %f J2000 %f, %f", epochPos.ra, epochPos.dec, J2000Pos.ra, J2000Pos.dec);
            //INDI::IEquatorialCoordinates jnpos;
            //INDI::J2000toObserved(&J2000Pos, jd, &jnpos);
            //LOGF_DEBUG("J2000toObserved JNow %f, %f J2000 %f, %f", jnpos.ra, jnpos.dec, J2000Pos.ra, J2000Pos.dec);

            currentDE += guideNSOffset;
            currentRA += guideWEOffset;
#ifdef USE_EQUATORIAL_PE
        }
#endif

        //  calc this now, we will use it a lot later
        rad = currentRA * 15.0;
        rar = rad * 0.0174532925;
        //  offsetting the dec by the guide head offset
        float cameradec;
        cameradec = currentDE + m_OAGOffset / 60;
        decr      = cameradec * 0.0174532925;

        decDrift = (m_PolarDrift * m_PolarError * cos(decr)) / 3.81;

        // Add declination drift, if any.
        decr += decDrift / 3600.0 * 0.0174532925;

        //  now lets calculate the radius we need to fetch
        double radius = sqrt((Scalex * Scalex * targetChip->getXRes() / 2.0 * targetChip->getXRes() / 2.0) +
                             (Scaley * Scaley * targetChip->getYRes() / 2.0 * targetChip->getYRes() / 2.0));
        //  we have radius in arcseconds now
        //  convert to arcminutes
        radius = radius / 60;
#if 0
        LOGF_DEBUG("Lookup radius %4.2f", radius);
#endif

        //  A saturationmag star saturates in one second
        //  and a limitingmag produces a one adu level in one second
        //  solve for zero point and system gain

        //k = (m_SaturationMag - m_LimitingMag) / ((-2.5 * log(m_MaxVal)) - (-2.5 * log(1.0 / 2.0)));
        //z = m_SaturationMag - k * (-2.5 * log(m_MaxVal));

        //  Should probably do some math here to figure out the dimmest
        //  star we can see on this exposure
        //  and only fetch to that magnitude
        //  for now, just use the limiting mag number with some room to spare
        double lookuplimit = m_LimitingMag;

        if (radius > 60)
            lookuplimit = 11;

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
                    range360(rad + PEOffset),
                    rangeDec(cameradec),
                    radius,
                    lookuplimit);

            pp = popen(gsccmd, "r");
            if (pp != nullptr)
            {
                char line[256];
                int stars = 0;
                int lines = 0;

                while (fgets(line, 256, pp) != nullptr)
                {
                    //  ok, lets parse this line for specifcs we want
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
                        lines++;
                        stars++;

                        //  Convert the ra/dec to standard co-ordinates
                        double sx;    //  standard co-ords
                        double sy;    //
                        double srar;  //  star ra in radians
                        double sdecr; //  star dec in radians;
                        double ccdx;
                        double ccdy;

                        srar  = ra * 0.0174532925;
                        sdecr = dec * 0.0174532925;

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

                        // Invert horizontally
                        ccdx = ccdW - ccdx;

                        rc = DrawImageStar(targetChip, mag, ccdx, ccdy, exposure_time);
                        drawn += rc;
#ifdef __DEV__
                        if (rc == 1)
                        {
                            LOGF_DEBUG("star %s scope %6.4f %6.4f star %6.4f %6.4f ccd %6.2f %6.2f", id, rad, decPE, ra, dec, ccdx, ccdy);
                            LOGF_DEBUG("star %s ccd %6.2f %6.2f", id, ccdx, ccdy);
                        }
#endif
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

        //  now we need to add background sky glow, with vignetting
        //  this is essentially the same math as drawing a dim star with
        //  fwhm equivalent to the full field of view

        if (ftype == INDI::CCDChip::LIGHT_FRAME || ftype == INDI::CCDChip::FLAT_FRAME)
        {
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

            // Flux represents one second, scale up linearly for exposure time
            float const skyflux = flux(glow) * exposure_time;

            uint16_t * pt = reinterpret_cast<uint16_t *>(targetChip->getFrameBuffer());

            nheight = targetChip->getSubH();
            nwidth  = targetChip->getSubW();

            for (int y = 0; y < nheight; y++)
            {
                float const sy = nheight / 2 - y;

                for (int x = 0; x < nwidth; x++)
                {
                    float const sx = nwidth / 2 - x;

                    // Vignetting parameter in arcsec
                    float const vig = std::min(nwidth, nheight) * ImageScalex;

                    // Squared distance to center in arcsec (need to make this account for actual pixel size)
                    float const dc2 = sx * sx * ImageScalex * ImageScalex + sy * sy * ImageScaley * ImageScaley;

                    // Gaussian falloff to the edges of the frame
                    float const fa = exp(-2.0 * 0.7 * dc2 / (vig * vig));

                    // Get the current value of the pixel, add the sky glow and scale for vignetting
                    float fp = (pt[0] + skyflux) * fa;

                    // Clamp to limits, store minmax
                    if (fp > m_MaxVal) fp = m_MaxVal;
                    if (fp < pt[0]) fp = pt[0];
                    if (fp > maxpix) maxpix = fp;
                    if (fp < minpix) minpix = fp;

                    // And put it back
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

                    AddToPixel(targetChip, x, y, m_Bias + noise);
                }
            }
        }
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

int CCDSim::DrawImageStar(INDI::CCDChip * targetChip, float mag, float x, float y, float exposure_time)
{
    int sx, sy;
    int drew     = 0;
    //int boxsizex = 5;
    int boxsizey = 5;
    float flux;

    int subX = targetChip->getSubX();
    int subY = targetChip->getSubY();
    int subW = targetChip->getSubW() + subX;
    int subH = targetChip->getSubH() + subY;

    if ((x < subX) || (x > subW || (y < subY) || (y > subH)))
    {
        //  this star is not on the ccd frame anyways
        return 0;
    }

    //  calculate flux from our zero point and gain values
    flux = this->flux(mag);

    //  ok, flux represents one second now
    //  scale up linearly for exposure time
    flux = flux * exposure_time;

    float qx;
    //  we need a box size that gives a radius at least 3 times fwhm
    qx       = seeing / ImageScalex;
    qx       = qx * 3;
    //boxsizex = (int)qx;
    //boxsizex++;
    qx       = seeing / ImageScaley;
    qx       = qx * 3;
    boxsizey = static_cast<int>(qx);
    boxsizey++;

    //IDLog("BoxSize %d %d\n",boxsizex,boxsizey);

    for (sy = -boxsizey; sy <= boxsizey; sy++)
    {
        for (sx = -boxsizey; sx <= boxsizey; sx++)
        {
            int rc;

            // Squared distance to center in arcsec (need to make this account for actual pixel size)
            float const dc2 = sx * sx * ImageScalex * ImageScalex + sy * sy * ImageScaley * ImageScaley;

            // Use a gaussian of unitary integral, scale it with the source flux
            // f(x) = 1/(sqrt(2*pi)*sigma) * exp( -x² / (2*sigma²) )
            // FWHM = 2*sqrt(2*log(2))*sigma => sigma = seeing/(2*sqrt(2*log(2)))
            float const sigma = seeing / ( 2 * sqrt(2 * log(2)));
            float const fa = 1 / (sigma * sqrt(2 * 3.1416)) * exp( -dc2 / (2 * sigma * sigma));

            // The source contribution is the gaussian value, stretched by seeing/FWHM
            float fp = fa * flux;

            if (fp < 0)
                fp = 0;

            rc = AddToPixel(targetChip, x + sx, y + sy, fp);
            if (rc != 0)
                drew = 1;
        }
    }
    return drew;
}

int CCDSim::AddToPixel(INDI::CCDChip * targetChip, int x, int y, int val)
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
                    if (newval > maxpix)
                        maxpix = newval;
                    if (newval < minpix)
                        minpix = newval;
                    pt[0] = newval;
                }
            }
        }
    }
    return drew;
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
        if (strcmp(name, FilterNameTP->name) == 0)
        {
            INDI::FilterInterface::processText(dev, name, texts, names, n);
            return true;
        }
        else if (!strcmp(DirectoryTP.name, name))
        {
            IUUpdateText(&DirectoryTP, texts, names, n);
            DirectoryTP.s = IPS_OK;
            IDSetText(&DirectoryTP, nullptr);
            return true;
        }

    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool CCDSim::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (!strcmp(name, GainNP.name))
        {
            IUUpdateNumber(&GainNP, values, names, n);
            GainNP.s = IPS_OK;
            IDSetNumber(&GainNP, nullptr);
            return true;
        }
        if (!strcmp(name, OffsetNP.name))
        {
            IUUpdateNumber(&OffsetNP, values, names, n);
            OffsetNP.s = IPS_OK;
            IDSetNumber(&OffsetNP, nullptr);
            m_Bias = OffsetN[0].value;
            return true;
        }
        else if (!strcmp(name, SimulatorSettingsNP.name))
        {
            IUUpdateNumber(&SimulatorSettingsNP, values, names, n);
            SimulatorSettingsNP.s = IPS_OK;

            //  Reset our parameters now
            setupParameters();
            IDSetNumber(&SimulatorSettingsNP, nullptr);
            return true;
        }
        // Record PE EQ to simulate different position in the sky than actual mount coordinate
        // This can be useful to simulate Periodic Error or cone error or any arbitrary error.
        else if (!strcmp(name, EqPENP.name))
        {
            IUUpdateNumber(&EqPENP, values, names, n);
            EqPENP.s = IPS_OK;

            INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
            epochPos.rightascension  = EqPEN[AXIS_RA].value;
            epochPos.declination = EqPEN[AXIS_DE].value;

            RA = EqPEN[AXIS_RA].value;
            Dec = EqPEN[AXIS_DE].value;

            INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            currentRA  = J2000Pos.rightascension;
            currentDE = J2000Pos.declination;
            usePE = true;

            IDSetNumber(&EqPENP, nullptr);
            return true;
        }
        else if (!strcmp(name, FilterSlotNP.name))
        {
            INDI::FilterInterface::processNumber(dev, name, values, names, n);
            return true;
        }
        else if (!strcmp(name, FocusSimulationNP.name))
        {
            // update focus simulation parameters
            IUUpdateNumber(&FocusSimulationNP, values, names, n);
            FocusSimulationNP.s = IPS_OK;
            IDSetNumber(&FocusSimulationNP, nullptr);
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool CCDSim::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Simulate RGB
        if (!strcmp(name, SimulateBayerSP.name))
        {
            IUUpdateSwitch(&SimulateBayerSP, states, names, n);
            int index = IUFindOnSwitchIndex(&SimulateBayerSP);
            if (index == -1)
            {
                SimulateBayerSP.s = IPS_ALERT;
                LOG_INFO("Cannot determine whether RGB simulation should be switched on or off.");
                IDSetSwitch(&SimulateBayerSP, nullptr);
                return false;
            }

            m_SimulateBayer = index == 0;
            setBayerEnabled(m_SimulateBayer);

            SimulateBayerS[INDI_ENABLED].s = m_SimulateBayer ? ISS_ON : ISS_OFF;
            SimulateBayerS[INDI_DISABLED].s = m_SimulateBayer ? ISS_OFF : ISS_ON;
            SimulateBayerSP.s   = IPS_OK;
            IDSetSwitch(&SimulateBayerSP, nullptr);

            return true;
        }
        else if (strcmp(name, CoolerSP.name) == 0)
        {
            IUUpdateSwitch(&CoolerSP, states, names, n);

            if (CoolerS[0].s == ISS_ON)
                CoolerSP.s = IPS_BUSY;
            else
            {
                CoolerSP.s          = IPS_IDLE;
                m_TargetTemperature = 20;
                TemperatureNP.s     = IPS_BUSY;
                m_TemperatureCheckTimer.start();
                m_TemperatureElapsedTimer.start();
            }

            IDSetSwitch(&CoolerSP, nullptr);

            return true;
        }
        else if (!strcmp(DirectorySP.name, name))
        {
            IUUpdateSwitch(&DirectorySP, states, names, n);
            m_AllFiles.clear();
            m_RemainingFiles.clear();
            if (DirectoryS[INDI_ENABLED].s == ISS_ON)
            {
                DIR* dirp = opendir(DirectoryT[0].text);
                if (dirp == nullptr)
                {
                    DirectoryS[INDI_ENABLED].s = ISS_OFF;
                    DirectoryS[INDI_DISABLED].s = ISS_ON;
                    DirectorySP.s = IPS_ALERT;
                    LOGF_ERROR("Cannot monitor invalid directory %s", DirectoryT[0].text);
                    IDSetSwitch(&DirectorySP, nullptr);
                    return true;
                }
                struct dirent * dp;
                std::string d_dir = std::string(DirectoryT[0].text);
                if (DirectoryT[0].text[strlen(DirectoryT[0].text) - 1] != '/')
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
                    IUResetSwitch(&DirectorySP);
                    DirectoryS[INDI_DISABLED].s = ISS_ON;
                    DirectorySP.s = IPS_ALERT;
                    LOGF_ERROR("No FITS files found in directory %s", DirectoryT[0].text);
                    IDSetSwitch(&DirectorySP, nullptr);
                }
                else
                {
                    DirectorySP.s = IPS_OK;
                    std::sort(m_AllFiles.begin(), m_AllFiles.end());
                    m_RemainingFiles = m_AllFiles;
                    LOGF_INFO("Directory-based images are enabled. Subsequent exposures will be loaded from directory %s", DirectoryT[0].text);
                }
            }
            else
            {
                m_RemainingFiles.clear();
                DirectorySP.s = IPS_OK;
                setBayerEnabled(SimulateBayerS[INDI_ENABLED].s == ISS_ON);
                LOG_INFO("Directory-based images are disabled.");
            }
            IDSetSwitch(&DirectorySP, nullptr);
            return true;
        }
        else if (strcmp(name, CrashSP.name) == 0)
        {
            abort();
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

void CCDSim::activeDevicesUpdated()
{
#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_PE");
#else
    IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "EQUATORIAL_EOD_COORD");
#endif
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "FWHM");

    strncpy(FWHMNP.device, ActiveDeviceT[ACTIVE_FOCUSER].text, MAXINDIDEVICE);
}

bool CCDSim::ISSnoopDevice(XMLEle * root)
{
    if (IUSnoopNumber(root, &FWHMNP) == 0)
    {
        // we calculate the FWHM and do not snoop it from the focus simulator
        // seeing = FWHMNP.np[0].value;
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

                // calculate FWHM
                double focus       = FocusSimulationN[0].value;
                double max         = FocusSimulationN[1].value;
                double optimalFWHM = FocusSimulationN[2].value;

                // limit to +/- 10
                double ticks = 20 * (FocuserPos - focus) / max;

                seeing = 0.5625 * ticks * ticks + optimalFWHM;
                return true;
            }
        }
    }
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
            usePE = true;

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

bool CCDSim::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);

    // Save Filter Wheel Config
    INDI::FilterInterface::saveConfigItems(fp);

    // Save CCD Simulator Config
    IUSaveConfigNumber(fp, &SimulatorSettingsNP);

    // Gain
    IUSaveConfigNumber(fp, &GainNP);
    IUSaveConfigNumber(fp, &OffsetNP);

    // Directory
    IUSaveConfigText(fp, &DirectoryTP);

    // Bayer
    IUSaveConfigSwitch(fp, &SimulateBayerSP);

    // Focus simulation
    IUSaveConfigNumber(fp, &FocusSimulationNP);

    return true;
}

bool CCDSim::SelectFilter(int f)
{
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

void CCDSim::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status = 0;
    fits_update_key_dbl(fptr, "Gain", GainN[0].value, 3, "Gain", &status);
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
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "0");
        IUSaveText(&BayerT[2], bayer_pattern);
    }
    else
    {
        SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    }

    fits_close_file(fptr, &status);
    return true;
}
