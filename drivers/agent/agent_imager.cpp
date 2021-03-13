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



// Driver entry points ----------------------------------------------------------------------------

void ISGetProperties(const char *dev)
{
    imager->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    imager->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    imager->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    imager->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    imager->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    imager->ISSnoopDevice(root);
}

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
    return ProgressNP.getState() == IPS_BUSY;
}

bool Imager::isCCDConnected()
{
    return StatusLP[0].getState() == IPS_OK;
}

bool Imager::isFilterConnected()
{
    return StatusLP[1].getState() == IPS_OK;
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
                ProgressNP.setState(IPS_ALERT);
                ProgressNP.apply("Filter wheel is not connected");
                return;
            }
            else
            {
                initiateNextCapture();
            }
        }
        else if (filterSlot != 0 && FilterSlotNP[0].getValue() != filterSlot)
        {
            FilterSlotNP[0].setValue(filterSlot);
            sendNewNumber(&FilterSlotNP);
            LOGF_DEBUG("Group %d of %d, image %d of %d, filer %d, filter set initiated on %s",
                   group, maxGroup, image, maxImage, (int)FilterSlotNP[0].getValue(), FilterSlotNP.getDeviceName());
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
                ProgressNP.setState(IPS_ALERT);
                ProgressNP.apply("CCD is not connected");
                return;
            }
            CCDImageBinNP[0].setValue(currentGroup()->binning());
            CCDImageBinNP[1].setValue(currentGroup()->binning());
            sendNewNumber(&CCDImageBinNP);
            CCDImageExposureNP[0].setValue(currentGroup()->exposure());
            sendNewNumber(&CCDImageExposureNP);
            CCDUploadSettingsTP[0].setText(ImageNameTP[0].getText());
            CCDUploadSettingsTP[1].setText("_TMP_");
            sendNewSwitch(&CCDUploadSP);
            sendNewText(&CCDUploadSettingsTP);
            LOGF_DEBUG("Group %d of %d, image %d of %d, duration %.1fs, binning %d, capture initiated on %s", group,
                   maxGroup, image, maxImage, CCDImageExposureNP[0].getValue(), (int)CCDImageBinNP[0].getValue(),
                   CCDImageExposureNP.getDeviceName());
        }
    }
}

void Imager::startBatch()
{
    LOG_DEBUG("Batch started");
    ProgressNP[0].setValue(group = 1);
    ProgressNP[1].setValue(image = 1);
    maxImage                   = currentGroup()->count();
    ProgressNP.setState(IPS_BUSY);
    ProgressNP.apply();
    initiateNextFilter();
}

void Imager::abortBatch()
{
    ProgressNP.setState(IPS_ALERT);
    ProgressNP.apply("Batch aborted");
}

void Imager::batchDone()
{
    ProgressNP.setState(IPS_OK);
    ProgressNP.apply("Batch done");
}

