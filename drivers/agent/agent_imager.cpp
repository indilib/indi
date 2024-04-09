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
    int i = 0;
    std::generate(groups.begin(), groups.end(), [this, &i] { return std::make_shared<Group>(i++, this); });
}

bool Imager::isRunning()
{
    return ProgressNP.getState() == IPS_BUSY;
}

bool Imager::isCCDConnected()
{
    return StatusLP[CCD].getState() == IPS_OK;
}

bool Imager::isFilterConnected()
{
    return StatusLP[FILTER].getState() == IPS_OK;
}

std::shared_ptr<Group> Imager::getGroup(int index) const
{
    if(index > -1 && index <= maxGroup)
        return groups[index];
    return {};
}

std::shared_ptr<Group> Imager::currentGroup() const
{
    return getGroup(group - 1);
}



std::shared_ptr<Group> Imager::nextGroup() const
{
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
                LOG_INFO("Filter wheel is not connected");
                ProgressNP.apply();
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
            sendNewNumber(FilterSlotNP);
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
                LOG_INFO("CCD is not connected");
                ProgressNP.apply();
                return;
            }
            CCDImageBinNP[HOR_BIN].setValue(currentGroup()->binning());
            CCDImageBinNP[VER_BIN].setValue(currentGroup()->binning());
            sendNewNumber(CCDImageBinNP);
            CCDImageExposureNP[0].setValue(currentGroup()->exposure());
            sendNewNumber(CCDImageExposureNP);
            CCDUploadSettingsTP[UPLOAD_DIR].setText(ImageNameTP[IMAGE_FOLDER].getText());
            CCDUploadSettingsTP[UPLOAD_PREFIX].setText("_TMP_");
            sendNewSwitch(CCDUploadSP);
            sendNewText(CCDUploadSettingsTP);
            LOGF_DEBUG("Group %d of %d, image %d of %d, duration %.1fs, binning %d, capture initiated on %s", group,
                       maxGroup, image, maxImage, CCDImageExposureNP[0].getValue(), (int)CCDImageBinNP[HOR_BIN].getValue(),
                       CCDImageExposureNP.getDeviceName());
        }
    }
}

void Imager::startBatch()
{
    LOG_DEBUG("Batch started");
    ProgressNP[GROUP].setValue(group = 1);
    ProgressNP[IMAGE].setValue(image = 1);
    maxImage                   = currentGroup()->count();
    ProgressNP.setState(IPS_BUSY);
    ProgressNP.apply();
    initiateNextFilter();
}

void Imager::abortBatch()
{
    ProgressNP.setState(IPS_ALERT);
    LOG_ERROR("Batch aborted");
    ProgressNP.apply();
}

void Imager::batchDone()
{
    ProgressNP.setState(IPS_OK);
    LOG_INFO("Batch done");
    ProgressNP.apply();
}

