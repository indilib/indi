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

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

static std::unique_ptr<CCDSim> ccdsim(new CCDSim());

void ISGetProperties(const char * dev)
{
    ccdsim->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    ccdsim->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    ccdsim->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    ccdsim->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
{
    ccdsim->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle * root)
{
    ccdsim->ISSnoopDevice(root);
}

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

    //    if (HasCooler())
    //    {
    //        TemperatureN[0].value = 20;
    //        IDSetNumber(&TemperatureNP, nullptr);
    //    }

    m_MaxNoise      = SimulatorSettingsN[SIM_NOISE].value;
    m_SkyGlow       = SimulatorSettingsN[SIM_SKYGLOW].value;
    m_MaxVal        = SimulatorSettingsN[SIM_MAXVAL].value;
    m_Bias          = SimulatorSettingsN[SIM_BIAS].value;
    m_LimitingMag   = SimulatorSettingsN[SIM_LIMITINGMAG].value;
    m_SaturationMag = SimulatorSettingsN[SIM_SATURATION].value;
    //  An oag is offset this much from center of scope position (arcminutes);
    m_OAGOffset = SimulatorSettingsN[SIM_OAGOFFSET].value;
    m_PolarError = SimulatorSettingsN[SIM_POLAR].value;
    m_PolarDrift = SimulatorSettingsN[SIM_POLARDRIFT].value;
    m_PEPeriod = SimulatorSettingsN[SIM_PE_PERIOD].value;
    m_PEMax = SimulatorSettingsN[SIM_PE_MAX].value;
    m_RotationCW = SimulatorSettingsN[SIM_ROTATION].value;
    m_KingGamma = SimulatorSettingsN[SIM_KING_GAMMA].value * 0.0174532925;
    m_KingTheta = SimulatorSettingsN[SIM_KING_THETA].value * 0.0174532925;
    m_TimeFactor = SimulatorSettingsN[SIM_TIME_FACTOR].value;

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
    SetTimer(POLLMS);
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

    IUFillNumber(&SimulatorSettingsN[SIM_XRES], "SIM_XRES", "CCD X resolution", "%4.0f", 0, 8192, 0, 1280);
    IUFillNumber(&SimulatorSettingsN[SIM_YRES], "SIM_YRES", "CCD Y resolution", "%4.0f", 0, 8192, 0, 1024);
    IUFillNumber(&SimulatorSettingsN[SIM_XSIZE], "SIM_XSIZE", "CCD X Pixel Size", "%4.2f", 0, 60, 0, 5.2);
    IUFillNumber(&SimulatorSettingsN[SIM_YSIZE], "SIM_YSIZE", "CCD Y Pixel Size", "%4.2f", 0, 60, 0, 5.2);
    IUFillNumber(&SimulatorSettingsN[SIM_MAXVAL], "SIM_MAXVAL", "CCD Maximum ADU", "%4.0f", 0, 65000, 0, 65000);
    IUFillNumber(&SimulatorSettingsN[SIM_BIAS], "SIM_BIAS", "CCD Bias", "%4.0f", 0, 6000, 0, 10);
    IUFillNumber(&SimulatorSettingsN[SIM_SATURATION], "SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 0, 1.0);
    IUFillNumber(&SimulatorSettingsN[SIM_LIMITINGMAG], "SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 0, 17.0);
    IUFillNumber(&SimulatorSettingsN[SIM_NOISE], "SIM_NOISE", "CCD Noise", "%4.0f", 0, 6000, 0, 10);
    IUFillNumber(&SimulatorSettingsN[SIM_SKYGLOW], "SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 0, 19.5);
    IUFillNumber(&SimulatorSettingsN[SIM_OAGOFFSET], "SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_POLAR], "SIM_POLAR", "PAE (arcminutes)", "%4.1f", -600, 600, 0,
                 0);
    IUFillNumber(&SimulatorSettingsN[SIM_POLARDRIFT], "SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.1f", 0, 6000, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_PE_PERIOD], "SIM_PEPERIOD", "PE Period (minutes)", "%4.1f", 0, 60, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_PE_MAX], "SIM_PEMAX", "PE Max (arcsec)", "%4.1f", 0, 6000, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_ROTATION], "SIM_ROTATION", "Rotation CW (degrees)", "%4.1f", -360, 360, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_KING_GAMMA], "SIM_KING_GAMMA", "(CP,TCP), deg", "%4.1f", 0, 10, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_KING_THETA], "SIM_KING_THETA", "hour hangle, deg", "%4.1f", 0, 360, 0, 0);
    IUFillNumber(&SimulatorSettingsN[SIM_TIME_FACTOR], "SIM_TIME_FACTOR", "Time Factor (x)", "%.2f", 0.01, 100, 0, 1);

    IUFillNumberVector(&SimulatorSettingsNP, SimulatorSettingsN, SIM_N, getDeviceName(), "SIMULATOR_SETTINGS",
                       "Settings", SIMULATOR_TAB, IP_RW, 60, IPS_IDLE);

    // RGB Simulation
    IUFillSwitch(&SimulateRgbS[0], "SIMULATE_YES", "Yes", ISS_OFF);
    IUFillSwitch(&SimulateRgbS[1], "SIMULATE_NO", "No", ISS_ON);
    IUFillSwitchVector(&SimulateRgbSP, SimulateRgbS, 2, getDeviceName(), "SIMULATE_RGB", "RGB",
                       SIMULATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%.f", 0, 100, 10, 50);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);


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
    setRGB(m_SimulateRGB);

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

void CCDSim::setRGB(bool onOff)
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

    defineNumber(&SimulatorSettingsNP);
    defineNumber(&EqPENP);
    defineSwitch(&SimulateRgbSP);
    defineSwitch(&CrashSP);
}

