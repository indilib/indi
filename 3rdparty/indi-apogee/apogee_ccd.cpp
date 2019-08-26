#if 0
Apogee CCD
INDI Driver for Apogee CCDs and Filter Wheels
Copyright (C) 2014 Jasem Mutlaq <mutlaqja AT ikarustech DOT com>

    This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <stdexcept>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <netdb.h>
#include <zlib.h>

#include <memory>

#ifdef OSX_EMBEDED_MODE
#include "Alta.h"
#include "AltaF.h"
#include "Ascent.h"
#include "Aspen.h"
#include "Quad.h"
#include "ApgLogger.h"
#else
#include <libapogee/Alta.h>
#include <libapogee/AltaF.h>
#include <libapogee/Ascent.h>
#include <libapogee/Aspen.h>
#include <libapogee/Quad.h>
#include <libapogee/ApgLogger.h>
#endif

#include <fitsio.h>

#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"
#include "apogee_ccd.h"
#include "config.h"

#define MAX_CCD_TEMP            45   /* Max CCD temperature */
#define MIN_CCD_TEMP            -55  /* Min CCD temperature */
#define MAX_X_BIN               16   /* Max Horizontal binning */
#define MAX_Y_BIN               16   /* Max Vertical binning */
#define MAX_PIXELS              4096 /* Max number of pixels in one dimension */
#define TEMP_THRESHOLD          .25  /* Differential temperature threshold (C)*/
#define NFLUSHES                1    /* Number of times a CCD array is flushed before an exposure */
#define TEMP_UPDATE_THRESHOLD   0.05
#define COOLER_UPDATE_THRESHOLD 0.05

static std::unique_ptr<ApogeeCCD> apogeeCCD(new ApogeeCCD());

void ISGetProperties(const char *dev)
{
    apogeeCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    apogeeCCD->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    apogeeCCD->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    apogeeCCD->ISNewNumber(dev, name, values, names, num);
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
    apogeeCCD->ISSnoopDevice(root);
}

ApogeeCCD::ApogeeCCD() : FilterInterface(this)
{
    setVersion(APOGEE_VERSION_MAJOR, APOGEE_VERSION_MINOR);
}

const char *ApogeeCCD::getDefaultName()
{
    return "Apogee CCD";
}

bool ApogeeCCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&ReadOutS[0], "QUALITY_HIGH", "High Quality", ISS_OFF);
    IUFillSwitch(&ReadOutS[1], "QUALITY_LOW", "Fast", ISS_OFF);
    IUFillSwitchVector(&ReadOutSP, ReadOutS, 2, getDeviceName(), "READOUT_QUALITY", "Readout Speed", OPTIONS_TAB, IP_WO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PortTypeS[0], "USB_PORT", "USB", ISS_ON);
    IUFillSwitch(&PortTypeS[1], "NETWORK_PORT", "Network", ISS_OFF);
    IUFillSwitchVector(&PortTypeSP, PortTypeS, 2, getDeviceName(), "PORT_TYPE", "Port", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&NetworkInfoT[NETWORK_SUBNET], "SUBNET_ADDRESS", "Subnet", "192.168.0.255");
    IUFillText(&NetworkInfoT[NETWORK_ADDRESS], "IP_PORT_ADDRESS", "IP:Port", "");
    IUFillTextVector(&NetworkInfoTP, NetworkInfoT, 2, getDeviceName(), "NETWORK_INFO", "Network", MAIN_CONTROL_TAB,
                     IP_RW, 0, IPS_IDLE);

    IUFillText(&CamInfoT[0], "CAM_NAME", "Name", "");
    IUFillText(&CamInfoT[1], "CAM_FIRMWARE", "Firmware", "");
    IUFillTextVector(&CamInfoTP, CamInfoT, 2, getDeviceName(), "CAM_INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillSwitch(&FanStatusS[FAN_OFF], "FAN_OFF", "Off", ISS_ON);
    IUFillSwitch(&FanStatusS[FAN_SLOW], "FAN_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&FanStatusS[FAN_MED], "FAN_MED", "Medium", ISS_OFF);
    IUFillSwitch(&FanStatusS[FAN_FAST], "FAN_FAST", "Fast", ISS_OFF);
    IUFillSwitchVector(&FanStatusSP, FanStatusS, 4, getDeviceName(), "CCD_FAN", "Fan", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Filter Type
    IUFillSwitch(&FilterTypeS[TYPE_UNKNOWN], "TYPE_UNKNOWN", "No CFW", ISS_ON);
    IUFillSwitch(&FilterTypeS[TYPE_FW50_9R], "TYPE_FW50_9R", "FW50 9R", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_FW50_7S], "TYPE_FW50_7S", "FW50 7S", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_AFW50_10S], "TYPE_AFW50_10S", "AFW50 10S", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_AFW31_17R], "TYPE_AFW31_17R", "AFW31 17R", ISS_OFF);
    IUFillSwitchVector(&FilterTypeSP, FilterTypeS, 5, getDeviceName(), "FILTER_TYPE", "Type", FILTER_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    INDI::FilterInterface::initProperties(FILTER_TAB);

    setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

    addDebugControl();
    addSimulationControl();

    return true;
}

void ApogeeCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineSwitch(&PortTypeSP);
    defineText(&NetworkInfoTP);
    defineSwitch(&FilterTypeSP);

    loadConfig(true, PortTypeSP.name);
    loadConfig(true, NetworkInfoTP.name);
    loadConfig(true, FilterTypeSP.name);
}

