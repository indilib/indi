/*******************************************************************************
 Copyright(c) 2013-2016 CloudMakers, s. r. o. All rights reserved.
 Copyright(c) 2017 Marco Gulino <marco.gulino@gmai.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 *******************************************************************************/

#include "agent_imager.h"
#include "indistandardproperty.h"

#include <cstring>
#include <algorithm>
#include <memory>

#include "group.h"

#define DOWNLOAD_TAB "Download images"
#define IMAGE_NAME   "%s/%s_%d_%03d%s"
#define IMAGE_PREFIX "_TMP_"


#define GROUP_PREFIX     "GROUP_"
#define GROUP_PREFIX_LEN 6


const std::string Imager::DEVICE_NAME = "Imager Agent";
std::shared_ptr<Imager> imager(new Imager());

// Imager ----------------------------------------------------------------------------

Imager::Imager()
{
    setVersion(1, 2);
    groups.resize(MAX_GROUP_COUNT);
    int i=0;
    std::generate(groups.begin(), groups.end(), [this, &i] { return std::make_shared<Group>(i++, this); });
}

bool Imager::isRunning()
{
    return ProgressNP.s == IPS_BUSY;
}

bool Imager::isCCDConnected()
{
    return StatusL[0].s == IPS_OK;
}

bool Imager::isFilterConnected()
{
    return StatusL[1].s == IPS_OK;
}

std::shared_ptr<Group> Imager::getGroup(int index) const {
    if(index > -1 && index <= maxGroup)
        return groups[index];
    return {};
}

std::shared_ptr<Group> Imager::currentGroup() const {
    return getGroup(group - 1); 
}



std::shared_ptr<Group> Imager::nextGroup() const {
    return getGroup(group);
}

void Imager::initiateNextFilter()
{
    if (!isRunning())
        return;

    if (group > 0 && image > 0 && group <= maxGroup && image <= maxImage)
    {
        int filterSlot = currentGroup()->filterSlot();

        if (!isFilterConnected())
        {
            if (filterSlot != 0)
            {
                ProgressNP.s = IPS_ALERT;
                IDSetNumber(&ProgressNP, "Filter wheel is not connected");
                return;
            }
            else
            {
                initiateNextCapture();
            }
        }
        else if (filterSlot != 0 && FilterSlotN[0].value != filterSlot)
        {
            FilterSlotN[0].value = filterSlot;
            sendNewNumber(&FilterSlotNP);
            LOGF_DEBUG("Group %d of %d, image %d of %d, filer %d, filter set initiated on %s",
                   group, maxGroup, image, maxImage, (int)FilterSlotN[0].value, FilterSlotNP.device);
        }
        else
        {
            initiateNextCapture();
        }
    }
}

void Imager::initiateNextCapture()
{
    if (isRunning())
    {
        if (group > 0 && image > 0 && group <= maxGroup && image <= maxImage)
        {
            if (!isCCDConnected())
            {
                ProgressNP.s = IPS_ALERT;
                IDSetNumber(&ProgressNP, "CCD is not connected");
                return;
            }
            CCDImageBinN[0].value = currentGroup()->binning();
            CCDImageBinN[1].value = currentGroup()->binning();
            sendNewNumber(&CCDImageBinNP);
            CCDImageExposureN[0].value = currentGroup()->exposure();
            sendNewNumber(&CCDImageExposureNP);
            IUSaveText(&CCDUploadSettingsT[0], ImageNameT[0].text);
            IUSaveText(&CCDUploadSettingsT[1], "_TMP_");
            sendNewSwitch(&CCDUploadSP);
            sendNewText(&CCDUploadSettingsTP);
            LOGF_DEBUG("Group %d of %d, image %d of %d, duration %.1fs, binning %d, capture initiated on %s", group,
                   maxGroup, image, maxImage, CCDImageExposureN[0].value, (int)CCDImageBinN[0].value,
                   CCDImageExposureNP.device);
        }
    }
}

void Imager::startBatch()
{
    LOG_DEBUG("Batch started");
    ProgressN[0].value = group = 1;
    ProgressN[1].value = image = 1;
    maxImage                   = currentGroup()->count();
    ProgressNP.s               = IPS_BUSY;
    IDSetNumber(&ProgressNP, nullptr);
    initiateNextFilter();
}

