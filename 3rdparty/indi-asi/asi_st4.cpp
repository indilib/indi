/*
 ASI ST4 Driver

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

#include "asi_st4.h"
#include "config.h"

#include <math.h>
#include <string.h>
#include <unistd.h>

#define MAX_DEVICES             4    /* Max device cameraCount */

static int iConnectedST4Count;
static ASIST4 *st4s[MAX_DEVICES];

static void cleanup()
{
    for (int i = 0; i < iConnectedST4Count; i++)
    {
        delete st4s[i];
    }   
}

void ASI_ST4_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iConnectedST4Count = USB2ST4GetNum();
        if (iConnectedST4Count > MAX_DEVICES)
            iConnectedST4Count = MAX_DEVICES;
        if (iConnectedST4Count <= 0)
            IDLog("No ASI ST4 detected. Power on?");
        else
        {
            for (int i = 0; i < iConnectedST4Count; i++)
            {
                int id=0;
                USB2ST4GetID(i, &id);
                st4s[i] = new ASIST4(id);
            }
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ASI_ST4_ISInit();

    if (iConnectedST4Count == 0)
    {
        IDMessage(nullptr, "No ASI ST4 detected.");
        return;
    }

    for (int i = 0; i < iConnectedST4Count; i++)
    {
        ASIST4 *st4 = st4s[i];
        if (dev == nullptr || !strcmp(dev, st4->name))
        {
            st4->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ASI_ST4_ISInit();
    for (int i = 0; i < iConnectedST4Count; i++)
    {
        ASIST4 *st4 = st4s[i];
        if (dev == nullptr || !strcmp(dev, st4->name))
        {
            st4->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ASI_ST4_ISInit();
    for (int i = 0; i < iConnectedST4Count; i++)
    {
        ASIST4 *st4 = st4s[i];
        if (dev == nullptr || !strcmp(dev, st4->name))
        {
            st4->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ASI_ST4_ISInit();
    for (int i = 0; i < iConnectedST4Count; i++)
    {
        ASIST4 *st4 = st4s[i];
        if (dev == nullptr || !strcmp(dev, st4->name))
        {
            st4->ISNewNumber(dev, name, values, names, num);
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
    ASI_ST4_ISInit();

    for (int i = 0; i < iConnectedST4Count; i++)
    {
        ASIST4 *st4 = st4s[i];
        st4->ISSnoopDevice(root);
    }
}

ASIST4::ASIST4(int id)
{
    this->ID = id;

    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);

    WEPulseRequest = NSPulseRequest = 0;
    WEtimerID = NStimerID = -1;
    NSDir = USB2ST4_NORTH;
    WEDir = USB2ST4_WEST;

    snprintf(this->name, MAXINDIDEVICE, "ZWO ST4 %d", id);
    setDeviceName(this->name);
}

const char *ASIST4::getDefaultName()
{
    return "ZWO ST4";
}

bool ASIST4::initProperties()
{
    INDI::DefaultDevice::initProperties();

    initGuiderProperties(getDeviceName(), MAIN_CONTROL_TAB);

    addDebugControl();

    return true;
}

bool ASIST4::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
    }

    return true;
}

bool ASIST4::Connect()
{
    LOGF_DEBUG("Attempting to open %s...", name);

    USB2ST4_ERROR_CODE rc = USB2ST4Open(ID);

    if (rc != USB2ST4_SUCCESS)
    {
        LOGF_ERROR("Error connecting to USB2ST4 adapter (%d)", rc);
        return false;
    }

    return true;
}

bool ASIST4::Disconnect()
{    
    LOGF_DEBUG("Closing %s...", name);

    USB2ST4Close(ID);

    return true;
}


bool ASIST4::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/* Helper function for NS timer call back */
void ASIST4::TimerHelperNS(void *context)
{
    static_cast<ASIST4*>(context)->TimerNS();
}

/* The timer call back for NS guiding */
void ASIST4::TimerNS()
{
    NStimerID = -1;
    double timeleft = calcTimeLeft(NSPulseRequest, &NSPulseStart);
    if (timeleft >= 0.000001)
    {
        if (timeleft < 0.001)
        {
            uint32_t uSecs = static_cast<uint32_t>(timeleft * 1000000.0);
            usleep(uSecs);
        }
        else
        {
            uint32_t mSecs = static_cast<uint32_t>(timeleft * 1000.0);
            NStimerID = IEAddTimer(mSecs, ASIST4::TimerHelperNS, this);
            return;
        }
    }
    USB2ST4PulseGuide(ID, NSDir, false);
    LOGF_DEBUG("Stopping %s guide.", NSDirName);
    GuideComplete(AXIS_DE);
}

/* Stop the timer for NS guiding */
void ASIST4::stopTimerNS()
{
    if (NStimerID != -1)
    {
      USB2ST4PulseGuide(ID, NSDir, false);
      GuideComplete(AXIS_DE);
      IERmTimer(NStimerID);
      NStimerID = -1;
    }
}

IPState ASIST4::guidePulseNS(uint32_t ms, USB2ST4_DIRECTION dir, const char *dirName)
{
    stopTimerNS();
    NSDir = dir;
    NSDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %d ms", NSDirName, ms);

    /*
     * If the pulse is for a ms or longer then schedule a timer callback
     * to turn off the pulse, otherwise wait here to turn it off
     */
    uint32_t mSecs = 0;
    uint32_t uSecs = 0;
    if (ms >= 1)
    {
        mSecs = ms;
        NSPulseRequest = ms / 1000;
        gettimeofday(&NSPulseStart, nullptr);
    }
    else
    {
        uSecs = ms * 1000;
    }

    USB2ST4PulseGuide(ID, NSDir, true);
    if (uSecs != 0)
    {
        usleep(uSecs);
        USB2ST4PulseGuide(ID, NSDir, false);
        LOGF_DEBUG("Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        NStimerID = IEAddTimer(mSecs, ASIST4::TimerHelperNS, this);
        return IPS_BUSY;
    }
}

IPState ASIST4::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, USB2ST4_NORTH, "North");
}

IPState ASIST4::GuideSouth(uint32_t ms)
{
    return guidePulseNS(ms, USB2ST4_SOUTH, "South");
}

/* Helper function for WE timer call back */
void ASIST4::TimerHelperWE(void *context)
{
    static_cast<ASIST4 *>(context)->TimerWE();
}

/* The timer call back for WE guiding */
void ASIST4::TimerWE()
{
    WEtimerID = -1;
    double timeleft = calcTimeLeft(WEPulseRequest, &WEPulseStart);
    if (timeleft >= 0.000001)
    {
        if (timeleft < 0.001)
        {
            uint32_t uSecs = static_cast<uint32_t>(timeleft * 1000000.0);
            usleep(uSecs);
        }
        else
        {
            int mSecs = static_cast<int>(timeleft * 1000.0);
            WEtimerID = IEAddTimer(mSecs, ASIST4::TimerHelperWE, this);
            return;
        }
    }
    USB2ST4PulseGuide(ID, WEDir, false);
    LOGF_DEBUG("Stopping %s guide.", WEDirName);
    GuideComplete(AXIS_RA);
}

void ASIST4::stopTimerWE()
{
    if (WEtimerID != -1)
    {
      USB2ST4PulseGuide(ID, WEDir, false);
      GuideComplete(AXIS_RA);
      IERmTimer(WEtimerID);
      WEtimerID = -1;
    }
}

IPState ASIST4::guidePulseWE(uint32_t ms, USB2ST4_DIRECTION dir, const char *dirName)
{
    stopTimerWE();
    WEDir = dir;
    WEDirName = dirName;

    LOGF_DEBUG("Starting %s guide for %d ms",  WEDirName, ms);

    /*
     * If the pulse is for a ms or longer then schedule a timer callback
     * to turn off the pulse, otherwise wait here to turn it off
     */
    uint32_t mSecs = 0;
    uint32_t uSecs = 0;
    if (ms >= 1)
    {
        mSecs = ms;
        WEPulseRequest = ms / 1000;
        gettimeofday(&WEPulseStart, nullptr);
    }
    else
    {
        uSecs = ms * 1000;
    }

    USB2ST4PulseGuide(ID, WEDir, true);
    if (uSecs != 0)
    {
        usleep(uSecs);
        USB2ST4PulseGuide(ID, WEDir, false);
        LOGF_DEBUG("Stopped %s guide.", dirName);
        return IPS_OK;
    }
    else
    {
        WEtimerID = IEAddTimer(mSecs, ASIST4::TimerHelperWE, this);
        return IPS_BUSY;
    }
}

IPState ASIST4::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, USB2ST4_EAST, "East");
}

IPState ASIST4::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, USB2ST4_WEST, "West");
}

double ASIST4::calcTimeLeft(double duration, timeval *start_time)
{
    double timeleft=0;
    struct timeval now;

    gettimeofday(&now, nullptr);

    double timesince = (now.tv_sec + now.tv_usec / 1000000.0) - (start_time->tv_sec + start_time->tv_usec / 1000000.0);

    if (duration > timesince)
        timeleft = duration - timesince;
    else
        timeleft = 0.0;

    return timeleft;
}