bool ApogeeCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineText(&CamInfoTP);
        defineSwitch(&CoolerSP);
        defineNumber(&CoolerNP);
        defineSwitch(&ReadOutSP);
        defineSwitch(&FanStatusSP);
        getCameraParams();

        if (cfwFound)
        {
            INDI::FilterInterface::updateProperties();
            defineText(&FilterInfoTP);
        }

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(CoolerSP.name);
        deleteProperty(CoolerNP.name);
        deleteProperty(ReadOutSP.name);
        deleteProperty(CamInfoTP.name);
        deleteProperty(FanStatusSP.name);

        if (cfwFound)
        {
            INDI::FilterInterface::updateProperties();
            deleteProperty(FilterInfoTP.name);
        }

        rmTimer(timerID);
    }

    return true;
}

bool ApogeeCCD::getCameraParams()
{
    double temperature;
    double pixel_size_x, pixel_size_y;
    long sub_frame_x, sub_frame_y;

    if (isSimulation())
    {
        TemperatureN[0].value = 10;
        IDSetNumber(&TemperatureNP, nullptr);

        IUResetSwitch(&FanStatusSP);
        FanStatusS[2].s = ISS_ON;
        IDSetSwitch(&FanStatusSP, nullptr);

        SetCCDParams(3326, 2504, 16, 5.4, 5.4);

        IUSaveText(&CamInfoT[0], modelStr.c_str());
        IUSaveText(&CamInfoT[1], firmwareRev.c_str());
        IDSetText(&CamInfoTP, nullptr);

        IUResetSwitch(&CoolerSP);
        CoolerS[1].s = ISS_ON;
        IDSetSwitch(&CoolerSP, nullptr);

        imageWidth  = PrimaryCCD.getSubW();
        imageHeight = PrimaryCCD.getSubH();

        int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
        PrimaryCCD.setFrameBufferSize(nbuf);

        return true;
    }

    try
    {
        PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, ApgCam->GetMaxBinRows(), 1);
        PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, ApgCam->GetMaxBinCols(), 1);
        pixel_size_x = ApgCam->GetPixelWidth();
        pixel_size_y = ApgCam->GetPixelHeight();

        IUSaveText(&CamInfoT[0], ApgCam->GetModel().c_str());
        IUSaveText(&CamInfoT[1], firmwareRev.c_str());
        IDSetText(&CamInfoTP, nullptr);

        sub_frame_x = ApgCam->GetMaxImgCols();
        sub_frame_y = ApgCam->GetMaxImgRows();

        temperature = ApgCam->GetTempCcd();

        IUResetSwitch(&CoolerSP);
        Apg::CoolerStatus cStatus = ApgCam->GetCoolerStatus();
        if (cStatus == Apg::CoolerStatus_AtSetPoint || cStatus == Apg::CoolerStatus_RampingToSetPoint)
            CoolerS[0].s = ISS_ON;
        else
            CoolerS[1].s = ISS_ON;

        IDSetSwitch(&CoolerSP, nullptr);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("getCameraParams failed. %s.", err.what());
        return false;
    }

    LOGF_INFO("The CCD Temperature is %f.", temperature);
    TemperatureN[0].value = temperature; /* CCD chip temperatre (degrees C) */
    IDSetNumber(&TemperatureNP, nullptr);

    Apg::FanMode fStatus = Apg::FanMode_Unknown;

    try
    {
        fStatus = ApgCam->GetFanMode();
        LOGF_DEBUG("Fan status: %d", fStatus);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("GetFanMode failed. %s.", err.what());
        return false;
    }

    if (fStatus != Apg::FanMode_Unknown)
    {
        IUResetSwitch(&FanStatusSP);
        FanStatusS[fStatus].s = ISS_ON;
        IDSetSwitch(&FanStatusSP, nullptr);
    }
    else
    {
        FanStatusSP.s = IPS_ALERT;
        LOG_WARN("Fan status is not known.");
    }

    SetCCDParams(sub_frame_x, sub_frame_y, 16, pixel_size_x, pixel_size_y);

    imageWidth  = PrimaryCCD.getSubW();
    imageHeight = PrimaryCCD.getSubH();

    try
    {
        minDuration = ApgCam->GetMinExposureTime();
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("get_MinExposureTime() failed. %s.", err.what());
        return false;
    }

    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

