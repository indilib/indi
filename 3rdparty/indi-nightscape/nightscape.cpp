/*
   Based on INDI Developers Manual Tutorial #3

   "Nightscape 8300 CCD Driver"

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file NightscapeCCD.cpp
    \brief Construct a basic INDI CCD device  It also generates an image  and uploads it as a FITS file.
    \author Dirk Niggemann

    \example NightscapeCCD.cpp
    A simple CCD device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex CCDs, please
    refer to the INDI Generic CCD driver template in INDI SVN (under 3rdparty).
*/


#ifdef _DARWIN_C_SOURCE
#define ___secure_getenv getenv
#else
#define ___secure_getenv secure_getenv
#endif

#include "nightscape.h"

#include <memory>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <math.h>

/* Macro shortcut to CCD temperature value */
#define currentCCDTemperature TemperatureN[0].value

static std::unique_ptr<NightscapeCCD> nightscapeCCD(new NightscapeCCD());

static int drop_root_privileges(void) {  // returns 0 on success and -1 on failure
    gid_t gid;
    uid_t uid;
    DO_DBG("%s\n", "privilege drop");
    // no need to "drop" the privileges that you don't have in the first place!
    if (getuid() != 0) {
        return 0;
    }

    // when your program is invoked with sudo, getuid() will return 0 and you
    // won't be able to drop your privileges
    if ((uid = getuid()) == 0) {
        const char *sudo_uid = ___secure_getenv("SUDO_UID");
        if (sudo_uid == nullptr) {
            DO_ERR("%s\n", "environment variable `SUDO_UID` not found");
            return -1;
        }
        errno = 0;
        uid = (uid_t) strtoll(sudo_uid, nullptr, 10);
        if (errno != 0) {
            DO_ERR("under-/over-flow in converting `SUDO_UID` to integer %s", strerror(errno));
            return -1;
        }
    }

    // again, in case your program is invoked using sudo
    if ((gid = getgid()) == 0) {
        const char *sudo_gid = ___secure_getenv("SUDO_GID");
        if (sudo_gid == nullptr) {
            DO_ERR("%s\n", "environment variable `SUDO_GID` not found");
            return -1;
        }
        errno = 0;
        gid = (gid_t) strtoll(sudo_gid, nullptr, 10);
        if (errno != 0) {
            DO_ERR("under-/over-flow in converting `SUDO_GID` to integer %s", strerror(errno));
            return -1;
        }
    }

    if (setgid(gid) != 0) {
        DO_ERR("setgid %s", strerror(errno));
        return -1;
    }
    if (setuid(uid) != 0) {
        DO_ERR("setuid %s", strerror(errno));
        return -1;
    }

    // change your directory to somewhere else, just in case if you are in a
    // root-owned one (e.g. /root)
    if (chdir("/") != 0) {
        DO_ERR("chdir %s", strerror(errno));
        return -1;
    }

    // check if we successfully dropped the root privileges
    if (setuid(0) == 0 || seteuid(0) == 0) {
        DO_ERR("%s\n", "could not drop root privileges!");
        return -1;
    }

    return 0;
}


