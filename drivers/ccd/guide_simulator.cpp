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

// We declare an auto pointer to GuideSim.
static std::unique_ptr<GuideSim> ccd(new GuideSim());

GuideSim::GuideSim()
{
    currentRA  = RA;
    currentDE = Dec;

    streamPredicate = 0;
    terminateThread = false;

    primaryFocalLength = 900; //  focal length of the telescope in millimeters
    guiderFocalLength  = 300;

    time(&RunStart);
}

bool GuideSim::SetupParms()
{
    int nbuf;
    SetCCDParams(SimulatorSettingsN[0].value, SimulatorSettingsN[1].value, 16, SimulatorSettingsN[2].value,
                 SimulatorSettingsN[3].value);

    if (HasCooler())
    {
        TemperatureN[0].value = 20;
        IDSetNumber(&TemperatureNP, nullptr);
    }

    //  Kwiq
    maxnoise      = SimulatorSettingsN[8].value;
    skyglow       = SimulatorSettingsN[9].value;
    maxval        = SimulatorSettingsN[4].value;
    bias          = SimulatorSettingsN[5].value;
    limitingmag   = SimulatorSettingsN[7].value;
    saturationmag = SimulatorSettingsN[6].value;
    OAGoffset = SimulatorSettingsN[10].value; //  An oag is offset this much from center of scope position (arcminutes);
    polarError = SimulatorSettingsN[11].value;
    polarDrift = SimulatorSettingsN[12].value;
    rotationCW = SimulatorSettingsN[13].value;
    //  Kwiq++
    king_gamma = SimulatorSettingsN[14].value * 0.0174532925;
    king_theta = SimulatorSettingsN[15].value * 0.0174532925;
    TimeFactor = SimulatorSettingsN[16].value;

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    //nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    Streamer->setPixelFormat(INDI_MONO, 16);
    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

    return true;
}

bool GuideSim::Connect()
{
    streamPredicate = 0;
    terminateThread = false;
    pthread_create(&primary_thread, nullptr, &streamVideoHelper, this);
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool GuideSim::Disconnect()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    terminateThread = true;
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

    IUFillNumber(&SimulatorSettingsN[0], "SIM_XRES", "CCD X resolution", "%4.0f", 0, 8192, 0, 1280);
    IUFillNumber(&SimulatorSettingsN[1], "SIM_YRES", "CCD Y resolution", "%4.0f", 0, 8192, 0, 1024);
    IUFillNumber(&SimulatorSettingsN[2], "SIM_XSIZE", "CCD X Pixel Size", "%4.2f", 0, 60, 0, 5.2);
    IUFillNumber(&SimulatorSettingsN[3], "SIM_YSIZE", "CCD Y Pixel Size", "%4.2f", 0, 60, 0, 5.2);
    IUFillNumber(&SimulatorSettingsN[4], "SIM_MAXVAL", "CCD Maximum ADU", "%4.0f", 0, 65000, 0, 65000);
    IUFillNumber(&SimulatorSettingsN[5], "SIM_BIAS", "CCD Bias", "%4.0f", 0, 6000, 0, 10);
    IUFillNumber(&SimulatorSettingsN[6], "SIM_SATURATION", "Saturation Mag", "%4.1f", 0, 20, 0, 1.0);
    IUFillNumber(&SimulatorSettingsN[7], "SIM_LIMITINGMAG", "Limiting Mag", "%4.1f", 0, 20, 0, 17.0);
    IUFillNumber(&SimulatorSettingsN[8], "SIM_NOISE", "CCD Noise", "%4.0f", 0, 6000, 0, 10);
    IUFillNumber(&SimulatorSettingsN[9], "SIM_SKYGLOW", "Sky Glow (magnitudes)", "%4.1f", 0, 6000, 0, 19.5);
    IUFillNumber(&SimulatorSettingsN[10], "SIM_OAGOFFSET", "Oag Offset (arcminutes)", "%4.1f", 0, 6000, 0, 0);
    IUFillNumber(&SimulatorSettingsN[11], "SIM_POLAR", "PAE (arcminutes)", "%4.1f", -600, 600, 0,
                 0); /* PAE = Polar Alignment Error */
    IUFillNumber(&SimulatorSettingsN[12], "SIM_POLARDRIFT", "PAE Drift (minutes)", "%4.1f", 0, 6000, 0, 0);
    IUFillNumber(&SimulatorSettingsN[13], "SIM_ROTATION", "Rotation CW (degrees)", "%4.1f", -360, 360, 0, 0);
    IUFillNumber(&SimulatorSettingsN[14], "SIM_KING_GAMMA", "(CP,TCP), deg", "%4.1f", 0, 10, 0, 0);
    IUFillNumber(&SimulatorSettingsN[15], "SIM_KING_THETA", "hour hangle, deg", "%4.1f", 0, 360, 0, 0);
    IUFillNumber(&SimulatorSettingsN[16], "SIM_TIME_FACTOR", "Time Factor (x)", "%.2f", 0.01, 100, 0, 1);

    IUFillNumberVector(&SimulatorSettingsNP, SimulatorSettingsN, 17, getDeviceName(), "SIMULATOR_SETTINGS",
                       "Simulator Settings", "Simulator Config", IP_RW, 60, IPS_IDLE);

    // RGB Simulation
    IUFillSwitch(&SimulateRgbS[0], "SIMULATE_YES", "Yes", ISS_OFF);
    IUFillSwitch(&SimulateRgbS[1], "SIMULATE_NO", "No", ISS_ON);
    IUFillSwitchVector(&SimulateRgbSP, SimulateRgbS, 2, getDeviceName(), "SIMULATE_RGB", "Simulate RGB",
                       "Simulator Config", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&FWHMN[0], "SIM_FWHM", "FWHM (arcseconds)", "%4.2f", 0, 60, 0, 7.5);
    IUFillNumberVector(&FWHMNP, FWHMN, 1, ActiveDeviceT[ACTIVE_FOCUSER].text, "FWHM", "FWHM", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%.f", 0, 100, 10, 50);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&EqPEN[0], "RA_PE", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&EqPEN[1], "DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&EqPENP, EqPEN, 2, getDeviceName(), "EQUATORIAL_PE", "EQ PE", "Simulator Config", IP_RW, 60,
                       IPS_IDLE);

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
    cap |= CCD_HAS_SHUTTER;
    cap |= CCD_HAS_ST4_PORT;
    cap |= CCD_HAS_STREAMING;

#ifdef HAVE_WEBSOCKET
    cap |= CCD_HAS_WEB_SOCKET;
#endif

    SetCCDCapability(cap);

    // This should be called after the initial SetCCDCapability (above)
    // as it modifies the capabilities.
    setRGB(simulateRGB);

    addDebugControl();

    setDriverInterface(getDriverInterface());

    // Make Guide Scope ON by default
    TelescopeTypeS[TELESCOPE_PRIMARY].s = ISS_OFF;
    TelescopeTypeS[TELESCOPE_GUIDE].s = ISS_ON;

    return true;
}