int ApogeeCCD::SetTemperature(double temperature)
{
    // If less than 0.1 of a degree, let's just return OK
    if (fabs(temperature - TemperatureN[0].value) < 0.1)
        return 1;

    activateCooler(true);

    try
    {
        if (isSimulation() == false)
            ApgCam->SetCoolerSetPoint(temperature);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("SetCoolerSetPoint failed. %s.", err.what());
        return -1;
    }

    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool ApogeeCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Filter Type
        if (!strcmp(name, FilterTypeSP.name))
        {
            IUUpdateSwitch(&FilterTypeSP, states, names, n);
            FilterTypeSP.s = IPS_OK;
            IDSetSwitch(&FilterTypeSP, nullptr);
            return true;
        }


        /* Port Type */
        if (!strcmp(name, PortTypeSP.name))
        {
            IUUpdateSwitch(&PortTypeSP, states, names, n);

            PortTypeSP.s = IPS_OK;

            IDSetSwitch(&PortTypeSP, nullptr);

            return true;
        }

        /* Readout Speed */
        if (!strcmp(name, ReadOutSP.name))
        {
            if (IUUpdateSwitch(&ReadOutSP, states, names, n) < 0)
                return false;

            if (ReadOutS[0].s == ISS_ON)
            {
                try
                {
                    if (isSimulation() == false)
                        ApgCam->SetCcdAdcSpeed(Apg::AdcSpeed_Normal);
                }
                catch (std::runtime_error &err)
                {
                    IUResetSwitch(&ReadOutSP);
                    ReadOutSP.s = IPS_ALERT;
                    LOGF_ERROR("SetCcdAdcSpeed failed. %s.", err.what());
                    IDSetSwitch(&ReadOutSP, nullptr);
                    return false;
                }
            }
            else
            {
                try
                {
                    if (isSimulation() == false)
                        ApgCam->SetCcdAdcSpeed(Apg::AdcSpeed_Fast);
                }
                catch (std::runtime_error &err)
                {
                    IUResetSwitch(&ReadOutSP);
                    ReadOutSP.s = IPS_ALERT;
                    LOGF_ERROR("SetCcdAdcSpeed failed. %s.", err.what());
                    IDSetSwitch(&ReadOutSP, nullptr);
                    return false;
                }

                ReadOutSP.s = IPS_OK;
                IDSetSwitch(&ReadOutSP, nullptr);
            }

            ReadOutSP.s = IPS_OK;
            IDSetSwitch(&ReadOutSP, nullptr);
            return true;
        }

        // Fan Speed
        if (!strcmp(name, FanStatusSP.name))
        {
            if (IUUpdateSwitch(&FanStatusSP, states, names, n) < 0)
                return false;

            ApgCam->SetFanMode(static_cast<Apg::FanMode>(IUFindOnSwitchIndex(&FanStatusSP)));
            FanStatusSP.s = IPS_OK;
            IDSetSwitch(&FanStatusSP, nullptr);
            return true;
        }

        /* Cooler */
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
                return false;

            if (CoolerS[0].s == ISS_ON)
                activateCooler(true);
            else
                activateCooler(false);

            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool ApogeeCCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, FilterNameTP->name))
        {
            INDI::FilterInterface::processText(dev, name, texts, names, n);
            return true;
        }

        if (!strcmp(NetworkInfoTP.name, name))
        {
            IUUpdateText(&NetworkInfoTP, texts, names, n);

            subnet = std::string(NetworkInfoT[NETWORK_SUBNET].text);

            if (strlen(NetworkInfoT[NETWORK_ADDRESS].text) > 0)
            {
                char ip[16];
                int port;
                int rc = sscanf(NetworkInfoT[NETWORK_ADDRESS].text, "%[^:]:%d", ip, &port);

                if (rc == 2)
                    NetworkInfoTP.s = IPS_OK;
                else
                {
                    LOG_ERROR("Invalid format. Format must be IP:Port (e.g. 192.168.1.1:80)");
                    NetworkInfoTP.s = IPS_ALERT;
                }
            }
            else
                NetworkInfoTP.s = IPS_OK;

            IDSetText(&NetworkInfoTP, nullptr);

            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool ApogeeCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, FilterSlotNP.name))
        {
            INDI::FilterInterface::processNumber(dev, name, values, names, n);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ApogeeCCD::StartExposure(float duration)
{
    ExposureRequest = duration;

    imageFrameType = PrimaryCCD.getFrameType();

    if (imageFrameType == INDI::CCDChip::BIAS_FRAME)
    {
        ExposureRequest = minDuration;
        LOGF_INFO("Bias Frame (s) : %.3f", ExposureRequest);
    }

    if (isSimulation() == false)
        ApgCam->SetImageCount(1);

    /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.*/
    if (imageFrameType == INDI::CCDChip::BIAS_FRAME || imageFrameType == INDI::CCDChip::DARK_FRAME)
    {
        try
        {
            if (isSimulation() == false)
            {
                ApgCam->StartExposure(ExposureRequest, false);
                PrimaryCCD.setExposureDuration(ExposureRequest);
            }
        }
        catch (std::runtime_error &err)
        {
            LOGF_ERROR("StartExposure() failed. %s.", err.what());
            return false;
        }
    }
    else if (imageFrameType == INDI::CCDChip::LIGHT_FRAME || imageFrameType == INDI::CCDChip::FLAT_FRAME)
    {
        try
        {
            if (isSimulation() == false)
            {
                ApgCam->StartExposure(ExposureRequest, true);
                PrimaryCCD.setExposureDuration(ExposureRequest);
            }
        }
        catch (std::runtime_error &err)
        {
            LOGF_ERROR("StartExposure() failed. %s.", err.what());
            return false;
        }
    }

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;
    return true;
}

bool ApogeeCCD::AbortExposure()
{
    try
    {
        if (isSimulation() == false)
            ApgCam->StopExposure(false);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("AbortExposure() failed. %s.", err.what());
        return false;
    }

    InExposure = false;
    return true;
}

float ApogeeCCD::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

bool ApogeeCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if (InExposure == true)
    {
        LOG_ERROR("Cannot change CCD frame while exposure is in progress.");
        return false;
    }

    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long x_2 = x_1 + (w / PrimaryCCD.getBinX());
    long y_2 = y_1 + (h / PrimaryCCD.getBinY());

    if (x_2 > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        LOGF_ERROR("Error: invalid width requested %ld", x_2);
        return false;
    }
    else if (y_2 > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        LOGF_ERROR("Error: invalid height request %ld", y_2);
        return false;
    }

    LOGF_DEBUG("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);

    imageWidth  = x_2 - x_1;
    imageHeight = y_2 - y_1;

    try
    {
        if (isSimulation() == false)
        {
            ApgCam->SetRoiStartCol(x_1);
            ApgCam->SetRoiStartRow(y_1);
            ApgCam->SetRoiNumCols(imageWidth);
            ApgCam->SetRoiNumRows(imageHeight);
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Setting image area failed. %s.", err.what());
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);
    int nbuf = (imageWidth * imageHeight * PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool ApogeeCCD::UpdateCCDBin(int binx, int biny)
{
    if (InExposure == true)
    {
        LOG_ERROR("Cannot change CCD binning while exposure is in progress.");
        return false;
    }

    try
    {
        if (isSimulation() == false)
            ApgCam->SetRoiBinCol(binx);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("SetRoiBinCol failed. %s.", err.what());
        return false;
    }

    try
    {
        if (isSimulation() == false)
            ApgCam->SetRoiBinRow(biny);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("SetRoiBinRow failed. %s.", err.what());
        return false;
    }

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

int ApogeeCCD::grabImage()
{
    std::vector<uint16_t> pImageData;
    uint16_t *image = reinterpret_cast<uint16_t*>(PrimaryCCD.getFrameBuffer());

    try
    {
        std::unique_lock<std::mutex> guard(ccdBufferLock);
        if (isSimulation())
        {
            for (int i = 0; i < imageHeight; i++)
                for (int j = 0; j < imageWidth; j++)
                    image[i * imageWidth + j] = rand() % 65535;
        }
        else
        {
            ApgCam->GetImage(pImageData);
            imageWidth  = ApgCam->GetRoiNumCols();
            imageHeight = ApgCam->GetRoiNumRows();
            copy(pImageData.begin(), pImageData.end(), image);
        }
        guard.unlock();
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("GetImage failed. %s.", err.what());
        return -1;
    }

    ExposureComplete(&PrimaryCCD);

    LOG_INFO("Download complete.");

    return 0;
}

///////////////////////////
// MAKE	  TOKENS
std::vector<std::string> ApogeeCCD::MakeTokens(const std::string &str, const std::string &separator)
{
    std::vector<std::string> returnVector;
    std::string::size_type start = 0;
    std::string::size_type end   = 0;

    while ((end = str.find(separator, start)) != std::string::npos)
    {
        returnVector.push_back(str.substr(start, end - start));
        start = end + separator.size();
    }

    returnVector.push_back(str.substr(start));

    return returnVector;
}

///////////////////////////
//	GET    ITEM    FROM     FIND       STR
std::string ApogeeCCD::GetItemFromFindStr(const std::string &msg, const std::string &item)
{
    //search the single device input string for the requested item
    std::vector<std::string> params = MakeTokens(msg, ",");
    std::vector<std::string>::iterator iter;

    for (iter = params.begin(); iter != params.end(); ++iter)
    {
        if (std::string::npos != (*iter).find(item))
        {
            std::string result = MakeTokens((*iter), "=").at(1);

            return result;
        }
    } //for

    std::string noOp;
    return noOp;
}

////////////////////////////
//	GET		USB  ADDRESS
std::string ApogeeCCD::GetUsbAddress(const std::string &msg)
{
    return GetItemFromFindStr(msg, "address=");
}

////////////////////////////
//	GET		IP Address
std::string ApogeeCCD::GetIPAddress(const std::string &msg)
{
    std::string addr = GetItemFromFindStr(msg, "address=");
    return addr;
}

////////////////////////////
//	GET		ETHERNET  ADDRESS
std::string ApogeeCCD::GetEthernetAddress(const std::string &msg)
{
    std::string addr = GetItemFromFindStr(msg, "address=");
    addr.append(":");
    addr.append(GetItemFromFindStr(msg, "port="));
    return addr;
}
////////////////////////////
//	GET		ID
uint16_t ApogeeCCD::GetID(const std::string &msg)
{
    std::string str = GetItemFromFindStr(msg, "id=");
    uint16_t id     = 0;
    std::stringstream ss;
    ss << std::hex << std::showbase << str.c_str();
    ss >> id;

    return id;
}

////////////////////////////
//	GET		FRMWR       REV
uint16_t ApogeeCCD::GetFrmwrRev(const std::string &msg)
{
    std::string str = GetItemFromFindStr(msg, "firmwareRev=");

    uint16_t rev = 0;
    std::stringstream ss;
    ss << std::hex << std::showbase << str.c_str();
    ss >> rev;

    return rev;
}

////////////////////////////
// Do we have a camera?
bool ApogeeCCD::IsDeviceCamera(const std::string &msg)
{
    std::string str = GetItemFromFindStr(msg, "deviceType=");

    return (0 == str.compare("camera") ? true : false);
}

////////////////////////////
//	        IS  	ASCENT
bool ApogeeCCD::IsAscent(const std::string &msg)
{
    std::string model = GetItemFromFindStr(msg, "model=");
    std::string ascent("Ascent");
    return (0 == model.compare(0, ascent.size(), ascent) ? true : false);
}

////////////////////////////
//      PRINT       INFO
void ApogeeCCD::printInfo(const std::string &model, const uint16_t maxImgRows, const uint16_t maxImgCols)
{
    std::cout << "Cam Info: " << std::endl;
    std::cout << "model = " << model.c_str() << std::endl;
    std::cout << "max # imaging rows = " << maxImgRows;
    std::cout << "\tmax # imaging cols = " << maxImgCols << std::endl;
}

////////////////////////////
//		CHECK	STATUS
void ApogeeCCD::checkStatus(const Apg::Status status)
{
    switch (status)
    {
        case Apg::Status_ConnectionError:
        {
            std::string errMsg("Status_ConnectionError");
            std::runtime_error except(errMsg);
            throw except;
        }

        case Apg::Status_DataError:
        {
            std::string errMsg("Status_DataError");
            std::runtime_error except(errMsg);
            throw except;
        }

        case Apg::Status_PatternError:
        {
            std::string errMsg("Status_PatternError");
            std::runtime_error except(errMsg);
            throw except;
        }

        case Apg::Status_Idle:
        {
            std::string errMsg("Status_Idle");
            std::runtime_error except(errMsg);
            throw except;
        }

        default:
            //no op on purpose
            break;
    }
}

CamModel::PlatformType ApogeeCCD::GetModel(const std::string &msg)
{
    modelStr = GetItemFromFindStr(msg, "model=");

    return CamModel::GetPlatformType(modelStr);
}

//bool ApogeeCCD::Connect()
//{
//    cameraFound = cfwFound = false;

//    bool rcCamera = connectCamera();

//    bool rcCFW = true;

//    if (IUFindOnSwitchIndex(&FilterTypeSP) != TYPE_UNKNOWN)
//        rcCFW = connectCFW();

//    return rcCamera && rcCFW;
//}

bool ApogeeCCD::Connect()
{
    cameraFound = cfwFound = false;

    std::string msg, addr, token, token_ip;
    std::string cameraInfo, cfwInfo;
    std::string delimiter = "</d>";
    size_t pos = 0;

    bool findCFW = IUFindOnSwitchIndex(&FilterTypeSP) != TYPE_UNKNOWN;

    if (!findCFW)
        LOG_INFO("Searching for Apogee CCD...");
    else
        LOG_INFO("Searching for Apogee CCD & CFW...");

    // USB
    if (PortTypeS[0].s == ISS_ON)
    {
        // Simulation
        if (isSimulation())
        {
            msg  = std::string("<d>address=0,interface=usb,deviceType=camera,id=0x49,firmwareRev=0x21,model=AltaU-"
                               "4020ML,interfaceStatus=NA</d><d>address=1,interface=usb,model=Filter "
                               "Wheel,deviceType=filterWheel,id=0xFFFF,firmwareRev=0xFFEE</d>");
            addr = GetUsbAddress(msg);
        }
        else
        {
            ioInterface = std::string("usb");
            FindDeviceUsb lookUsb;
            try
            {
                msg  = lookUsb.Find();
            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error finding USB device: %s", err.what());
                return false;
            }
        }

        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);
            LOGF_DEBUG("Checking device: %s", token.c_str());

            if (cameraFound == false)
            {
                cameraFound = IsDeviceCamera(token);
                if (cameraFound)
                    cameraInfo = token;
            }

            if (findCFW && cfwFound == false)
            {
                cfwFound = IsDeviceFilterWheel(token);
                if (cfwFound)
                    cfwInfo = token;
            }

            // Exit if camera and optionally cfw are found
            // if no cfw is required, then just camera suffice
            if (cameraFound && (findCFW == false || (findCFW && cfwFound)))
                break;

            msg.erase(0, pos + delimiter.length());
        }
    }
    // Ethernet
    else
    {
        ioInterface = std::string("ethernet");
        FindDeviceEthernet look4cam;
        char ip[32];
        int port;

        // Simulation
        if (isSimulation())
        {
            msg = std::string("<d>address=192.168.1.20,interface=ethernet,port=80,mac=0009510000FF,deviceType=camera,"
                              "id=0xfeff,firmwareRev=0x0,model=AltaU-4020ML</d>"
                              "<d>address=192.168.1.21,interface=ethernet,port=80,mac=0009510000FF,deviceType=camera,"
                              "id=0xfeff,firmwareRev=0x0,model=AltaU-4020ML</d>"
                              "<d>address=192.168.2.22,interface=ethernet,port=80,mac=0009510000FF,deviceType=camera,"
                              "id=0xfeff,firmwareRev=0x0,model=AltaU-4020ML</d>");
        }
        else
        {
            try
            {
                msg = look4cam.Find(subnet);
                // This can cause a crash
                //LOGF_DEBUG("Network search result: %s", msg.c_str());
            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error getting network address: %s", err.what());
                return false;
            }
        }

        int rc = 0;

        // Check if we have IP:Port format
        if (NetworkInfoT[NETWORK_ADDRESS].text != nullptr && strlen(NetworkInfoT[NETWORK_ADDRESS].text) > 0)
            rc = sscanf(NetworkInfoT[NETWORK_ADDRESS].text, "%[^:]:%d", ip, &port);

        // If we have IP:Port, then let's skip all entries that does not have our desired IP address.
        // If we have IP:Port, then let's skip all entries that does not have our desired IP address.
        addr = NetworkInfoT[NETWORK_ADDRESS].text;
        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);

            if (cameraFound == false && IsDeviceCamera(token))
            {
                if (rc == 2)
                {
                    addr = GetEthernetAddress(token);
                    IUSaveText(&NetworkInfoT[NETWORK_ADDRESS], addr.c_str());
                    LOGF_INFO("Detected camera at %s", addr.c_str());
                    IDSetText(&NetworkInfoTP, nullptr);
                    cameraFound = true;
                    cameraInfo = token;
                }
                else
                {
                    token_ip = GetIPAddress(token);
                    addr = GetEthernetAddress(token);
                    LOGF_DEBUG("Checking %s (%s) for IP %s", token.c_str(), token_ip.c_str(), ip);
                    if (token_ip == ip)
                    {
                        LOGF_DEBUG("IP matched (%s).", msg.c_str());
                        cameraFound = true;
                        cameraInfo = token;
                    }
                }
            }
            else if (findCFW && cfwFound == false && IsDeviceFilterWheel(token))
            {
                if (rc == 2)
                {
                    addr = GetEthernetAddress(token);
                    LOGF_INFO("Detected filter wheel at %s", addr.c_str());
                    cfwFound = true;
                    cfwInfo = token;
                }
                else
                {
                    token_ip = GetIPAddress(token);
                    addr = GetEthernetAddress(token);
                    LOGF_DEBUG("Checking %s (%s) for IP %s", token.c_str(), token_ip.c_str(), ip);
                    if (token_ip == ip)
                    {
                        LOGF_DEBUG("IP matched (%s).", msg.c_str());
                        cfwFound = true;
                        cfwInfo = token;
                    }
                }
            }

            // Exit if camera and optionally cfw are found
            // if no cfw is required, then just camera suffice
            if (cameraFound && (findCFW == false || (findCFW && cfwFound)))
                break;

            msg.erase(0, pos + delimiter.length());
        }
    }

    if (cameraFound == false)
    {
        LOG_ERROR("Unable to find Apogee camera attached. Please check connection and power and try again.");
        return false;
    }

    uint16_t id       = GetID(cameraInfo);
    uint16_t frmwrRev = GetFrmwrRev(cameraInfo);

    char firmwareStr[16];
    snprintf(firmwareStr, 16, "0x%X", frmwrRev);
    firmwareRev = std::string(firmwareStr);

    model = GetModel(cameraInfo);
    addr = GetUsbAddress(cameraInfo);

    LOGF_INFO("Model: %s ID: %d Address: %s Firmware: %s",
              GetItemFromFindStr(cameraInfo, "model=").c_str(), id, addr.c_str(), firmwareRev.c_str());

    switch (model)
    {
        case CamModel::ALTAU:
        case CamModel::ALTAE:
            ApgCam.reset(new Alta());
            break;

        case CamModel::ASPEN:
            ApgCam.reset(new Aspen());
            break;

        case CamModel::ALTAF:
            ApgCam.reset(new AltaF());
            break;

        case CamModel::ASCENT:
            ApgCam.reset(new Ascent());
            break;

        case CamModel::QUAD:
            ApgCam.reset(new Quad());
            break;

        default:
            LOGF_ERROR("Model %s is not supported by the INDI Apogee driver.",
                       GetItemFromFindStr(cameraInfo, "model=").c_str());
            return false;
    }

    try
    {
        if (isSimulation() == false)
        {
            ApgCam->OpenConnection(ioInterface, addr, frmwrRev, id);
            ApgCam->Init();
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error opening camera: %s", err.what());
        return false;
    }

    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER;
    SetCCDCapability(cap);

    // If we do not need to find CFW, we're done.
    if (findCFW == false)
    {
        LOG_INFO("Camera is online. Retrieving basic data.");
        return true;
    }

    LOG_INFO("Camera is online.");

    if (cfwFound == false)
    {
        LOG_ERROR("Unable to find Apogee Filter Wheels attached. Please check connection and power and try again.");
        return false;
    }

    ApgCFW.reset(new ApogeeFilterWheel());
    addr = GetUsbAddress(cfwInfo);

    try
    {
        if (isSimulation() == false)
        {
            ApogeeFilterWheel::Type type = static_cast<ApogeeFilterWheel::Type>(IUFindOnSwitchIndex(&FilterTypeSP));
            LOGF_DEBUG("Opening connection to CFW type: %d @ address: %s", type, addr.c_str());
            ApgCFW->Init(type, addr);
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error opening CFW: %s", err.what());
        return false;
    }

    if (isSimulation())
        FilterSlotN[0].max = 5;
    else
    {
        try
        {
            FilterSlotN[0].max = ApgCFW->GetMaxPositions();
        }
        catch(std::runtime_error &err)
        {
            LOGF_ERROR("Failed to retrieve maximum filter position: %s", err.what());
            ApgCFW->Close();
            return false;
        }
    }

    if (isSimulation())
    {
        IUSaveText(&FilterInfoT[INFO_NAME], "Simulated Filter");
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], "123456");
    }
    else
    {
        IUSaveText(&FilterInfoT[INFO_NAME], ApgCFW->GetName().c_str());
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], ApgCFW->GetUsbFirmwareRev().c_str());
    }

    FilterInfoTP.s = IPS_OK;

    LOG_INFO("CFW is online.");

    return true;
}