void ISGetProperties(const char *dev)
{
    nightscapeCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    nightscapeCCD->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    nightscapeCCD->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    nightscapeCCD->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    nightscapeCCD->ISSnoopDevice(root);
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool NightscapeCCD::Connect()
{
#ifdef HAVE_D2XX   
    if (useD2xx == 1) { cn = new NsChannelFTD(camnum);
    }
    else
#endif
        if (useD2xx == 0)
        {
            cn = new NsChannelU(camnum);
        }
#ifdef HAVE_SERIAL
        else
        {
            cn = new NsChannelSER(camnum);
        }
#endif
    if (cn->open() < 0) {
        LOG_DEBUG( "open failed!");
        delete cn;
        return false;
    } else {
        LOG_DEBUG( "opened  successfully!");
    }

    m = new Nsmsg(cn);
    dn = new NsDownload(cn);
    st = new NsStatus(m, dn);
    if(!m->inquiry()) {
        LOG_WARN( "inquiry failed!");
        delete cn;
        delete dn;
        delete st;
        return false;

    } else {
        LOGF_INFO( "Firmware ver %s", m->getFirmwareVer());
    }
    dn->setFrameYBinning(1);
    dn->setFrameXBinning(1);

    dn->setIncrement(1);
    dn->setFbase("");
    dn->setNumExp(99999);
    dn->setImgWrite(false);
    if (useD2xx == 0) {
        dn->setZeroReads(100);
    }
    if (useD2xx == 2) {
        //dn->setZeroReads(100);
    }
    dn->startThread();
    st->startThread();
    // Let's set a timer that checks teleCCDs status every POLLMS milliseconds.
    SetTimer(POLLMS);
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool NightscapeCCD::Disconnect()
{
    LOG_INFO("Nightscape CCD disconnected successfully!");
    m->abort();

    dn->stopThread();
    st->stopThread();
    //m->sendfan(deffanspeed);
    cn->close();
    delete m;
    
    delete cn;
    delete dn;
    m = nullptr;
    dn = nullptr;
    cn = nullptr;

    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *NightscapeCCD::getDefaultName()
{
    return "Nightscape 8300";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool NightscapeCCD::initProperties()
{
    setpriority(PRIO_PROCESS, getpid(), -20);
    drop_root_privileges();

    // Must init parent properties first!
    INDI::CCD::initProperties();
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", cooler ? ISS_ON : ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", cooler ? ISS_OFF : ISS_ON );
    
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler",
                       MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);
    
    IUFillSwitch(&FanS[0], "FANOFF", "Off", fanspeed == 1 ? ISS_ON: ISS_OFF);
    IUFillSwitch(&FanS[1], "FANQUIET", "Quiet", fanspeed == 2 ? ISS_ON: ISS_OFF);
    IUFillSwitch(&FanS[2], "FANFULL", "Full", fanspeed == 3 ? ISS_ON: ISS_OFF);
    IUFillSwitchVector(&FanSP, FanS, 3, getDeviceName(), "CCD_FAN", "Fan",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    

    IUFillNumber(&CamNumN[0], "CAMNUM", "Camera Number", "%4.0f", 1.0, 4.0, 1.0, 1.0);
    IUFillNumberVector(&CamNumNP, CamNumN, 1, getDeviceName(), "CAMNUM", "Camera Number",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    defineNumber(&CamNumNP);
    
    IUFillSwitch(&D2xxS[0], "USEFTDI", "libftdi", useD2xx == 0 ? ISS_ON: ISS_OFF);
#ifdef HAVE_D2XX
    IUFillSwitch(&D2xxS[1], "USED2XX", "libd2xx", useD2xx== 1 ? ISS_ON: ISS_OFF);
#ifdef HAVE_SERIAL
    IUFillSwitch(&D2xxS[2], "USESERIAL", "Serial", useD2xx == 2? ISS_ON: ISS_OFF);
#endif
#else
#ifdef HAVE_SERIAL
    IUFillSwitch(&D2xxS[1], "USESERIAL", "Serial", useD2xx == 2? ISS_ON: ISS_OFF);
#endif
#endif

#ifdef HAVE_D2XX
#ifdef HAVE_SERIAL
    IUFillSwitchVector(&D2xxSP, D2xxS, 3, getDeviceName(), "CCD_LIBRARY", "USB Library",
                       MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);
#else 
    IUFillSwitchVector(&D2xxSP, D2xxS, 2, getDeviceName(), "CCD_LIBRARY", "USB Library",
                       MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);
#endif
#else 
#ifdef HAVE_SERIAL
    IUFillSwitchVector(&D2xxSP, D2xxS, 2, getDeviceName(), "CCD_LIBRARY", "USB Library", 
    	MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);
#else 
    IUFillSwitchVector(&D2xxSP, D2xxS, 1, getDeviceName(), "CCD_LIBRARY", "USB Library",
                       MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 60, IPS_IDLE);
#endif
#endif
    defineSwitch(&D2xxSP);
    
    // We set the CCD capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER ;
    if (bayer) {
        cap |= CCD_HAS_BAYER;
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "1");
        IUSaveText(&BayerT[2], "RGGB");

    }
    SetCCDCapability(cap);

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool NightscapeCCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();
        defineSwitch(&CoolerSP);
        defineSwitch(&FanSP);

        // Start the timer
        SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(FanSP.name);
        deleteProperty(CoolerSP.name);
    }

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void NightscapeCCD::setupParams()
{
    // Our CCD is an 16 bit CCD, 1280x1024 resolution, with 5.4um square pixels.
    SetCCDParams(KAF8300_ACTIVE_X, IMG_Y, 16, 5.4, 5.4);
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    nbuf += 512; //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);
    //IDLog("fbuf size %d\n",nbuf);

    IUResetSwitch(&FanSP);
    FanS[fanspeed - 1].s = ISS_ON;
    defineSwitch(&FanSP);
    IUResetSwitch(&CoolerSP);
    CoolerS[!cooler].s = ISS_ON;
    defineSwitch(&CoolerSP);


}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool NightscapeCCD::StartExposure(float duration)
{
    ExposureRequest = duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart, nullptr);
    //int zonestart = (IMG_Y - PrimaryCCD.getSubH()) /2;
    int zonestart = PrimaryCCD.getSubY();
    int zonelen = PrimaryCCD.getSubH();
    int framediv = PrimaryCCD.getBinY();
    PrimaryCCD.setPixelSize(5.4*PrimaryCCD.getBinX(), 5.4*PrimaryCCD.getBinY());
    dn->setImgSize(m->getRawImgSize(zonestart,zonelen,framediv));
    dn->setFrameYBinning(framediv);
    dn->setFrameXBinning(PrimaryCCD.getBinX());
    m->sendzone(zonestart , zonelen, framediv);
    INDI::CCDChip::CCD_FRAME ft = PrimaryCCD.getFrameType();
    if (ft == INDI::CCDChip::DARK_FRAME || ft == INDI::CCDChip::BIAS_FRAME) dark = true;
    else dark = false;
    m->senddur(duration, framediv, dark);
    InExposure = true;
    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool NightscapeCCD::AbortExposure()
{
    InExposure = false;
    m->abort();
    return true;
}

/**************************************************************************************
** Client is asking us to set a new temperature
***************************************************************************************/
int NightscapeCCD::SetTemperature(double temperature)
{
    setTemp = TemperatureRequest = temperature;
    m->sendtemp(setTemp, cooler);
    dn->setSetTemp(setTemp);
    ntemps = 0;
    backoffs = 1;
    // 0 means it will take a while to change the temperature
    return 0;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float NightscapeCCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
            (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void NightscapeCCD::TimerHit()
{
    long timeleft;

    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and simpleCCD for better timing checks
        if (timeleft < 0.1)
        {
            /* We're done exposing */
            LOG_INFO( "Exposure done, starting readout...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            InExposure = false;
            InReadout = true;
            /* grab and save image */
            st->doStatus();

            
        }
        else
        {    // Just update time left in client
            PrimaryCCD.setExposureLeft(timeleft);
        }
    }
    if (InReadout) {
        stat = st->getStatus();
        if(oldstat == 2 && stat == 0) {
            LOG_INFO( "Starting download...");
            InReadout = false;
            InDownload = true;
        }
        oldstat = stat;

    }
    if (InDownload && !dn->inDownload()) {
        LOG_INFO( "download done...");
        InDownload = false;
        grabImage();
    }


    // TemperatureNP is defined in INDI::CCD
    switch (TemperatureNP.s)
    {
    case IPS_IDLE:
    case IPS_OK:
        break;

    case IPS_BUSY:
        if (InDownload || InReadout || InExposure) break;
        if(ntemps % backoffs == 0) {
            currentCCDTemperature = m->rcvtemp();
            backoffs *= 2;
            if(backoffs > 32) backoffs = 32;
        }
        ntemps++;
        dn->setActTemp(currentCCDTemperature);

        /* If target temperature is higher, then increase current CCD temperature */
        if (fabs(currentCCDTemperature - TemperatureRequest)  < 0.1)
            
        {
            TemperatureNP.s = IPS_OK;
            IDSetNumber(&TemperatureNP, "Target temperature reached.");

            break;
        }

        IDSetNumber(&TemperatureNP, nullptr);

        break;

    case IPS_ALERT:
        break;
    }
    if (!InReadout && !InDownload) {
        int stat = m->rcvstat();
        if (oldstat != stat) {
            LOGF_DEBUG("Status change %d", stat);
        }
        oldstat = stat;
        //    	 if(oldstat == 2 && stat == 0) {
        //    	 		LOG_INFO( "Starting download...");
        //    	 		dn->doDownload();
        //
        //					InReadout = false;
        //					InDownload = true;
        //    	 } else {
        //    	 	  oldstat = stat;
        //    	 }
    }
    SetTimer(POLLMS);
}

/**************************************************************************************
** copy image from internal raw buffer
***************************************************************************************/
void NightscapeCCD::grabImage()
{
    // Let's get a pointer to the frame buffer
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    uint8_t * downbuf = dn->getBuf();
    size_t downsz = dn->getBufImageSize();
    LOGF_DEBUG("image size %ld buf %p", downsz, downbuf);
    // Get width and height
    //int width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
    //int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
    memset(image, 0, PrimaryCCD.getFrameBufferSize());
    dn->copydownload(image, PrimaryCCD.getSubX(), PrimaryCCD.getSubW(), PrimaryCCD.getBinX(), 1, 1);
    IDLog("copied..\n");

    // Fill buffer with random pattern
    //for (int i = 0; i < height; i++)
    //    for (int j = 0; j < width; j++)
    //        image[i * width + j] = rand() % 255;
    dn->freeBuf();
    LOGF_DEBUG( "Download %d lines complete.", dn->getActWriteLines());

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);
}

bool NightscapeCCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure the call is for our device
    if(!strcmp(dev,getDeviceName()))
    {
        if (!strcmp(name, FanSP.name)) {
            const char *actionName = IUFindOnSwitchName(states, names, n);
            if (!strcmp(actionName, FanS[fanspeed -1].name))
            {
                LOGF_INFO("Fan is already %s", FanS[fanspeed -1].label);
                FanSP.s = IPS_IDLE;
                IDSetSwitch(&FanSP, nullptr);
                return true;
            }
            IUUpdateSwitch(&FanSP, states, names, n);
            fanspeed = IUFindOnSwitchIndex(&FanSP) +1;
            LOGF_INFO("Fan is now %s", FanS[fanspeed -1].label);
            FanSP.s = IPS_OK;
            IDSetSwitch(&FanSP, nullptr);
            m->sendfan(fanspeed);
            return true;
        } else if (!strcmp(name, CoolerSP.name)) {
            const char *actionName = IUFindOnSwitchName(states, names, n);
            if (!strcmp(actionName, CoolerS[!cooler].name))
            {
                LOGF_INFO( "Cooler is already %s", CoolerS[!cooler].label);
                CoolerSP.s = IPS_IDLE;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }
            IUUpdateSwitch(&CoolerSP, states, names, n);
            cooler = !IUFindOnSwitchIndex(&CoolerSP);
            LOGF_INFO( "Cooler is now %s", CoolerS[!cooler].label);
            CoolerSP.s = IPS_OK;
            IDSetSwitch(&CoolerSP, nullptr);
            m->sendtemp(setTemp, cooler);
            dn->setActTemp(currentCCDTemperature);
            return true;

        } else if (!strcmp(name, D2xxSP.name)) {
            const char *actionName = IUFindOnSwitchName(states, names, n);
            if (!strcmp(actionName, D2xxS[useD2xx].name))
            {
                LOGF_INFO( "Library is already %s", D2xxS[useD2xx].label);
                D2xxSP.s = IPS_IDLE;
                IDSetSwitch(&D2xxSP, nullptr);
                return true;
            }
            IUUpdateSwitch(&D2xxSP, states, names, n);
#ifdef HAVE_D2XX
            useD2xx = IUFindOnSwitchIndex(&D2xxSP);
            LOGF_INFO( "Library is now %s", D2xxS[useD2xx].label);
#else 
            useD2xx = IUFindOnSwitchIndex(&D2xxSP);
            LOGF_INFO( "Library is now %s", D2xxS[useD2xx].label);
            if(useD2xx == 1) useD2xx = 2;
#endif
            D2xxSP.s = IPS_OK;
            IDSetSwitch(&D2xxSP, nullptr);
            return true;

        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);

}

bool NightscapeCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(name, CamNumNP.name)) {
        if (values[0] < CamNumN[0].min || values[0] > CamNumN[0].max)
        {
            CamNumNP.s = IPS_ALERT;
            LOGF_ERROR("Error: Bad camera number value! Range is [%.1f, %.1f].",
                   CamNumN[0].min, CamNumN[0].max);
            IDSetNumber(&CamNumNP, nullptr);
            return false;
        }

        camnum = values[0];
        CamNumN[0].value = camnum;
        //if (rc == 0)
        //    TemperatureNP.s = IPS_BUSY;
        //else if (rc == 1)
        //    TemperatureNP.s = IPS_OK;
        //else
        //    TemperatureNP.s = IPS_ALERT;

        IDSetNumber(&CamNumNP, nullptr);
        return true;
    }
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);

}

bool NightscapeCCD::saveConfigItems(FILE *fp) {
    currentCCDTemperature = setTemp;
    IUSaveConfigSwitch(fp, &FanSP);
    IUSaveConfigSwitch(fp, &CoolerSP);
    IUSaveConfigNumber(fp, &CamNumNP);
    IUSaveConfigSwitch(fp, &D2xxSP);
    float tTemp = currentCCDTemperature;

    IUSaveConfigNumber(fp, &TemperatureNP);

    currentCCDTemperature = tTemp;

    return INDI::CCD::saveConfigItems(fp);
};