void Imager::initiateDownload()
{
    int group = (int)DownloadNP[GROUP].getValue();
    int image = (int)DownloadNP[IMAGE].getValue();
    char name[128] = {0};
    std::ifstream file;

    if (group == 0 || image == 0)
        return;

    sprintf(name, IMAGE_NAME, ImageNameTP[IMAGE_FOLDER].getText(), ImageNameTP[IMAGE_NAME_PREFIX].getText(), group, image, format);
    file.open(name, std::ios::in | std::ios::binary | std::ios::ate);
    DownloadNP[GROUP].setValue(0);
    DownloadNP[IMAGE].setValue(0);
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
        LOG_INFO("Download initiated");
        DownloadNP.apply();
//        strncpy(FitsBP[0].format, format, MAXINDIBLOBFMT);
        FitsBP[0].setFormat(format);
        FitsBP[0].setBlob(data);
        FitsBP[0].setBlobLen(FitsBP[0].getSize() == size);
        FitsBP.setState(IPS_OK);
        FitsBP.apply();
        DownloadNP.setState(IPS_OK);
        LOG_INFO("Download finished");
        DownloadNP.apply();
    }
    else
    {
        DownloadNP.setState(IPS_ALERT);
        LOG_ERROR("Download failed");
        DownloadNP.apply();
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

    ControlledDeviceTP[CCD].fill("CCD", "CCD", "CCD Simulator");
    ControlledDeviceTP[FILTER].fill("FILTER", "Filter wheel", "Filter Simulator");
    ControlledDeviceTP.fill(getDefaultName(), "DEVICES", "Controlled devices",
                            MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    controlledCCD         = ControlledDeviceTP[CCD].getText();
    controlledFilterWheel = ControlledDeviceTP[FILTER].getText();

    StatusLP[CCD].fill("CCD", controlledCCD, IPS_IDLE);
    StatusLP[FILTER].fill("FILTER", controlledFilterWheel, IPS_IDLE);
    StatusLP.fill(getDefaultName(), "STATUS", "Controlled devices", MAIN_CONTROL_TAB, IPS_IDLE);

    ProgressNP[GROUP].fill("GROUP", "Current group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 0);
    ProgressNP[IMAGE].fill("IMAGE", "Current image", "%3.0f", 1, 100, 1, 0);
    ProgressNP[REMAINING_TIME].fill("REMAINING_TIME", "Remaining time", "%5.2f", 0, 36000, 0, 0.0);
    ProgressNP.fill(getDefaultName(), "PROGRESS", "Batch execution progress", MAIN_CONTROL_TAB,
                    IP_RO, 60, IPS_IDLE);

    BatchSP[START].fill("START", "Start batch", ISS_OFF);
    BatchSP[ABORT].fill("ABORT", "Abort batch", ISS_OFF);
    BatchSP.fill(getDefaultName(), "BATCH", "Batch control", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY,
                 60, IPS_IDLE);

    ImageNameTP[IMAGE_FOLDER].fill("IMAGE_FOLDER", "Image folder", "/tmp");
    ImageNameTP[IMAGE_NAME_PREFIX].fill("IMAGE_NAME_PREFIX", "Image prefix", "IMG");
    ImageNameTP.fill(getDefaultName(), "IMAGE_NAME", "Image name", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    DownloadNP[GROUP].fill("GROUP", "Group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
    DownloadNP[IMAGE].fill("IMAGE", "Image", "%3.0f", 1, 100, 1, 1);
    DownloadNP.fill(getDefaultName(), "DOWNLOAD", "Download image", DOWNLOAD_TAB, IP_RW, 60,
                    IPS_IDLE);

    FitsBP[0].fill("IMAGE", "Image", "");
    FitsBP.fill(getDefaultName(), "IMAGE", "Image Data", DOWNLOAD_TAB, IP_RO, 60, IPS_IDLE);

    defineProperty(GroupCountNP);
    defineProperty(ControlledDeviceTP);
    defineProperty(ImageNameTP);

    for (int i = 0; i < GroupCountNP[0].getValue(); i++)
    {
        groups[i]->defineProperties();
    }

    CCDImageExposureNP[0].fill("CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0, 36000, 0, 1.0);
    CCDImageExposureNP.fill(ControlledDeviceTP[CCD].getText(), "CCD_EXPOSURE", "Expose",
                            MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    CCDImageBinNP[HOR_BIN].fill("HOR_BIN", "X", "%2.0f", 1, 4, 1, 1);
    CCDImageBinNP[VER_BIN].fill("VER_BIN", "Y", "%2.0f", 1, 4, 1, 1);
    CCDImageBinNP.fill(ControlledDeviceTP[CCD].getText(), "CCD_BINNING", "Binning",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    CCDUploadSP[UPLOAD_CLIENT].fill("UPLOAD_CLIENT", "Client", ISS_OFF);
    CCDUploadSP[UPLOAD_LOCAL].fill("UPLOAD_LOCAL", "Local", ISS_ON);
    CCDUploadSP[UPLOAD_BOTH].fill("UPLOAD_BOTH", "Both", ISS_OFF);
    CCDUploadSP.fill(ControlledDeviceTP[CCD].getText(), "UPLOAD_MODE", "Upload", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    CCDUploadSettingsTP[UPLOAD_DIR].fill("UPLOAD_DIR", "Dir", "");
    CCDUploadSettingsTP[UPLOAD_PREFIX].fill("UPLOAD_PREFIX", "Prefix", IMAGE_PREFIX);
    CCDUploadSettingsTP.fill(ControlledDeviceTP[CCD].getText(), "UPLOAD_SETTINGS",
                     "Upload Settings", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    FilterSlotNP[0].fill("FILTER_SLOT_VALUE", "Filter", "%3.0f", 1.0, 12.0, 1.0, 1.0);
    FilterSlotNP.fill(ControlledDeviceTP[FILTER].getText(), "FILTER_SLOT", "Filter Slot",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

bool Imager::updateProperties()
{
    if (isConnected())
    {
        defineProperty(StatusLP);
        ProgressNP[GROUP].setValue(group = 0);
        ProgressNP[IMAGE].setValue(image = 0);
        ProgressNP.setState(IPS_IDLE);
        defineProperty(ProgressNP);
        BatchSP.setState(IPS_IDLE);
        defineProperty(BatchSP);
        DownloadNP[GROUP].setValue(0);
        DownloadNP[IMAGE].setValue(0);
        DownloadNP.setState(IPS_IDLE);
        defineProperty(DownloadNP);
        FitsBP.setState(IPS_IDLE);
        defineProperty(FitsBP);
    }
    else
    {
        deleteProperty(StatusLP);
        deleteProperty(ProgressNP);
        deleteProperty(BatchSP);
        deleteProperty(DownloadNP);
        deleteProperty(FitsBP);
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
        if (GroupCountNP[0].isNameMatch(name))
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
                if (strcmp(names[i], BatchSP[START].getName()) == 0 && states[i] == ISS_ON)
                {
                    if (!isRunning())
                        startBatch();
                }
                if (strcmp(names[i], BatchSP[ABORT].getName()) == 0 && states[i] == ISS_ON)
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

            StatusLP[CCD].setLabel(ControlledDeviceTP[CCD].getText());
            CCDImageExposureNP.setDeviceName(ControlledDeviceTP[CCD].getText());
            CCDImageBinNP.setDeviceName(ControlledDeviceTP[CCD].getText());
            StatusLP[FILTER].setLabel(ControlledDeviceTP[FILTER].getText());
            FilterSlotNP.setDeviceName(ControlledDeviceTP[FILTER].getText());

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
    BaseClient::watchDevice(controlledCCD);
    BaseClient::watchDevice(controlledFilterWheel);
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
    StatusLP[CCD].setState(IPS_ALERT);
    StatusLP[FILTER].setState(IPS_ALERT);
    StatusLP.apply();
}

void Imager::newDevice(INDI::BaseDevice baseDevice)
{
    std::string deviceName{baseDevice.getDeviceName()};

    LOGF_DEBUG("Device %s detected", deviceName.c_str());
    if (deviceName == controlledCCD)
        StatusLP[CCD].setState(IPS_BUSY);
    if (deviceName == controlledFilterWheel)
        StatusLP[FILTER].setState(IPS_BUSY);

    StatusLP.apply();
}

void Imager::newProperty(INDI::Property property)
{
    std::string deviceName{property.getDeviceName()};

    if (property.isNameMatch(INDI::SP::CONNECTION))
    {
        bool state = INDI::PropertySwitch(property)[0].getState() != ISS_OFF;
        if (deviceName == controlledCCD)
        {
            if (state)
            {
                StatusLP[CCD].setState(IPS_OK);
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
                StatusLP[FILTER].setState(IPS_OK);
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

void Imager::updateProperty(INDI::Property property)
{
    std::string deviceName{property.getDeviceName()};

    if (property.getType() == INDI_BLOB)
    {
        for (auto &bp: INDI::PropertyBlob(property))
        {
            if (ProgressNP.getState() == IPS_BUSY)
            {
                char name[128] = {0};
                std::ofstream file;

                strncpy(format, bp.getFormat(), 16);
                sprintf(name, IMAGE_NAME, ImageNameTP[IMAGE_FOLDER].getText(), ImageNameTP[IMAGE_NAME_PREFIX].getText(), group, image, format);
                file.open(name, std::ios::out | std::ios::binary | std::ios::trunc);
                file.write(static_cast<char *>(bp.getBlob()), bp.getBlobLen());
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
                        ProgressNP[GROUP].setValue(group = group + 1);
                        ProgressNP[IMAGE].setValue(image = 1);
                        ProgressNP.apply();
                        initiateNextFilter();
                    }
                }
                else
                {
                    ProgressNP[IMAGE].setValue(image = image + 1);
                    ProgressNP.apply();
                    initiateNextFilter();
                }
            }
        }
        return;
    }

    if (property.isNameMatch(INDI::SP::CONNECTION))
    {
        INDI::PropertySwitch propertySwitch{property};

        bool state = propertySwitch[0].getState() != ISS_OFF;
        if (deviceName == controlledCCD)
        {
            StatusLP[CCD].setState(state ? IPS_OK : IPS_BUSY);
        }

        if (deviceName == controlledFilterWheel)
        {
            StatusLP[FILTER].setState(state ? IPS_OK : IPS_BUSY);
        }
        StatusLP.apply();
        return;
    }

    if (deviceName == controlledCCD && property.isNameMatch("CCD_EXPOSURE"))
    {
        INDI::PropertyNumber propertyNumber{property};
        ProgressNP[REMAINING_TIME].setValue(propertyNumber[0].getValue());
        ProgressNP.apply();
        return;
    }

    if (deviceName == controlledFilterWheel && property.isNameMatch("FILTER_SLOT"))
    {
        INDI::PropertyNumber propertyNumber{property};
        FilterSlotNP[0].setValue(propertyNumber[0].getValue());
        if (property.getState() == IPS_OK)
            initiateNextCapture();
        return;
    }

    if (deviceName == controlledCCD && property.isNameMatch("CCD_FILE_PATH"))
    {
        INDI::PropertyText propertyText(property);
        char name[128] = {0};

        strncpy(format, strrchr(propertyText[0].getText(), '.'), sizeof(format));
        sprintf(name, IMAGE_NAME, ImageNameTP[IMAGE_FOLDER].getText(), ImageNameTP[IMAGE_NAME_PREFIX].getText(), group, image, format);
        rename(propertyText[0].getText(), name);
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
                ProgressNP[GROUP].setValue(group = group + 1);
                ProgressNP[IMAGE].setValue(image = 1);
                ProgressNP.apply();
                initiateNextFilter();
            }
        }
        else
        {
            ProgressNP[IMAGE].setValue(image = image + 1);
            ProgressNP.apply();
            initiateNextFilter();
        }
        return;
    }
}

void Imager::serverDisconnected(int exit_code)
{
    INDI_UNUSED(exit_code);
    LOG_DEBUG("Server disconnected");
    StatusLP[CCD].setState(IPS_ALERT);
    StatusLP[FILTER].setState(IPS_ALERT);
}