bool CCDSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
            defineSwitch(&CoolerSP);

        defineNumber(&GainNP);

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
    uint32_t nextTimer = POLLMS;

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
        if (fabs(TemperatureRequest - TemperatureN[0].value) <= 0.5)
        {
            LOGF_INFO("Temperature reached requested value %.2f degrees C", TemperatureRequest);
            TemperatureN[0].value = TemperatureRequest;
            TemperatureNP.s       = IPS_OK;
        }
        else
        {
            if (TemperatureRequest < TemperatureN[0].value)
                TemperatureN[0].value -= 0.5;
            else
                TemperatureN[0].value += 0.5;
        }

        IDSetNumber(&TemperatureNP, nullptr);

        // Above 20, cooler is off
        if (TemperatureN[0].value >= 20)
        {
            CoolerS[0].s = ISS_OFF;
            CoolerS[0].s = ISS_ON;
            CoolerSP.s   = IPS_IDLE;
            IDSetSwitch(&CoolerSP, nullptr);
        }
    }


    SetTimer(nextTimer);
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

        double theta = m_RotationCW + 270;
        if (pierSide == 1)
            theta -= 180;       // rotate 180 if on East

        if (theta >= 360)
            theta -= 360;
        else if (theta <= -360)
            theta += 360;

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

            ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };

            double jd = ln_get_julian_from_sys();

            epochPos.ra  = currentRA * 15.0;
            epochPos.dec = currentDE;

            // Convert from JNow to J2000
            LibAstro::ObservedToJ2000(&epochPos, jd, &J2000Pos);

            currentRA  = J2000Pos.ra / 15.0;
            currentDE = J2000Pos.dec;

            //LOGF_DEBUG("DrawCcdFrame JNow %f, %f J2000 %f, %f", epochPos.ra, epochPos.dec, J2000Pos.ra, J2000Pos.dec);
            //ln_equ_posn jnpos;
            //LibAstro::J2000toObserved(&J2000Pos, jd, &jnpos);
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

        k = (m_SaturationMag - m_LimitingMag) / ((-2.5 * log(m_MaxVal)) - (-2.5 * log(1.0 / 2.0)));
        z = m_SaturationMag - k * (-2.5 * log(m_MaxVal));

        //  Should probably do some math here to figure out the dimmest
        //  star we can see on this exposure
        //  and only fetch to that magnitude
        //  for now, just use the limiting mag number with some room to spare
        double lookuplimit = m_LimitingMag;

        if (radius > 60)
            lookuplimit = 11;

        if (m_KingGamma > 0.)
        {
            // wildi, make sure there are always stars, e.g. in case where king_gamma is set to 1 degree.
            // Otherwise the solver will fail.
            radius = 60.;

            // wildi, transform to telescope coordinate system, differential form
            // see E.S. King based on Chauvenet:
            // https://ui.adsabs.harvard.edu/link_gateway/1902AnHar..41..153K/ADS_PDF
            char JnRAStr[64] = {0};
            fs_sexa(JnRAStr, RA, 2, 360000);
            char JnDecStr[64] = {0};
            fs_sexa(JnDecStr, Dec, 2, 360000);
#ifdef __DEV__
            //            IDLog("Longitude      : %8.3f, Latitude    : %8.3f\n", this->Longitude, this->Latitude);
            //            IDLog("King gamma     : %8.3f, King theta  : %8.3f\n", king_gamma / 0.0174532925, king_theta / 0.0174532925);
            //            IDLog("Jnow RA        : %11s,       dec: %11s\n", JnRAStr, JnDecStr );
            //            IDLog("Jnow RA        : %8.3f, Dec         : %8.3f\n", RA * 15., Dec);
            //            IDLog("J2000    Pos.ra: %8.3f,      Pos.dec: %8.3f\n", J2000Pos.ra, J2000Pos.dec);
#endif

            // Since the catalog is J2000, we  are going back in time
            // tra, tdec are at the center of the projection center for the simulated
            // images
            //double J2ra = J2000Pos.ra;  // J2000Pos: 0,360, RA: 0,24
            double J2dec = J2000Pos.dec;

            //double J2rar = J2ra * 0.0174532925;
            double J2decr = J2dec * 0.0174532925;
            double sid  = get_local_sidereal_time(this->Longitude);
            // HA is what is observed, that is Jnow
            // ToDo check if mean or apparent
            double JnHAr  = get_local_hour_angle(sid, RA) * 15. * 0.0174532925;

            char sidStr[64] = {0};
            fs_sexa(sidStr, sid, 2, 3600);
            char JnHAStr[64] = {0};
            fs_sexa(JnHAStr, JnHAr / 15. / 0.0174532925, 2, 360000);

#ifdef __DEV__
            IDLog("sid            : %s\n", sidStr);
            IDLog("Jnow                               JnHA: %8.3f degree\n", JnHAr / 0.0174532925);
            IDLog("                                JnHAStr: %11s hms\n", JnHAStr);
#endif
            // king_theta is the HA of the great circle where the HA axis is in.
            // RA is a right and HA a left handed coordinate system.
            // apparent or J2000? apparent, since we live now :-)

            // Transform to the mount coordinate system
            // remember it is the center of the simulated image
            double J2_mnt_d_rar = m_KingGamma * sin(J2decr) * sin(JnHAr - m_KingTheta) / cos(J2decr);
            double J2_mnt_rar = rar - J2_mnt_d_rar
                                ; // rad = currentRA * 15.0; rar = rad * 0.0174532925; currentRA  = J2000Pos.ra / 15.0;

            // Imagine the HA axis points to HA=0, dec=89deg, then in the mount's coordinate
            // system a star at true dec = 88 is seen at 89 deg in the mount's system
            // Or in other words: if one uses the setting circle, that is the mount system,
            // and set it to 87 deg then the real location is at 88 deg.
            double J2_mnt_d_decr = m_KingGamma * cos(JnHAr - m_KingTheta);
            double J2_mnt_decr = decr + J2_mnt_d_decr;
#ifdef __DEV__
            IDLog("raw mod ra     : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / 0.0174532925, J2_mnt_decr / 0.0174532925 );
#endif
            if (J2_mnt_decr > M_PI / 2.)
            {
                J2_mnt_decr = M_PI / 2. - (J2_mnt_decr - M_PI / 2.);
                J2_mnt_rar -= M_PI;
            }
            J2_mnt_rar = fmod(J2_mnt_rar, 2. * M_PI) ;
#ifdef __DEV__
            IDLog("mod sin        : %8.3f,          cos: %8.3f\n", sin(JnHAr - king_theta), cos(JnHAr - king_theta));
            IDLog("mod dra        : %8.3f,         ddec: %8.3f (degree)\n", J2_mnt_d_rar / 0.0174532925, J2_mnt_d_decr / 0.0174532925 );
            IDLog("mod ra         : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / 0.0174532925, J2_mnt_decr / 0.0174532925 );
#endif
            char J2RAStr[64] = {0};
            fs_sexa(J2RAStr, J2_mnt_rar / 15. / 0.0174532925, 2, 360000);
            char J2DecStr[64] = {0};
            fs_sexa(J2DecStr, J2_mnt_decr / 0.0174532925, 2, 360000);
#ifdef __DEV__
            IDLog("mod ra         : %s,       dec: %s\n", J2RAStr, J2DecStr );
            IDLog("PEOffset       : %10.5f setting it to ZERO\n", PEOffset);
#endif
            PEOffset = 0.;
            // feed the result to the original variables
            rar = J2_mnt_rar ;
            rad = rar / 0.0174532925;
            decr = J2_mnt_decr;
            cameradec = decr / 0.0174532925;
#ifdef __DEV__
            IDLog("mod ra      rad: %8.3f (degree)\n", rad);
#endif
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
                    range360(rad + PEOffset),
                    rangeDec(cameradec),
                    radius,
                    lookuplimit);

            if (!Streamer->isStreaming() || (m_KingGamma > 0.))
                LOGF_DEBUG("GSC Command: %s", gsccmd);

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

            skyflux = pow(10, ((glow - z) * k / -2.5));
            //  ok, flux represents one second now
            //  scale up linearly for exposure time
            skyflux = skyflux * exposure_time;

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
                    vig = vig * ImageScalex;
                    //  need to make this account for actual pixel size
                    dc = std::sqrt(sx * sx * ImageScalex * ImageScalex + sy * sy * ImageScaley * ImageScaley);
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
                    if (fp > maxpix)
                        maxpix = fp;
                    if (fp < minpix)
                        minpix = fp;
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
    flux = pow(10, ((mag - z) * k / -2.5));

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
            float dc; //  distance from center
            float fp; //  flux this pixel;

            //  need to make this account for actual pixel size
            dc = std::sqrt(sx * sx * ImageScalex * ImageScalex + sy * sy * ImageScaley * ImageScaley);
            //  now we have the distance from center, in arcseconds
            //  This should be gaussian, but, for now we'll just go with
            //  a simple linear function
            float fa = exp(-2.0 * 0.7 * (dc * dc) / seeing / seeing);

            fp = fa * flux;

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

            ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
            epochPos.ra  = EqPEN[AXIS_RA].value * 15.0;
            epochPos.dec = EqPEN[AXIS_DE].value;

            RA = EqPEN[AXIS_RA].value;
            Dec = EqPEN[AXIS_DE].value;

            LibAstro::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            //ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);
            currentRA  = J2000Pos.ra / 15.0;
            currentDE = J2000Pos.dec;
            usePE = true;

            IDSetNumber(&EqPENP, nullptr);
            return true;
        }
        else if (!strcmp(name, FilterSlotNP.name))
        {
            INDI::FilterInterface::processNumber(dev, name, values, names, n);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool CCDSim::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Simulate RGB
        if (!strcmp(name, SimulateRgbSP.name))
        {
            IUUpdateSwitch(&SimulateRgbSP, states, names, n);
            int index = IUFindOnSwitchIndex(&SimulateRgbSP);
            if (index == -1)
            {
                SimulateRgbSP.s = IPS_ALERT;
                LOG_INFO("Cannot determine whether RGB simulation should be switched on or off.");
                IDSetSwitch(&SimulateRgbSP, nullptr);
                return false;
            }

            m_SimulateRGB = index == 0;
            setRGB(m_SimulateRGB);

            SimulateRgbS[0].s = m_SimulateRGB ? ISS_ON : ISS_OFF;
            SimulateRgbS[1].s = m_SimulateRGB ? ISS_OFF : ISS_ON;
            SimulateRgbSP.s   = IPS_OK;
            IDSetSwitch(&SimulateRgbSP, nullptr);

            return true;
        }
        else if (strcmp(name, CoolerSP.name) == 0)
        {
            IUUpdateSwitch(&CoolerSP, states, names, n);

            if (CoolerS[0].s == ISS_ON)
                CoolerSP.s = IPS_BUSY;
            else
            {
                CoolerSP.s         = IPS_IDLE;
                TemperatureRequest = 20;
                TemperatureNP.s    = IPS_BUSY;
            }

            IDSetSwitch(&CoolerSP, nullptr);

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
        seeing = FWHMNP.np[0].value;
        return true;
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
            ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
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

    // RGB
    IUSaveConfigSwitch(fp, &SimulateRgbSP);

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

void * CCDSim::streamVideoHelper(void * context)
{
    return static_cast<CCDSim *>(context)->streamVideo();
}

void * CCDSim::streamVideo()
{
    auto start = std::chrono::high_resolution_clock::now();
    auto finish = std::chrono::high_resolution_clock::now();

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

        finish = std::chrono::high_resolution_clock::now();
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