void Imager::initiateDownload()
{
    int group = (int)DownloadNP[0].getValue();
    int image = (int)DownloadNP[1].getValue();
    char name[128]={0};
    std::ifstream file;

    if (group == 0 || image == 0)
        return;

    sprintf(name, IMAGE_NAME, ImageNameTP[0].getText(), ImageNameTP[1].getText(), group, image, format);
    file.open(name, std::ios::in | std::ios::binary | std::ios::ate);
    DownloadNP[0].setValue(0);
    DownloadNP[1].setValue(0);
    if (file.is_open())
    {
        long size  = file.tellg();
        char *data = new char[size];

        file.seekg(0, std::ios::beg);
        file.read(data, size);
        file.close();
        remove(name);
        LOGF_DEBUG("Group %d, image %d, download initiated", group, image);
        DownloadNP.setState(IPS_BUSY);
        DownloadNP.apply("Download initiated");
        strncpy(FitsB[0].format, format, MAXINDIBLOBFMT);
        FitsB[0].blob    = data;
        FitsB[0].bloblen = FitsB[0].size = size;
        FitsBP.s                         = IPS_OK;
        IDSetBLOB(&FitsBP, nullptr);
        DownloadNP.setState(IPS_OK);
        DownloadNP.apply("Download finished");
    }
    else
    {
        DownloadNP.setState(IPS_ALERT);
        DownloadNP.apply("Download failed");
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

    GroupCountNP[0].fill("GROUP_COUNT", "Image group count", "%3.0f", 1, MAX_GROUP_COUNT, 1, maxGroup = 1);
    GroupCountNP.fill(getDefaultName(), "GROUPS", "Image groups", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    ControlledDeviceTP[0].fill("CCD", "CCD", "CCD Simulator");
    ControlledDeviceTP[1].fill("FILTER", "Filter wheel", "Filter Simulator");
    ControlledDeviceTP.fill(getDefaultName(), "DEVICES", "Controlled devices",
                     MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    controlledCCD         = ControlledDeviceTP[0].getText();
    controlledFilterWheel = ControlledDeviceTP[1].getText();

    StatusLP[0].fill("CCD", controlledCCD, IPS_IDLE);
    StatusLP[1].fill("FILTER", controlledFilterWheel, IPS_IDLE);
    StatusLP.fill(getDefaultName(), "STATUS", "Controlled devices", MAIN_CONTROL_TAB, IPS_IDLE);

    ProgressNP[0].fill("GROUP", "Current group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 0);
    ProgressNP[1].fill("IMAGE", "Current image", "%3.0f", 1, 100, 1, 0);
    ProgressNP[2].fill("REMAINING_TIME", "Remaining time", "%5.2f", 0, 36000, 0, 0.0);
    ProgressNP.fill(getDefaultName(), "PROGRESS", "Batch execution progress", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    BatchSP[0].fill("START", "Start batch", ISS_OFF);
    BatchSP[1].fill("ABORT", "Abort batch", ISS_OFF);
    BatchSP.fill(getDefaultName(), "BATCH", "Batch control", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY,
                       60, IPS_IDLE);

    ImageNameTP[0].fill("IMAGE_FOLDER", "Image folder", "/tmp");
    ImageNameTP[1].fill("IMAGE_PREFIX", "Image prefix", "IMG");
    ImageNameTP.fill(getDefaultName(), "IMAGE_NAME", "Image name", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    DownloadNP[0].fill("GROUP", "Group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
    DownloadNP[1].fill("IMAGE", "Image", "%3.0f", 1, 100, 1, 1);
    DownloadNP.fill(getDefaultName(), "DOWNLOAD", "Download image", DOWNLOAD_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillBLOB(&FitsB[0], "IMAGE", "Image", "");
    IUFillBLOBVector(&FitsBP, FitsB, 1, getDefaultName(), "IMAGE", "Image Data", DOWNLOAD_TAB, IP_RO, 60, IPS_IDLE);

    defineProperty(GroupCountNP);
    defineProperty(ControlledDeviceTP);
    defineProperty(ImageNameTP);

    for (int i = 0; i < GroupCountNP[0].getValue(); i++)
    {
        groups[i]->defineProperties();
    }

    CCDImageExposureNP[0].fill("CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0, 36000, 0, 1.0);
    CCDImageExposureNP.fill(ControlledDeviceTP[0].getText(), "CCD_EXPOSURE", "Expose",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    CCDImageBinNP[0].fill("HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    CCDImageBinNP[1].fill("VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    CCDImageBinNP.fill(ControlledDeviceTP[0].getText(), "CCD_BINNING", "Binning",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    CCDUploadSP[0].fill("UPLOAD_CLIENT", "Client", ISS_OFF);
    CCDUploadSP[1].fill("UPLOAD_LOCAL", "Local", ISS_ON);
    CCDUploadSP[2].fill("UPLOAD_BOTH", "Both", ISS_OFF);
    CCDUploadSP.fill(ControlledDeviceTP[0].getText(), "UPLOAD_MODE", "Upload", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    CCDUploadSettingsTP[0].fill("UPLOAD_DIR", "Dir", "");
    CCDUploadSettingsTP[1].fill("UPLOAD_PREFIX", "Prefix", IMAGE_PREFIX);
    CCDUploadSettingsTP.fill(ControlledDeviceTP[0].getText(), "UPLOAD_SETTINGS",
                     "Upload Settings", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    FilterSlotNP[0].fill("FILTER_SLOT_VALUE", "Filter", "%3.0f", 1.0, 12.0, 1.0, 1.0);
    FilterSlotNP.fill(ControlledDeviceTP[1].getText(), "FILTER_SLOT", "Filter Slot",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

bool Imager::updateProperties()
{
    if (isConnected())
    {
        defineProperty(StatusLP);
        ProgressNP[0].setValue(group = 0);
        ProgressNP[1].setValue(image = 0);
        ProgressNP.setState(IPS_IDLE);
        defineProperty(ProgressNP);
        BatchSP.setState(IPS_IDLE);
        defineProperty(BatchSP);
        DownloadNP[0].setValue(0);
        DownloadNP[1].setValue(0);
        DownloadNP.setState(IPS_IDLE);
        defineProperty(DownloadNP);
        FitsBP.s = IPS_IDLE;
        defineProperty(&FitsBP);
    }
    else
    {
        deleteProperty(StatusLP.getName());
        deleteProperty(ProgressNP.getName());
        deleteProperty(BatchSP.getName());
        deleteProperty(DownloadNP.getName());
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
        if (GroupCountNP.isNameMatch(name))
        {
            for (int i = 0; i < maxGroup; i++)
                groups[i]->deleteProperties();
            GroupCountNP.update(values, names, n);
            maxGroup = (int)GroupCountNP[0].getValue();
            if (maxGroup > MAX_GROUP_COUNT)
                GroupCountNP[0].setValue(maxGroup = MAX_GROUP_COUNT);
            for (int i = 0; i < maxGroup; i++)
                groups[i]->defineProperties();
            GroupCountNP.setState(IPS_OK);
            GroupCountNP.apply();
            return true;
        }
        if (DownloadNP.isNameMatch(name))
        {
            DownloadNP.update(values, names, n);
            initiateDownload();
            return true;
        }
        if (strncmp(name, GROUP_PREFIX, GROUP_PREFIX_LEN) == 0)
        {
            for (int i = 0; i < GroupCountNP[0].getValue(); i++)
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
        if (BatchSP.isNameMatch(name))
        {
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], BatchSP[0].getName()) == 0 && states[i] == ISS_ON)
                {
                    if (!isRunning())
                        startBatch();
                }
                if (strcmp(names[i], BatchSP[1].getName()) == 0 && states[i] == ISS_ON)
                {
                    if (isRunning())
                        abortBatch();
                }
            }
            BatchSP.setState(IPS_OK);
            BatchSP.apply();
            return true;
        }
    }
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Imager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (Imager::DEVICE_NAME == dev)
    {
        if (ControlledDeviceTP.isNameMatch(name))
        {
            ControlledDeviceTP.update(texts, names, n);
            ControlledDeviceTP.apply();
            StatusLP[0].setLabel(ControlledDeviceTP[0].getText());
            CCDImageExposureNP.setDeviceName(ControlledDeviceTP[0].getText());
            CCDImageBinNP.setDeviceName(ControlledDeviceTP[0].getText());
            StatusLP[1].setLabel(ControlledDeviceTP[1].getText());
            FilterSlotNP.setDeviceName(ControlledDeviceTP[1].getText());
            return true;
        }
        if (ImageNameTP.isNameMatch(name))
        {
            ImageNameTP.update(texts, names, n);
            ImageNameTP.apply();
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
    StatusLP[0].setState(IPS_ALERT);
    StatusLP[1].setState(IPS_ALERT);
    StatusLP.apply();
}

void Imager::newDevice(INDI::BaseDevice *dp)
{
    std::string deviceName{dp->getDeviceName()};

    LOGF_DEBUG("Device %s detected", deviceName.c_str());
    if (deviceName == controlledCCD)
        StatusLP[0].setState(IPS_BUSY);
    if (deviceName == controlledFilterWheel)
        StatusLP[1].setState(IPS_BUSY);

    StatusLP.apply();
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
                StatusLP[0].setState(IPS_OK);
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
                StatusLP[1].setState(IPS_OK);
            }
            else
            {
                connectDevice(controlledFilterWheel);
                LOGF_DEBUG("Connecting %s", controlledFilterWheel);
            }
        }
        StatusLP.apply();
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
    if (ProgressNP.getState() == IPS_BUSY)
    {
        char name[128]={0};
        std::ofstream file;

        strncpy(format, bp->format, 16);
        sprintf(name, IMAGE_NAME, ImageNameTP[0].getText(), ImageNameTP[1].getText(), group, image, format);
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
                ProgressNP[0].setValue(group = group + 1);
                ProgressNP[1].setValue(image = 1);
                ProgressNP.apply();
                initiateNextFilter();
            }
        }
        else
        {
            ProgressNP[1].setValue(image = image + 1);
            ProgressNP.apply();
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
                StatusLP[0].setState(IPS_OK);
            }
            else
            {
                StatusLP[0].setState(IPS_BUSY);
            }
        }
        if (deviceName == controlledFilterWheel)
        {
            if (state)
            {
                StatusLP[1].setState(IPS_OK);
            }
            else
            {
                StatusLP[1].setState(IPS_BUSY);
            }
        }
        StatusLP.apply();
    }
}

void Imager::newNumber(INumberVectorProperty *nvp)
{
    std::string deviceName{nvp->device};

    if (deviceName == controlledCCD)
    {
        if (strcmp(nvp->name, "CCD_EXPOSURE") == 0)
        {
            ProgressNP[2].setValue(nvp->np[0].value);
            ProgressNP.apply();
        }
    }
    if (deviceName == controlledFilterWheel)
    {
        if (strcmp(nvp->name, "FILTER_SLOT") == 0)
        {
            FilterSlotNP[0].setValue(nvp->np->value);
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
            sprintf(name, IMAGE_NAME, ImageNameTP[0].getText(), ImageNameTP[1].getText(), group, image, format);
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
                    ProgressNP[0].setValue(group = group + 1);
                    ProgressNP[1].setValue(image = 1);
                    ProgressNP.apply();
                    initiateNextFilter();
                }
            }
            else
            {
                ProgressNP[1].setValue(image = image + 1);
                ProgressNP.apply();
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
    StatusLP[0].setState(IPS_ALERT);
    StatusLP[1].setState(IPS_ALERT);
}