void GuideSim::setRGB(bool onOff)
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

void GuideSim::ISGetProperties(const char * dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineProperty(&SimulatorSettingsNP);
    defineProperty(&EqPENP);
    defineProperty(&SimulateRgbSP);
}

bool GuideSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
            defineProperty(&CoolerSP);

        defineProperty(&GainNP);

        SetupParms();

        if (HasGuideHead())
        {
            SetGuiderParams(500, 290, 16, 9.8, 12.6);
            GuideCCD.setFrameBufferSize(GuideCCD.getXRes() * GuideCCD.getYRes() * 2);
        }

    }
    else
    {
        if (HasCooler())
            deleteProperty(CoolerSP.name);

        deleteProperty(GainNP.name);
    }

    return true;
}

int GuideSim::SetTemperature(double temperature)
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

bool GuideSim::StartExposure(float duration)
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
    ExposureRequest = duration * TimeFactor;
    InExposure      = true;

    return true;
}

bool GuideSim::StartGuideExposure(float n)
{
    GuideExposureRequest = n;
    AbortGuideFrame      = false;
    GuideCCD.setExposureDuration(n);
    DrawCcdFrame(&GuideCCD);
    gettimeofday(&GuideExpStart, nullptr);
    InGuideExposure = true;
    return true;
}

bool GuideSim::AbortExposure()
{
    if (!InExposure)
        return true;

    AbortPrimaryFrame = true;

    return true;
}