void Imager::abortBatch()
{
    ProgressNP.s = IPS_ALERT;
    IDSetNumber(&ProgressNP, "Batch aborted");
}

void Imager::batchDone()
{
    ProgressNP.s = IPS_OK;
    IDSetNumber(&ProgressNP, "Batch done");
}

void Imager::initiateDownload()
{
    int group = (int)DownloadN[0].value;
    int image = (int)DownloadN[1].value;
    char name[128]={0};
    std::ifstream file;

    if (group == 0 || image == 0)
        return;

    sprintf(name, IMAGE_NAME, ImageNameT[0].text, ImageNameT[1].text, group, image, format);
    file.open(name, std::ios::in | std::ios::binary | std::ios::ate);
    DownloadN[0].value = 0;
    DownloadN[1].value = 0;
    if (file.is_open())
    {
        long size  = file.tellg();
        char *data = new char[size];

        file.seekg(0, std::ios::beg);
        file.read(data, size);
        file.close();
        remove(name);
        LOGF_DEBUG("Group %d, image %d, download initiated", group, image);
        DownloadNP.s = IPS_BUSY;
        IDSetNumber(&DownloadNP, "Download initiated");
        strncpy(FitsB[0].format, format, MAXINDIBLOBFMT);
        FitsB[0].blob    = data;
        FitsB[0].bloblen = FitsB[0].size = size;
        FitsBP.s                         = IPS_OK;
        IDSetBLOB(&FitsBP, nullptr);
        DownloadNP.s = IPS_OK;
        IDSetNumber(&DownloadNP, "Download finished");
    }
    else
    {
        DownloadNP.s = IPS_ALERT;
        IDSetNumber(&DownloadNP, "Download failed");
        LOGF_DEBUG("Group %d, image %d, upload failed", group, image);
    }
}

// DefaultDevice ----------------------------------------------------------------------------

const char *Imager::getDefaultName()
{
    return Imager::DEVICE_NAME.c_str();
}

