/*
 ATIK CCD & Filter Wheel Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "atik_wheel.h"

#include "config.h"

#include <algorithm>
#include <math.h>
#include <unistd.h>
#include <string.h>

#define TEMP_TIMER_MS           1000 /* Temperature polling time (ms) */
#define MAX_DEVICES             4    /* Max device filterWheelCount */

static int iAvailablefilterWheelsCount;
static ATIKWHEEL *filterWheels[MAX_DEVICES];

static void cleanup()
{
    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        delete filterWheels[i];
    }
}

void ATIK_WHEEL_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iAvailablefilterWheelsCount = 0;
        std::vector<std::string> filterWheelNames;

        iAvailablefilterWheelsCount = MAX_DEVICES;
        for (int i = 0; i < iAvailablefilterWheelsCount; i++)
        {
            // We only do filterWheels in this driver.
            if (ArtemisEFWIsPresent(i) == false)
                continue;

            ARTEMISEFWTYPE type;
            char *serialNumber = new char[MAXINDIDEVICE];
            int rc = ArtemisEFWGetDeviceDetails(i, &type, serialNumber);
            delete [] serialNumber;

            if (rc != ARTEMIS_OK)
            {
                IDLog("ArtemisEFWGetDeviceDetails for device %d failed with errpr %d.", i, rc);
                continue;
            }

            const char *fwName = (type == ARTEMIS_EFW1) ? "EFW1" : "EFW2";

            std::string filterWheelName;

            if (std::find(filterWheelNames.begin(), filterWheelNames.end(), fwName) == filterWheelNames.end())
                filterWheelName = "Atik " + std::string(fwName);
            else
                filterWheelName = "Atik " + std::string(fwName) + " " +
                                  std::to_string(static_cast<int>(std::count(filterWheelNames.begin(), filterWheelNames.end(), fwName)) + 1);

            filterWheels[i] = new ATIKWHEEL(filterWheelName, i);
            filterWheelNames.push_back(fwName);
        }

        if (filterWheelNames.empty())
        {
            iAvailablefilterWheelsCount = 0;
            return;
        }
        else
            iAvailablefilterWheelsCount = filterWheelNames.size();

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ATIK_WHEEL_ISInit();

    if (iAvailablefilterWheelsCount == 0)
    {
        IDMessage(nullptr, "No Atik filter wheels detected. Power on?");
        return;
    }

    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        ATIKWHEEL *filterWheel = filterWheels[i];
        if (dev == nullptr || !strcmp(dev, filterWheel->name))
        {
            filterWheel->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ATIK_WHEEL_ISInit();
    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        ATIKWHEEL *filterWheel = filterWheels[i];
        if (dev == nullptr || !strcmp(dev, filterWheel->name))
        {
            filterWheel->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ATIK_WHEEL_ISInit();
    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        ATIKWHEEL *filterWheel = filterWheels[i];
        if (dev == nullptr || !strcmp(dev, filterWheel->name))
        {
            filterWheel->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ATIK_WHEEL_ISInit();
    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        ATIKWHEEL *filterWheel = filterWheels[i];
        if (dev == nullptr || !strcmp(dev, filterWheel->name))
        {
            filterWheel->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
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
    ATIK_WHEEL_ISInit();

    for (int i = 0; i < iAvailablefilterWheelsCount; i++)
    {
        ATIKWHEEL *filterWheel = filterWheels[i];
        filterWheel->ISSnoopDevice(root);
    }
}

ATIKWHEEL::ATIKWHEEL(std::string filterWheelName, int id) : FilterWheel(), m_iDevice(id)
{
    setVersion(ATIK_VERSION_MAJOR, ATIK_VERSION_MINOR);
    strncpy(this->name, filterWheelName.c_str(), MAXINDIDEVICE);
    setDeviceName(this->name);
}

const char *ATIKWHEEL::getDefaultName()
{
    return "Atik";
}

bool ATIKWHEEL::initProperties()
{
    INDI::FilterWheel::initProperties();

    addDebugControl();

    return true;
}

bool ATIKWHEEL::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", name);

    hWheel = ArtemisEFWConnect(m_iDevice);

    if (hWheel == nullptr)
    {
        LOGF_ERROR("Failed to connected to %s", name);
        return false;
    }

    SetTimer(POLLMS);

    return setupParams();
}

bool ATIKWHEEL::setupParams()
{
    ARTEMISEFWTYPE type;
    char *serialNumber = new char[MAXINDIDEVICE];

    int rc = ArtemisEFWGetDetails(hWheel, &type, serialNumber);
    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to inquire filter wheel properties (%d)", rc);
        return false;
    }

    LOGF_INFO("Detected %s Serial Number %s", type == ARTEMIS_EFW1 ? "EFW1" : "EFW2", serialNumber);
    delete[] serialNumber;

    int numOfFilter = 0;
    rc = ArtemisEFWNmrPosition(hWheel, &numOfFilter);
    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Failed to inquire filter wheel max position (%d)", rc);
        return false;
    }

    CurrentFilter = QueryFilter();
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = numOfFilter;
    FilterSlotN[0].value = CurrentFilter;

    return true;
}

bool ATIKWHEEL::Disconnect()
{
    ArtemisEFWDisconnect(hWheel);
    return true;
}

void ATIKWHEEL::TimerHit()
{
    if (FilterSlotNP.s == IPS_BUSY)
    {
        CurrentFilter = QueryFilter();
        if (TargetFilter == CurrentFilter)
        {
            SelectFilterDone(CurrentFilter);
        }
    }

    SetTimer(POLLMS);
}

bool ATIKWHEEL::SelectFilter(int targetFilter)
{
    TargetFilter = targetFilter;
    int rc = ArtemisEFWSetPosition(hWheel, targetFilter - 1 );
    return (rc == ARTEMIS_OK);
}

int ATIKWHEEL::QueryFilter()
{
    int iPosition = 0;
    bool isMoving = false;
    int rc = ArtemisEFWGetPosition(hWheel, &iPosition, &isMoving);

    if (rc != ARTEMIS_OK)
    {
        LOGF_ERROR("Querying internal filter wheel failed (%d).", rc);
        return -1;
    }

    LOGF_DEBUG("iPosition: %d isMoving: %d", iPosition, isMoving);

    return iPosition + 1;
}