bool GuideSim::AbortGuideExposure()
{
    //IDLog("Enter AbortGuideExposure\n");
    if (!InGuideExposure)
        return true; //  no need to abort if we aren't doing one
    AbortGuideFrame = true;
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
        float timeleft;
        timeleft = CalcTimeLeft(GuideExpStart, GuideExposureRequest);

        //IDLog("GUIDE Exposure left: %g - Requset: %g\n", timeleft, GuideExposureRequest);

        if (timeleft < 0)
            timeleft = 0;

        //ImageExposureN[0].value = timeleft;
        //IDSetNumber(ImageExposureNP, nullptr);
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

int GuideSim::DrawCcdFrame(INDI::CCDChip * targetChip)
{
    //  CCD frame is 16 bit data
    double exposure_time;
    double targetFocalLength;

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
        float PEOffset;
        float PESpot;
        float decDrift;
        double rad;  //  telescope ra in degrees
        double rar;  //  telescope ra in radians
        double decr; //  telescope dec in radians;
        int nwidth = 0, nheight = 0;

        double timesince;
        time_t now;
        time(&now);

        //  Lets figure out where we are on the pe curve
        timesince = difftime(now, RunStart);
        //  This is our spot in the curve
        PESpot = timesince / PEPeriod;
        //  Now convert to radians
        PESpot = PESpot * 2.0 * 3.14159;

        PEOffset = PEMax * std::sin(PESpot);
        //fprintf(stderr,"PEOffset = %4.2f arcseconds timesince %4.2f\n",PEOffset,timesince);
        PEOffset = PEOffset / 3600; //  convert to degrees
        //PeOffset=PeOffset/15;       //  ra is in h:mm

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

        double theta = rotationCW + 270;
        if (theta > 360)
            theta -= 360;
        if (pierSide == 1)
            theta -= 180;       // rotate 180 if on East
        else if (theta < -360)
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

            epochPos.ra  = currentRA * 15.0;
            epochPos.dec = currentDE;

            // Convert from JNow to J2000
            LibAstro::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);
            //ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

            currentRA  = J2000Pos.ra / 15.0;
            currentDE = J2000Pos.dec;

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
        cameradec = currentDE + OAGoffset / 60;
        decr      = cameradec * 0.0174532925;

        decDrift = (polarDrift * polarError * cos(decr)) / 3.81;

        // Add declination drift, if any.
        decr += decDrift / 3600.0 * 0.0174532925;

        //fprintf(stderr,"decPE %7.5f  cameradec %7.5f  CenterOffsetDec %4.4f\n",decPE,cameradec,decr);
        //  now lets calculate the radius we need to fetch
        float radius;

        radius = sqrt((Scalex * Scalex * targetChip->getXRes() / 2.0 * targetChip->getXRes() / 2.0) +
                      (Scaley * Scaley * targetChip->getYRes() / 2.0 * targetChip->getYRes() / 2.0));
        //  we have radius in arcseconds now
        radius = radius / 60; //  convert to arcminutes
#if 0
        LOGF_DEBUG("Lookup radius %4.2f", radius);
#endif

        //  A saturationmag star saturates in one second
        //  and a limitingmag produces a one adu level in one second
        //  solve for zero point and system gain

        k = (saturationmag - limitingmag) / ((-2.5 * log(maxval)) - (-2.5 * log(1.0 / 2.0)));
        z = saturationmag - k * (-2.5 * log(maxval));
        //z=z+saturationmag;

        //IDLog("K=%4.2f  Z=%4.2f\n",k,z);

        //  Should probably do some math here to figure out the dimmest
        //  star we can see on this exposure
        //  and only fetch to that magnitude
        //  for now, just use the limiting mag number with some room to spare
        float lookuplimit = limitingmag;

        if (radius > 60)
            lookuplimit = 11;

        if (king_gamma > 0.)
        {
            // wildi, make sure there are always stars, e.g. in case where king_gamma is set to 1 degree.
            // Otherwise the solver will fail.
            radius = 60.;

            // wildi, transform to telescope coordinate system, differential form
            // see E.S. King based on Chauvenet:
            // https://ui.adsabs.harvard.edu/link_gateway/1902AnHar..41..153K/ADS_PDF
            // Currently it is not possible to enable the logging in simulator devices (tested with ccd and telescope)
            // Replace LOGF_DEBUG by IDLog
            //IDLog("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++", king_gamma); // without variable, macro expansion fails
            char JnRAStr[64] = {0};
            fs_sexa(JnRAStr, RA, 2, 360000);
            char JnDecStr[64] = {0};
            fs_sexa(JnDecStr, Dec, 2, 360000);
            //            IDLog("Longitude      : %8.3f, Latitude    : %8.3f\n", this->Longitude, this->Latitude);
            //            IDLog("King gamma     : %8.3f, King theta  : %8.3f\n", king_gamma / 0.0174532925, king_theta / 0.0174532925);
            //            IDLog("Jnow RA        : %11s,       dec: %11s\n", JnRAStr, JnDecStr );
            //            IDLog("Jnow RA        : %8.3f, Dec         : %8.3f\n", RA * 15., Dec);
            //            IDLog("J2000    Pos.ra: %8.3f,      Pos.dec: %8.3f\n", J2000Pos.ra, J2000Pos.dec);
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

            //            IDLog("sid            : %s\n", sidStr);
            //            IDLog("Jnow                               JnHA: %8.3f degree\n", JnHAr / 0.0174532925);
            //            IDLog("                                JnHAStr: %11s hms\n", JnHAStr);
            // king_theta is the HA of the great circle where the HA axis is in.
            // RA is a right and HA a left handed coordinate system.
            // apparent or J2000? apparent, since we live now :-)

            // Transform to the mount coordinate system
            // remember it is the center of the simulated image
            double J2_mnt_d_rar = king_gamma * sin(J2decr) * sin(JnHAr - king_theta) / cos(J2decr);
            double J2_mnt_rar = rar - J2_mnt_d_rar
                                ; // rad = currentRA * 15.0; rar = rad * 0.0174532925; currentRA  = J2000Pos.ra / 15.0;

            // Imagine the HA axis points to HA=0, dec=89deg, then in the mount's coordinate
            // system a star at true dec = 88 is seen at 89 deg in the mount's system
            // Or in other words: if one uses the setting circle, that is the mount system,
            // and set it to 87 deg then the real location is at 88 deg.
            double J2_mnt_d_decr = king_gamma * cos(JnHAr - king_theta);
            double J2_mnt_decr = decr + J2_mnt_d_decr
                                 ; // decr      = cameradec * 0.0174532925; cameradec = currentDE + OAGoffset / 60; currentDE = J2000Pos.dec;
            //            IDLog("raw mod ra     : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / 0.0174532925, J2_mnt_decr / 0.0174532925 );
            if (J2_mnt_decr > M_PI / 2.)
            {
                J2_mnt_decr = M_PI / 2. - (J2_mnt_decr - M_PI / 2.);
                J2_mnt_rar -= M_PI;
            }
            J2_mnt_rar = fmod(J2_mnt_rar, 2. * M_PI) ;
            //            IDLog("mod sin        : %8.3f,          cos: %8.3f\n", sin(JnHAr - king_theta), cos(JnHAr - king_theta));
            //            IDLog("mod dra        : %8.3f,         ddec: %8.3f (degree)\n", J2_mnt_d_rar / 0.0174532925, J2_mnt_d_decr / 0.0174532925 );
            //            IDLog("mod ra         : %8.3f,          dec: %8.3f (degree)\n", J2_mnt_rar / 0.0174532925, J2_mnt_decr / 0.0174532925 );
            //IDLog("mod ra         : %11s,       dec: %11s\n",  );
            char J2RAStr[64] = {0};
            fs_sexa(J2RAStr, J2_mnt_rar / 15. / 0.0174532925, 2, 360000);
            char J2DecStr[64] = {0};
            fs_sexa(J2DecStr, J2_mnt_decr / 0.0174532925, 2, 360000);
            //            IDLog("mod ra         : %s,       dec: %s\n", J2RAStr, J2DecStr );
            //            IDLog("PEOffset       : %10.5f setting it to ZERO\n", PEOffset);
            PEOffset = 0.;
            // feed the result to the original variables
            rar = J2_mnt_rar ;
            rad = rar / 0.0174532925;
            decr = J2_mnt_decr;
            cameradec = decr / 0.0174532925;
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
                    range360(rad + PEOffset),
                    rangeDec(cameradec),
                    radius,
                    lookuplimit);

            if (!Streamer->isStreaming() || (king_gamma > 0.))
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
                    //fprintf(stderr,"Parsed %d items\n",rc);
                    if (rc == 12)
                    {
                        lines++;
                        //if(c==0) {
                        stars++;
                        //fprintf(stderr,"%s %8.4f %8.4f %5.2f %5.2f %d\n",id,ra,dec,mag,dist,dir);

                        //  Convert the ra/dec to standard co-ordinates
                        double sx;    //  standard co-ords
                        double sy;    //
                        double srar;  //  star ra in radians
                        double sdecr; //  star dec in radians;
                        double ccdx;
                        double ccdy;

                        //fprintf(stderr,"line %s",line);
                        //fprintf(stderr,"parsed %6.5f %6.5f\n",ra,dec);

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
            float glow = skyglow;

            if (ftype == INDI::CCDChip::FLAT_FRAME)
            {
                //  Assume flats are done with a diffuser
                //  in broad daylight, so, the sky magnitude
                //  is much brighter than at night
                glow = skyglow / 10;
            }

            //fprintf(stderr,"Using glow %4.2f\n",glow);

            skyflux = pow(10, ((glow - z) * k / -2.5));
            //  ok, flux represents one second now
            //  scale up linearly for exposure time
            skyflux = skyflux * exposure_time;
            //IDLog("SkyFlux = %g ExposureRequest %g\n",skyflux,exposure_time);

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
                    if (fp > maxval)
                        fp = maxval;
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

        if (maxnoise > 0)
        {
            for (int x = subX; x < subW; x++)
            {
                for (int y = subY; y < subH; y++)
                {
                    int noise;

                    noise = random();
                    noise = noise % maxnoise; //

                    //IDLog("noise is %d\n", noise);
                    AddToPixel(targetChip, x, y, bias + noise);
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

int GuideSim::DrawImageStar(INDI::CCDChip * targetChip, float mag, float x, float y, float exposure_time)
{
    //float d;
    //float r;
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
                    if (newval > maxval)
                        newval = maxval;
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

IPState GuideSim::GuideNorth(uint32_t v)
{
    guideNSOffset    += v / 1000.0 * GuideRate / 3600;
    return IPS_OK;
}

IPState GuideSim::GuideSouth(uint32_t v)
{
    guideNSOffset    += v / -1000.0 * GuideRate / 3600;
    return IPS_OK;
}

IPState GuideSim::GuideEast(uint32_t v)
{
    float c   = v / 1000.0 * GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(currentDE * 0.0174532925));

    guideWEOffset += c;

    return IPS_OK;
}

IPState GuideSim::GuideWest(uint32_t v)
{
    float c   = v / -1000.0 * GuideRate;
    c   = c / 3600.0 / 15.0;
    c   = c / (cos(currentDE * 0.0174532925));

    guideWEOffset += c;

    return IPS_OK;
}

bool GuideSim::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
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

        if (strcmp(name, "SIMULATOR_SETTINGS") == 0)
        {
            IUUpdateNumber(&SimulatorSettingsNP, values, names, n);
            SimulatorSettingsNP.s = IPS_OK;

            //  Reset our parameters now
            SetupParms();
            IDSetNumber(&SimulatorSettingsNP, nullptr);

            maxnoise      = SimulatorSettingsN[8].value;
            skyglow       = SimulatorSettingsN[9].value;
            maxval        = SimulatorSettingsN[4].value;
            bias          = SimulatorSettingsN[5].value;
            limitingmag   = SimulatorSettingsN[7].value;
            saturationmag = SimulatorSettingsN[6].value;
            OAGoffset = SimulatorSettingsN[10].value;
            polarError = SimulatorSettingsN[11].value;
            polarDrift = SimulatorSettingsN[12].value;
            rotationCW = SimulatorSettingsN[13].value;
            //  Kwiq++
            king_gamma = SimulatorSettingsN[14].value * 0.0174532925;
            king_theta = SimulatorSettingsN[15].value * 0.0174532925;
            TimeFactor = SimulatorSettingsN[16].value;

            return true;
        }

        // Record PE EQ to simulate different position in the sky than actual mount coordinate
        // This can be useful to simulate Periodic Error or cone error or any arbitrary error.
        if (!strcmp(name, EqPENP.name))
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

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool GuideSim::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

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

            simulateRGB = index == 0;
            setRGB(simulateRGB);

            SimulateRgbS[0].s = simulateRGB ? ISS_ON : ISS_OFF;
            SimulateRgbS[1].s = simulateRGB ? ISS_OFF : ISS_ON;
            SimulateRgbSP.s   = IPS_OK;
            IDSetSwitch(&SimulateRgbSP, nullptr);

            return true;
        }

        if (strcmp(name, CoolerSP.name) == 0)
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
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

void GuideSim::activeDevicesUpdated()
{
#ifdef USE_EQUATORIAL_PE
    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_PE");
#else
    IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "EQUATORIAL_EOD_COORD");
#endif
    IDSnoopDevice(ActiveDeviceT[ACTIVE_FOCUSER].text, "FWHM");

    strncpy(FWHMNP.device, ActiveDeviceT[ACTIVE_FOCUSER].text, MAXINDIDEVICE);
}

bool GuideSim::ISSnoopDevice(XMLEle * root)
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

bool GuideSim::saveConfigItems(FILE * fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);


    // Save CCD Simulator Config
    IUSaveConfigNumber(fp, &SimulatorSettingsNP);

    // Gain
    IUSaveConfigNumber(fp, &GainNP);

    // RGB
    IUSaveConfigSwitch(fp, &SimulateRgbSP);

    return true;
}

bool GuideSim::StartStreaming()
{
    ExposureRequest = 1.0 / Streamer->getTargetExposure();
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool GuideSim::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 0;
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

void GuideSim::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status = 0;
    fits_update_key_dbl(fptr, "Gain", GainN[0].value, 3, "Gain", &status);
}