bool ApogeeCCD::Disconnect()
{
    try
    {
        if (isSimulation() == false)
        {
            ApgCam->CloseConnection();
            if (cfwFound)
                ApgCFW->Close();
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error: Close camera failed. %s.", err.what());
        return false;
    }

    LOG_INFO("Camera is offline.");
    return true;
}

void ApogeeCCD::activateCooler(bool enable)
{
    bool coolerOn;
    bool coolerSet = false;

    if (isSimulation())
        return;

    try
    {
        coolerOn = ApgCam->IsCoolerOn();
        if ((enable && coolerOn == false) || (!enable && coolerOn == true))
        {
            ApgCam->SetCooler(enable);
            coolerSet = true;
        }
    }
    catch (std::runtime_error &err)
    {
        CoolerSP.s   = IPS_ALERT;
        CoolerS[0].s = ISS_OFF;
        CoolerS[1].s = ISS_ON;
        LOGF_ERROR("Error: SetCooler failed. %s.", err.what());
        IDSetSwitch(&CoolerSP, nullptr);
        return;
    }

    /* Success! */
    CoolerS[0].s = enable ? ISS_ON : ISS_OFF;
    CoolerS[1].s = enable ? ISS_OFF : ISS_ON;
    CoolerSP.s   = IPS_OK;
    if (coolerSet)
        LOG_INFO(enable ? "Cooler ON" : "Cooler Off");
    IDSetSwitch(&CoolerSP, nullptr);
}

void ApogeeCCD::TimerHit()
{
    long timeleft;
    double ccdTemp;
    double coolerPower;

    if (isConnected() == false)
        return;

    if (InExposure)
    {
        timeleft = CalcTimeLeft(ExpStart, ExposureRequest);

        if (timeleft < 1)
        {
            if (isSimulation() == false)
            {
                Apg::Status status = ApgCam->GetImagingStatus();

                while (status != Apg::Status_ImageReady)
                {
                    usleep(250000);
                    status = ApgCam->GetImagingStatus();
                }
            }

            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            /* grab and save image */
            grabImage();
        }
        else
        {
            LOGF_DEBUG("Image not ready, time left %ld\n", timeleft);
            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:

            try
            {
                if (isSimulation())
                    ccdTemp = TemperatureN[0].value;
                else
                    ccdTemp = ApgCam->GetTempCcd();
            }
            catch (std::runtime_error &err)
            {
                TemperatureNP.s = IPS_IDLE;
                LOGF_ERROR("GetTempCcd failed. %s.", err.what());
                IDSetNumber(&TemperatureNP, nullptr);
                return;
            }

            if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_UPDATE_THRESHOLD)
            {
                TemperatureN[0].value = ccdTemp;
                IDSetNumber(&TemperatureNP, nullptr);
            }
            break;

        case IPS_BUSY:

            try
            {
                if (isSimulation())
                    ccdTemp = TemperatureN[0].value;
                else
                    ccdTemp = ApgCam->GetTempCcd();
            }
            catch (std::runtime_error &err)
            {
                TemperatureNP.s = IPS_ALERT;
                LOGF_ERROR("GetTempCcd failed. %s.", err.what());
                IDSetNumber(&TemperatureNP, nullptr);
                return;
            }

            if (fabs(TemperatureN[0].value - ccdTemp) <= TEMP_THRESHOLD)
                TemperatureNP.s = IPS_OK;

            TemperatureN[0].value = ccdTemp;
            IDSetNumber(&TemperatureNP, nullptr);
            break;

        case IPS_ALERT:
            break;
    }

    switch (CoolerNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:

            try
            {
                if (isSimulation())
                    coolerPower = 50;
                else
                    coolerPower = ApgCam->GetCoolerDrive();
            }
            catch (std::runtime_error &err)
            {
                CoolerNP.s = IPS_IDLE;
                LOGF_ERROR("GetCoolerDrive failed. %s.", err.what());
                IDSetNumber(&CoolerNP, nullptr);
                return;
            }

            if (fabs(CoolerN[0].value - coolerPower) >= COOLER_UPDATE_THRESHOLD)
            {
                if (coolerPower > 0)
                    CoolerNP.s = IPS_BUSY;

                CoolerN[0].value = coolerPower;
                IDSetNumber(&CoolerNP, nullptr);
            }

            break;

        case IPS_BUSY:

            try
            {
                if (isSimulation())
                    coolerPower = 50;
                else
                    coolerPower = ApgCam->GetCoolerDrive();
            }
            catch (std::runtime_error &err)
            {
                CoolerNP.s = IPS_ALERT;
                LOGF_ERROR("GetCoolerDrive failed. %s.", err.what());
                IDSetNumber(&CoolerNP, nullptr);
                return;
            }

            if (fabs(CoolerN[0].value - coolerPower) >= COOLER_UPDATE_THRESHOLD)
            {
                if (coolerPower <= 0)
                    CoolerNP.s = IPS_IDLE;

                CoolerN[0].value = coolerPower;
                IDSetNumber(&CoolerNP, nullptr);
            }

            break;

        case IPS_ALERT:
            break;
    }

    if (FilterSlotNP.s == IPS_BUSY)
    {
        try
        {
            ApogeeFilterWheel::Status status = ApgCFW->GetStatus();
            if (status == ApogeeFilterWheel::READY)
            {
                CurrentFilter = TargetFilter;
                SelectFilterDone(CurrentFilter);
            }
        }
        catch (std::runtime_error &err)
        {
            LOGF_ERROR("Failed to get CFW status: %s", err.what());
            FilterSlotNP.s = IPS_ALERT;
            IDSetNumber(&FilterSlotNP, nullptr);
        }
    }

    SetTimer(POLLMS);
    return;
}

void ApogeeCCD::debugTriggered(bool enabled)
{
    ApgLogger::Instance().SetLogLevel(enabled ? ApgLogger::LEVEL_DEBUG : ApgLogger::LEVEL_RELEASE);
}

bool ApogeeCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &PortTypeSP);
    IUSaveConfigText(fp, &NetworkInfoTP);
    if (FanStatusSP.s != IPS_ALERT)
        IUSaveConfigSwitch(fp, &FanStatusSP);

    if (cfwFound)
    {
        INDI::FilterInterface::saveConfigItems(fp);
        IUSaveConfigSwitch(fp, &FilterTypeSP);
    }


    return true;
}

