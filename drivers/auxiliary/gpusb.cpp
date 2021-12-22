/*******************************************************************************
  Copyright(c) 2012-2019 Jasem Mutlaq. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "gpusb.h"

#include "gpdriver.h"

#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to gpGuide.
static std::unique_ptr<GPUSB> gpGuide(new GPUSB());

GPUSB::GPUSB()
{
    driver = new GPUSBDriver();
}

GPUSB::~GPUSB()
{
    delete (driver);
}

const char *GPUSB::getDefaultName()
{
    return "GPUSB";
}

bool GPUSB::Connect()
{
    driver->setDebug(isDebug());

    bool rc = driver->Connect();

    if (rc)
        LOG_INFO("GPUSB is online.");
    else
        LOG_ERROR("Error: cannot find GPUSB device.");

    return rc;
}

bool GPUSB::Disconnect()
{
    LOG_INFO("GPSUSB is offline.");

    return driver->Disconnect();
}

bool GPUSB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    initGuiderProperties(getDeviceName(), MAIN_CONTROL_TAB);

    addDebugControl();

    setDriverInterface(AUX_INTERFACE | GUIDER_INTERFACE);

    setDefaultPollingPeriod(250);

    return true;
}

bool GPUSB::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
    }

    return true;
}

bool GPUSB::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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

void GPUSB::debugTriggered(bool enable)
{
    driver->setDebug(enable);
}

//float GPUSB::CalcWEPulseTimeLeft()
//{
//    double timesince;
//    double timeleft;
//    struct timeval now
//    {
//        0, 0
//    };
//    gettimeofday(&now, nullptr);

//    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
//                (double)(WEPulseStart.tv_sec * 1000.0 + WEPulseStart.tv_usec / 1000);
//    timesince = timesince / 1000;

//    timeleft = WEPulseRequest - timesince;
//    return timeleft;
//}

//float GPUSB::CalcNSPulseTimeLeft()
//{
//    double timesince;
//    double timeleft;
//    struct timeval now
//    {
//        0, 0
//    };
//    gettimeofday(&now, nullptr);

//    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
//                (double)(NSPulseStart.tv_sec * 1000.0 + NSPulseStart.tv_usec / 1000);
//    timesince = timesince / 1000;

//    timeleft = NSPulseRequest - timesince;
//    return timeleft;
//}


IPState GPUSB::GuideNorth(uint32_t ms)
{
    RemoveTimer(NSTimerID);

    driver->startPulse(GPUSB_NORTH);

    NSDirection = GPUSB_NORTH;

    LOG_DEBUG("Starting NORTH guide");

    NSPulseRequest = ms;

    NSGuideTS = std::chrono::system_clock::now();

    NSTimerID = IEAddTimer(ms, &GPUSB::NSTimerHelper, this);

    return IPS_BUSY;
}

IPState GPUSB::GuideSouth(uint32_t ms)
{
    RemoveTimer(NSTimerID);

    driver->startPulse(GPUSB_SOUTH);

    NSDirection = GPUSB_SOUTH;

    LOG_DEBUG("Starting SOUTH guide");

    NSPulseRequest = ms;

    NSGuideTS = std::chrono::system_clock::now();

    NSTimerID = IEAddTimer(ms, &GPUSB::NSTimerHelper, this);

    return IPS_BUSY;
}

IPState GPUSB::GuideEast(uint32_t ms)
{
    RemoveTimer(WETimerID);

    driver->startPulse(GPUSB_EAST);

    WEDirection = GPUSB_EAST;

    LOG_DEBUG("Starting EAST guide");

    WEPulseRequest = ms;

    WEGuideTS = std::chrono::system_clock::now();

    WETimerID = IEAddTimer(ms, &GPUSB::WETimerHelper, this);

    return IPS_BUSY;
}

IPState GPUSB::GuideWest(uint32_t ms)
{
    RemoveTimer(WETimerID);

    driver->startPulse(GPUSB_WEST);

    WEDirection = GPUSB_WEST;

    LOG_DEBUG("Starting WEST guide");

    WEPulseRequest = ms;

    WEGuideTS = std::chrono::system_clock::now();

    WETimerID = IEAddTimer(ms, &GPUSB::WETimerHelper, this);

    return IPS_BUSY;
}

void GPUSB::NSTimerHelper(void *context)
{
    static_cast<GPUSB*>(context)->NSTimerCallback();
}

void GPUSB::WETimerHelper(void *context)
{
    static_cast<GPUSB*>(context)->WETimerCallback();
}

void GPUSB::NSTimerCallback()
{
    driver->stopPulse(NSDirection);
    GuideComplete(AXIS_DE);
}

void GPUSB::WETimerCallback()
{
    driver->stopPulse(WEDirection);
    GuideComplete(AXIS_RA);
}