bool Imager::initProperties()
{
    INDI::DefaultDevice::initProperties();

    addDebugControl();

    IUFillNumber(&GroupCountN[0], "GROUP_COUNT", "Image group count", "%3.0f", 1, MAX_GROUP_COUNT, 1, maxGroup = 1);
    IUFillNumberVector(&GroupCountNP, GroupCountN, 1, getDefaultName(), "GROUPS", "Image groups", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    IUFillText(&ControlledDeviceT[0], "CCD", "CCD", "CCD Simulator");
    IUFillText(&ControlledDeviceT[1], "FILTER", "Filter wheel", "Filter Simulator");
    IUFillTextVector(&ControlledDeviceTP, ControlledDeviceT, 2, getDefaultName(), "DEVICES", "Controlled devices",
                     MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    controlledCCD         = ControlledDeviceT[0].text;
    controlledFilterWheel = ControlledDeviceT[1].text;

    IUFillLight(&StatusL[0], "CCD", controlledCCD, IPS_IDLE);
    IUFillLight(&StatusL[1], "FILTER", controlledFilterWheel, IPS_IDLE);
    IUFillLightVector(&StatusLP, StatusL, 2, getDefaultName(), "STATUS", "Controlled devices", MAIN_CONTROL_TAB, IPS_IDLE);

    IUFillNumber(&ProgressN[0], "GROUP", "Current group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 0);
    IUFillNumber(&ProgressN[1], "IMAGE", "Current image", "%3.0f", 1, 100, 1, 0);
    IUFillNumber(&ProgressN[2], "REMAINING_TIME", "Remaining time", "%5.2f", 0, 36000, 0, 0.0);
    IUFillNumberVector(&ProgressNP, ProgressN, 3, getDefaultName(), "PROGRESS", "Batch execution progress", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&BatchS[0], "START", "Start batch", ISS_OFF);
    IUFillSwitch(&BatchS[1], "ABORT", "Abort batch", ISS_OFF);
    IUFillSwitchVector(&BatchSP, BatchS, 2, getDefaultName(), "BATCH", "Batch control", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY,
                       60, IPS_IDLE);

    IUFillText(&ImageNameT[0], "IMAGE_FOLDER", "Image folder", "/tmp");
    IUFillText(&ImageNameT[1], "IMAGE_PREFIX", "Image prefix", "IMG");
    IUFillTextVector(&ImageNameTP, ImageNameT, 2, getDefaultName(), "IMAGE_NAME", "Image name", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    IUFillNumber(&DownloadN[0], "GROUP", "Group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
    IUFillNumber(&DownloadN[1], "IMAGE", "Image", "%3.0f", 1, 100, 1, 1);
    IUFillNumberVector(&DownloadNP, DownloadN, 2, getDefaultName(), "DOWNLOAD", "Download image", DOWNLOAD_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillBLOB(&FitsB[0], "IMAGE", "Image", "");
    IUFillBLOBVector(&FitsBP, FitsB, 1, getDefaultName(), "IMAGE", "Image Data", DOWNLOAD_TAB, IP_RO, 60, IPS_IDLE);

    defineProperty(&GroupCountNP);
    defineProperty(&ControlledDeviceTP);
    defineProperty(&ImageNameTP);

    for (int i = 0; i < GroupCountN[0].value; i++)
    {
        groups[i]->defineProperties();
    }

    IUFillNumber(&CCDImageExposureN[0], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0, 36000, 0, 1.0);
    IUFillNumberVector(&CCDImageExposureNP, CCDImageExposureN, 1, ControlledDeviceT[0].text, "CCD_EXPOSURE", "Expose",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&CCDImageBinN[0], "HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    IUFillNumber(&CCDImageBinN[1], "VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    IUFillNumberVector(&CCDImageBinNP, CCDImageBinN, 2, ControlledDeviceT[0].text, "CCD_BINNING", "Binning",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&CCDUploadS[0], "UPLOAD_CLIENT", "Client", ISS_OFF);
    IUFillSwitch(&CCDUploadS[1], "UPLOAD_LOCAL", "Local", ISS_ON);
    IUFillSwitch(&CCDUploadS[2], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&CCDUploadSP, CCDUploadS, 3, ControlledDeviceT[0].text, "UPLOAD_MODE", "Upload", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&CCDUploadSettingsT[0], "UPLOAD_DIR", "Dir", "");
    IUFillText(&CCDUploadSettingsT[1], "UPLOAD_PREFIX", "Prefix", IMAGE_PREFIX);
    IUFillTextVector(&CCDUploadSettingsTP, CCDUploadSettingsT, 2, ControlledDeviceT[0].text, "UPLOAD_SETTINGS",
                     "Upload Settings", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&FilterSlotN[0], "FILTER_SLOT_VALUE", "Filter", "%3.0f", 1.0, 12.0, 1.0, 1.0);
    IUFillNumberVector(&FilterSlotNP, FilterSlotN, 1, ControlledDeviceT[1].text, "FILTER_SLOT", "Filter Slot",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

bool Imager::updateProperties()
{
    if (isConnected())
    {
        defineProperty(&StatusLP);
        ProgressN[0].value = group = 0;
        ProgressN[1].value = image = 0;
        ProgressNP.s               = IPS_IDLE;
        defineProperty(&ProgressNP);
        BatchSP.s = IPS_IDLE;
        defineProperty(&BatchSP);
        DownloadN[0].value = 0;
        DownloadN[1].value = 0;
        DownloadNP.s       = IPS_IDLE;
        defineProperty(&DownloadNP);
        FitsBP.s = IPS_IDLE;
        defineProperty(&FitsBP);
    }
    else
    {
        deleteProperty(StatusLP.name);
        deleteProperty(ProgressNP.name);
        deleteProperty(BatchSP.name);
        deleteProperty(DownloadNP.name);
        deleteProperty(FitsBP.name);
    }
    return true;
}

void Imager::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);
}

bool Imager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (Imager::DEVICE_NAME == dev)
    {
        if (std::string{name} == std::string{GroupCountNP.name})
        {
            for (int i = 0; i < maxGroup; i++)
                groups[i]->deleteProperties();
            IUUpdateNumber(&GroupCountNP, values, names, n);
            maxGroup = (int)GroupCountN[0].value;
            if (maxGroup > MAX_GROUP_COUNT)
                GroupCountN[0].value = maxGroup = MAX_GROUP_COUNT;
            for (int i = 0; i < maxGroup; i++)
                groups[i]->defineProperties();
            GroupCountNP.s = IPS_OK;
            IDSetNumber(&GroupCountNP, nullptr);
            return true;
        }
        if (std::string{name} == std::string{DownloadNP.name})
        {
            IUUpdateNumber(&DownloadNP, values, names, n);
            initiateDownload();
            return true;
        }
        if (strncmp(name, GROUP_PREFIX, GROUP_PREFIX_LEN) == 0)
        {
            for (int i = 0; i < GroupCountN[0].value; i++)
                if (groups[i]->ISNewNumber(dev, name, values, names, n))
                {
                    return true;
                }
            return false;
        }
    }
    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool Imager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (Imager::DEVICE_NAME == dev)
    {
        if (std::string{name} == std::string{BatchSP.name})
        {
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], BatchS[0].name) == 0 && states[i] == ISS_ON)
                {
                    if (!isRunning())
                        startBatch();
                }
                if (strcmp(names[i], BatchS[1].name) == 0 && states[i] == ISS_ON)
                {
                    if (isRunning())
                        abortBatch();
                }
            }
            BatchSP.s = IPS_OK;
            IDSetSwitch(&BatchSP, nullptr);
            return true;
        }
    }
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Imager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (Imager::DEVICE_NAME == dev)
    {
        if (std::string{name} == std::string{ControlledDeviceTP.name})
        {
            IUUpdateText(&ControlledDeviceTP, texts, names, n);
            IDSetText(&ControlledDeviceTP, nullptr);
            strncpy(StatusL[0].label, ControlledDeviceT[0].text, sizeof(StatusL[0].label));
            strncpy(CCDImageExposureNP.device, ControlledDeviceT[0].text, sizeof(CCDImageExposureNP.device));
            strncpy(CCDImageBinNP.device, ControlledDeviceT[0].text, sizeof(CCDImageBinNP.device));
            strncpy(StatusL[1].label, ControlledDeviceT[1].text, sizeof(StatusL[1].label));
            strncpy(FilterSlotNP.device, ControlledDeviceT[1].text, sizeof(FilterSlotNP.device));
            return true;
        }
        if (std::string{name} == std::string{ImageNameTP.name})
        {
            IUUpdateText(&ImageNameTP, texts, names, n);
            IDSetText(&ImageNameTP, nullptr);
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool Imager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                       char *names[], int n)
{
    return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool Imager::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool Imager::Connect()
{
    setServer("localhost", 7624); // TODO configuration options
    watchDevice(controlledCCD);
    watchDevice(controlledFilterWheel);
    connectServer();
    setBLOBMode(B_ALSO, controlledCCD, nullptr);

    return true;
}

bool Imager::Disconnect()
{
    if (isRunning())
        abortBatch();
    disconnectServer();
    return true;
}

// BaseClient ----------------------------------------------------------------------------

void Imager::serverConnected()
{
    LOG_DEBUG("Server connected");
    StatusL[0].s = IPS_ALERT;
    StatusL[1].s = IPS_ALERT;
    IDSetLight(&StatusLP, nullptr);
}

void Imager::newDevice(INDI::BaseDevice *dp)
{
    std::string deviceName{dp->getDeviceName()};

    LOGF_DEBUG("Device %s detected", deviceName.c_str());
    if (deviceName == controlledCCD)
        StatusL[0].s = IPS_BUSY;
    if (deviceName == controlledFilterWheel)
        StatusL[1].s = IPS_BUSY;

    IDSetLight(&StatusLP, nullptr);
}

void Imager::newProperty(INDI::Property *property)
{
    std::string deviceName{property->getDeviceName()};

    if (strcmp(property->getName(), INDI::SP::CONNECTION) == 0)
    {
        bool state = property->getSwitch()->sp[0].s != ISS_OFF;
        if (deviceName == controlledCCD)
        {
            if (state)
            {
                StatusL[0].s = IPS_OK;
            }
            else
            {
                connectDevice(controlledCCD);
                LOGF_DEBUG("Connecting %s", controlledCCD);
            }
        }
        if (deviceName == controlledFilterWheel)
        {
            if (state)
            {
                StatusL[1].s = IPS_OK;
            }
            else
            {
                connectDevice(controlledFilterWheel);
                LOGF_DEBUG("Connecting %s", controlledFilterWheel);
            }
        }
        IDSetLight(&StatusLP, nullptr);
    }
}

void Imager::removeProperty(INDI::Property *property)
{
    INDI_UNUSED(property);
}

void Imager::removeDevice(INDI::BaseDevice *dp)
{
    INDI_UNUSED(dp);
}

void Imager::newBLOB(IBLOB *bp)
{
    if (ProgressNP.s == IPS_BUSY)
    {
        char name[128]={0};
        std::ofstream file;

        strncpy(format, bp->format, 16);
        sprintf(name, IMAGE_NAME, ImageNameT[0].text, ImageNameT[1].text, group, image, format);
        file.open(name, std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(static_cast<char *>(bp->blob), bp->bloblen);
        file.close();
        LOGF_DEBUG("Group %d of %d, image %d of %d, saved to %s", group, maxGroup, image, maxImage,
               name);
        if (image == maxImage)
        {
            if (group == maxGroup)
            {
                batchDone();
            }
            else
            {
                maxImage           = nextGroup()->count();
                ProgressN[0].value = group = group + 1;
                ProgressN[1].value = image = 1;
                IDSetNumber(&ProgressNP, nullptr);
                initiateNextFilter();
            }
        }
        else
        {
            ProgressN[1].value = image = image + 1;
            IDSetNumber(&ProgressNP, nullptr);
            initiateNextFilter();
        }
    }
}

void Imager::newSwitch(ISwitchVectorProperty *svp)
{
    std::string deviceName{svp->device};
    bool state             = svp->sp[0].s != ISS_OFF;

    if (strcmp(svp->name, INDI::SP::CONNECTION) == 0)
    {
        if (deviceName == controlledCCD)
        {
            if (state)
            {
                StatusL[0].s = IPS_OK;
            }
            else
            {
                StatusL[0].s = IPS_BUSY;
            }
        }
        if (deviceName == controlledFilterWheel)
        {
            if (state)
            {
                StatusL[1].s = IPS_OK;
            }
            else
            {
                StatusL[1].s = IPS_BUSY;
            }
        }
        IDSetLight(&StatusLP, nullptr);
    }
}

void Imager::newNumber(INumberVectorProperty *nvp)
{
    std::string deviceName{nvp->device};

    if (deviceName == controlledCCD)
    {
        if (strcmp(nvp->name, "CCD_EXPOSURE") == 0)
        {
            ProgressN[2].value = nvp->np[0].value;
            IDSetNumber(&ProgressNP, nullptr);
        }
    }
    if (deviceName == controlledFilterWheel)
    {
        if (strcmp(nvp->name, "FILTER_SLOT") == 0)
        {
            FilterSlotN[0].value = nvp->np->value;
            if (nvp->s == IPS_OK)
                initiateNextCapture();
        }
    }
}

void Imager::newText(ITextVectorProperty *tvp)
{
    std::string deviceName{tvp->device};

    if (deviceName == controlledCCD)
    {
        if (strcmp(tvp->name, "CCD_FILE_PATH") == 0)
        {
            char name[128]={0};

            strncpy(format, strrchr(tvp->tp[0].text, '.'), sizeof(format));
            sprintf(name, IMAGE_NAME, ImageNameT[0].text, ImageNameT[1].text, group, image, format);
            rename(tvp->tp[0].text, name);
            LOGF_DEBUG("Group %d of %d, image %d of %d, saved to %s", group, maxGroup, image,
                   maxImage, name);
            if (image == maxImage)
            {
                if (group == maxGroup)
                {
                    batchDone();
                }
                else
                {
                    maxImage           = nextGroup()->count();
                    ProgressN[0].value = group = group + 1;
                    ProgressN[1].value = image = 1;
                    IDSetNumber(&ProgressNP, nullptr);
                    initiateNextFilter();
                }
            }
            else
            {
                ProgressN[1].value = image = image + 1;
                IDSetNumber(&ProgressNP, nullptr);
                initiateNextFilter();
            }
        }
    }
}

void Imager::newLight(ILightVectorProperty *lvp)
{
    INDI_UNUSED(lvp);
}

void Imager::newMessage(INDI::BaseDevice *dp, int messageID)
{
    INDI_UNUSED(dp);
    INDI_UNUSED(messageID);
}

void Imager::serverDisconnected(int exit_code)
{
    INDI_UNUSED(exit_code);
    LOG_DEBUG("Server disconnected");
    StatusL[0].s = IPS_ALERT;
    StatusL[1].s = IPS_ALERT;
}