int ApogeeCCD::QueryFilter()
{
    try
    {
        CurrentFilter = ApgCFW->GetPosition();
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Failed to query filter: %s", err.what());
        FilterSlotNP.s = IPS_ALERT;
        IDSetNumber(&FilterSlotNP, nullptr);
        return -1;
    }

    return CurrentFilter;
}

bool ApogeeCCD::SelectFilter(int position)
{
    try
    {
        ApgCFW->SetPosition(position);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Failed to set filter: %s", err.what());
        FilterSlotNP.s = IPS_ALERT;
        IDSetNumber(&FilterSlotNP, nullptr);
        return false;
    }

    TargetFilter = position;
    return true;
}

////////////////////////////
// Is CFW
bool ApogeeCCD::IsDeviceFilterWheel(const std::string &msg)
{
    std::string str = GetItemFromFindStr(msg, "deviceType=");

    return (0 == str.compare("filterWheel") ? true : false);
}

#if 0
bool ApogeeCCD::connectCFW()
{
    std::string msg;
    std::string addr;

    LOG_INFO("Attempting to find Apogee CFW...");

    // USB
    if (PortTypeS[PORT_USB].s == ISS_ON)
    {
        // Simulation
        if (isSimulation())
        {
            msg  = std::string("<d>address=1,interface=usb,model=Filter "
                               "Wheel,deviceType=filterWheel,id=0xFFFF,firmwareRev=0xFFEE</d>");
            addr = GetUsbAddress(msg);
        }
        else
        {
            ioInterface = std::string("usb");
            FindDeviceUsb lookUsb;
            try
            {
                msg  = lookUsb.Find();
            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error finding USB device: %s", err.what());
                return false;
            }
        }

        std::string delimiter = "</d>";
        size_t pos = 0;
        std::string token, token_ip;
        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);
            LOGF_DEBUG("Checking device: %s", token.c_str());

            cfwFound = IsDeviceFilterWheel(token);
            if (cfwFound)
            {
                msg = token;
                break;
            }

            msg.erase(0, pos + delimiter.length());
        }
    }
    // Ethernet
    else
    {
        ioInterface = std::string("ethernet");
        FindDeviceEthernet look4Filter;
        char ip[32];
        int port;

        // Simulation
        if (isSimulation())
        {
            msg  = std::string("<d>address=1,interface=usb,model=Filter "
                               "Wheel,deviceType=filterWheel,id=0xFFFF,firmwareRev=0xFFEE</d>");
        }
        else
        {
            try
            {
                msg = look4Filter.Find(subnet);
                // FIXME this can cause a crash
                //LOGF_DEBUG("Network search result: %s", msg.c_str());
            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error getting network address: %s", err.what());
                return false;
            }
        }

        int rc = 0;

        // Check if we have IP:Port format
        if (NetworkInfoT[NETWORK_ADDRESS].text != nullptr && strlen(NetworkInfoT[NETWORK_ADDRESS].text) > 0)
            rc = sscanf(NetworkInfoT[NETWORK_ADDRESS].text, "%[^:]:%d", ip, &port);

        // If we have IP:Port, then let's skip all entries that does not have our desired IP address.
        addr = NetworkInfoT[NETWORK_ADDRESS].text;

        std::string delimiter = "</d>";
        size_t pos = 0;
        std::string token, token_ip;
        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);

            if (IsDeviceFilterWheel(token))
            {
                if (rc == 2)
                {
                    addr = GetEthernetAddress(token);
                    IUSaveText(&NetworkInfoT[NETWORK_ADDRESS], addr.c_str());
                    LOGF_INFO("Detected filter at %s", addr.c_str());
                    IDSetText(&NetworkInfoTP, nullptr);
                    cfwFound = true;
                    msg = token;
                    break;
                }
                else
                {
                    token_ip = GetIPAddress(token);
                    LOGF_DEBUG("Checking %s (%s) for IP %s", token.c_str(), token_ip.c_str(), ip);
                    if (token_ip == ip)
                    {
                        msg = token;
                        LOGF_DEBUG("IP matched (%s).", msg.c_str());
                        addr = GetEthernetAddress(token);
                        cfwFound = true;
                        break;
                    }
                }
            }

            msg.erase(0, pos + delimiter.length());
        }
    }

    if (cfwFound == false)
    {
        LOG_ERROR("Unable to find Apogee Filter Wheels attached. Please check connection and power and try again.");
        return false;
    }

    //    //uint16_t id       = GetID(msg);
    //    uint16_t frmwrRev = GetFrmwrRev(msg);
    //    char firmwareStr[16]={0};
    //    snprintf(firmwareStr, 16, "0x%X", frmwrRev);
    //    firmwareRev = std::string(firmwareStr);
    //model = GetModel(msg);

    try
    {
        if (isSimulation() == false)
        {
            ApogeeFilterWheel::Type type = static_cast<ApogeeFilterWheel::Type>(IUFindOnSwitchIndex(&FilterTypeSP));
            LOGF_DEBUG("Opening connection to CFW type: %d @ address: %s", type, addr.c_str());
            ApgCFW->Init(type, addr);
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error opening CFW: %s", err.what());
        return false;
    }

    if (isSimulation())
        FilterSlotN[0].max = 5;
    else
    {
        try
        {
            FilterSlotN[0].max = ApgCFW->GetMaxPositions();
        }
        catch(std::runtime_error &err)
        {
            LOGF_ERROR("Failed to retrieve maximum filter position: %s", err.what());
            ApgCFW->Close();
            return false;
        }
    }

    if (isSimulation())
    {
        IUSaveText(&FilterInfoT[INFO_NAME], "Simulated Filter");
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], "123456");
    }
    else
    {
        IUSaveText(&FilterInfoT[INFO_NAME], ApgCFW->GetName().c_str());
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], ApgCFW->GetUsbFirmwareRev().c_str());
    }

    FilterInfoTP.s = IPS_OK;

    LOG_INFO("CFW is online.");
    return true;
}
#endif
