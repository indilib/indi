/*
 ASI Filter Wheel INDI Driver

 Copyright (c) 2016 by Rumen G.Bogdanovski.
 All Rights Reserved.

 Contributors:
 Yang Zhou
 Hans Lambermont

 Code is based on SX Filter Wheel INDI Driver by Gerry Rozema
 Copyright(c) 2010 Gerry Rozema.
 All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include "asi_wheel.h"

#include "config.h"

#include <string.h>
#include <unistd.h>

#define MAX_DEVICES 16  /* Max device cameraCount */

static int num_wheels;
static ASIWHEEL *wheels[MAX_DEVICES];

void ASI_EFW_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        num_wheels = 0;

        num_wheels = EFWGetNum();
        if (num_wheels > MAX_DEVICES)
            num_wheels = MAX_DEVICES;

        if (num_wheels <= 0)
        {
            IDLog("No ASI EFW detected.");
        }
        else
        {
            int num_wheels_ok = 0;
            for (int i = 0; i < num_wheels; i++)
            {
                int id;
                EFW_ERROR_CODE result = EFWGetID(i, &id);
                if (result != EFW_SUCCESS) {
                    IDLog("ERROR: ASI EFW %d EFWGetID error %d.", i+1, result);
                    continue;
                }
                EFW_INFO info;
                result = EFWGetProperty(id, &info);
                if (result != EFW_SUCCESS && result != EFW_ERROR_CLOSED) { // TODO: remove the ERROR_CLOSED hack
                    IDLog("ERROR: ASI EFW %d EFWGetProperty error %d.", i+1, result);
                    continue;
                }
                /* Enumerate FWs if more than one ASI EFW is connected */
                wheels[i] = new ASIWHEEL(id, info, (bool)(num_wheels - 1));
                num_wheels_ok++;
            }
            IDLog("%d ASI EFW attached out of %d detected.", num_wheels_ok, num_wheels);
            if (num_wheels == num_wheels_ok)
                isInit = true;
        }
    }
}

void ISGetProperties(const char *dev)
{
    ASI_EFW_ISInit();
    for (int i = 0; i < num_wheels; i++)
    {
        ASIWHEEL *wheel = wheels[i];
        if (dev == nullptr || !strcmp(dev, wheel->name))
        {
            wheel->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ASI_EFW_ISInit();
    for (int i = 0; i < num_wheels; i++)
    {
        ASIWHEEL *wheel = wheels[i];
        if (dev == nullptr || !strcmp(dev, wheel->name))
        {
            wheel->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ASI_EFW_ISInit();
    for (int i = 0; i < num_wheels; i++)
    {
        ASIWHEEL *wheel = wheels[i];
        if (dev == nullptr || !strcmp(dev, wheel->name))
        {
            wheel->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ASI_EFW_ISInit();
    for (int i = 0; i < num_wheels; i++)
    {
        ASIWHEEL *wheel = wheels[i];
        if (dev == nullptr || !strcmp(dev, wheel->name))
        {
            wheel->ISNewNumber(dev, name, values, names, num);
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
    ASI_EFW_ISInit();
    for (int i = 0; i < num_wheels; i++)
    {
        ASIWHEEL *wheel = wheels[i];
        wheel->ISSnoopDevice(root);
    }
}

ASIWHEEL::ASIWHEEL(int id, EFW_INFO info, bool enumerate)
{
    char str[MAXINDIDEVICE];

    if (enumerate)
        snprintf(str, MAXINDIDEVICE, "ASI %s %d", info.Name, id);
    else
        snprintf(str, MAXINDIDEVICE, "ASI %s", info.Name);

    fw_id              = id;
    CurrentFilter      = 0;
    FilterSlotN[0].min = 0;
    FilterSlotN[0].max = 0;
    strncpy(name, str, MAXINDIDEVICE);
    setDeviceName(str);
    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);

    LOGF_DEBUG("FW ID: %d FW Name: %s enumerate? %s", id, info.Name, enumerate ? "true":"false");
}

ASIWHEEL::~ASIWHEEL()
{
    Disconnect();
}

const char *ASIWHEEL::getDefaultName()
{
    return (char *)"ASI EFW";
}

bool ASIWHEEL::Connect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation connected.");
        fw_id = 0;
    }
    else if (fw_id >= 0)
    {
        EFW_ERROR_CODE result = EFWOpen(fw_id);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWOpen() = %d", __FUNCTION__, result);
            return false;
        }

        EFW_INFO info;
        result = EFWGetProperty(fw_id, &info);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetProperty() = %d", __FUNCTION__, result);
            return false;
        }
        FilterSlotN[0].min = 1;
        FilterSlotN[0].max = info.slotNum;

        // get current filter
        int current;
        result = EFWGetPosition(fw_id, &current);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetPosition() = %d", __FUNCTION__, result);
            return false;
        }
        SelectFilter(current + 1);
        LOGF_DEBUG("%s(): current filter position %d", __FUNCTION__, CurrentFilter);
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

bool ASIWHEEL::Disconnect()
{
    EFW_ERROR_CODE result = EFW_SUCCESS;

    if (isSimulation())
    {
        LOG_INFO("Simulation disconnected.");
    }
    else if (fw_id >= 0)
    {
        result = EFWClose(fw_id);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWClose() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    // NOTE: do not unset fw_id here, otherwise we cannot reconnect without reloading the driver
    return true;
}

bool ASIWHEEL::initProperties()
{
    INDI::FilterWheel::initProperties();
    addAuxControls();
    setDefaultPollingPeriod(250);
    return true;
}

int ASIWHEEL::QueryFilter()
{
    if (isSimulation())
     return CurrentFilter;
    
    if (fw_id >= 0)
    {
        EFW_ERROR_CODE result;
        result = EFWGetPosition(fw_id, &CurrentFilter);
        if (result != EFW_SUCCESS)
        {
            LOGF_ERROR("%s(): EFWGetPosition() = %d", __FUNCTION__, result);
            return 0;
        }
        CurrentFilter++;
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return 0;
    }

    return CurrentFilter;
}

bool ASIWHEEL::SelectFilter(int f)
{
    TargetFilter = f;
    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        return true;
    }
 
    if (fw_id >= 0)
    {
        EFW_ERROR_CODE result;
        result = EFWSetPosition(fw_id, f - 1);
        if (result == EFW_SUCCESS)
        {
            SetTimer(POLLMS);
            do
            {
                result = EFWGetPosition(fw_id, &CurrentFilter);
                CurrentFilter++;
                usleep(POLLMS * 1000);
            } while (result == EFW_SUCCESS && CurrentFilter != TargetFilter);
            if (result != EFW_SUCCESS)
            {
                LOGF_ERROR("%s(): EFWSetPosition() = %d", __FUNCTION__, result);
                return false;
            }
        }
        else
        {
            LOGF_ERROR("%s(): EFWSetPosition() = %d", __FUNCTION__, result);
            return false;
        }
    }
    else
    {
        LOGF_INFO("%s(): no filter wheel known, fw_id = %d", __FUNCTION__, fw_id);
        return false;
    }
    return true;
}

void ASIWHEEL::TimerHit()
{
    QueryFilter();
    if (CurrentFilter != TargetFilter)
    {
        SetTimer(POLLMS);
    }
    else
    {
        SelectFilterDone(CurrentFilter);
    }
}
