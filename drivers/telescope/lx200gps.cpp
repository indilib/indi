/*
    LX200 GPS
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200gps.h"

#include "lx200driver.h"

#include <cstring>
#include <unistd.h>

#define GPS_TAB "Extended GPS Features"

LX200GPS::LX200GPS() : LX200Autostar()
{
    MaxReticleFlashRate = 9;
}

const char *LX200GPS::getDefaultName()
{
    return (const char *)"LX200 GPS";
}

bool LX200GPS::initProperties()
{
    LX200Autostar::initProperties();

    GPSPowerSP[0].fill("On", "", ISS_OFF);
    GPSPowerSP[1].fill("Off", "", ISS_OFF);
    GPSPowerSP.fill(getDeviceName(), "GPS Power", "", GPS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    GPSStatusSP[0].fill("Sleep", "", ISS_OFF);
    GPSStatusSP[1].fill("Wake Up", "", ISS_OFF);
    GPSStatusSP[2].fill("Restart", "", ISS_OFF);
    GPSStatusSP.fill(getDeviceName(), "GPS Status", "", GPS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    GPSUpdateSP[0].fill("Update GPS", "", ISS_OFF);
    GPSUpdateSP[1].fill("Update Client", "", ISS_OFF);
    GPSUpdateSP.fill(getDeviceName(), "GPS System", "", GPS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    AltDecPecSP[0].fill("Enable", "", ISS_OFF);
    AltDecPecSP[1].fill("Disable", "", ISS_OFF);
    AltDecPecSP.fill(getDeviceName(), "Alt/Dec PEC", "", GPS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    AzRaPecSP[0].fill("Enable", "", ISS_OFF);
    AzRaPecSP[1].fill("Disable", "", ISS_OFF);
    AzRaPecSP.fill(getDeviceName(), "Az/RA PEC", "", GPS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    SelenSyncSP[0].fill("Sync", "", ISS_OFF);
    SelenSyncSP.fill(getDeviceName(), "Selenographic Sync", "", GPS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    AltDecBacklashSP[0].fill("Activate", "", ISS_OFF);
    AltDecBacklashSP.fill(getDeviceName(), "Alt/Dec Anti-backlash", "", GPS_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    AzRaBacklashSP[0].fill("Activate", "", ISS_OFF);
    AzRaBacklashSP.fill(getDeviceName(), "Az/Ra Anti-backlash", "", GPS_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    OTAUpdateSP[0].fill("Update", "", ISS_OFF);
    OTAUpdateSP.fill(getDeviceName(), "OTA Update", "", GPS_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

    OTATempNP[0].fill("Temp", "", "%03g", -200.0, 500.0, 0.0, 0);
    OTATempNP.fill(getDeviceName(), "OTA Temp (C)", "", GPS_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

void LX200GPS::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    // process parent first
    LX200Autostar::ISGetProperties(dev);

    /*
    if (isConnected())
    {
        defineProperty(GPSPowerSP);
        defineProperty(GPSStatusSP);
        defineProperty(GPSUpdateSP);
        defineProperty(AltDecPecSP);
        defineProperty(AzRaPecSP);
        defineProperty(SelenSyncSP);
        defineProperty(AltDecBacklashSP);
        defineProperty(AzRaBacklashSP);
        defineProperty(OTATempNP);
        defineProperty(OTAUpdateSP);
    }
    */
}

bool LX200GPS::updateProperties()
{
    LX200Autostar::updateProperties();

    if (isConnected())
    {
        defineProperty(GPSPowerSP);
        defineProperty(GPSStatusSP);
        defineProperty(GPSUpdateSP);
        defineProperty(AltDecPecSP);
        defineProperty(AzRaPecSP);
        defineProperty(SelenSyncSP);
        defineProperty(AltDecBacklashSP);
        defineProperty(AzRaBacklashSP);
        defineProperty(OTATempNP);
        defineProperty(OTAUpdateSP);
    }
    else
    {
        deleteProperty(GPSPowerSP.getName());
        deleteProperty(GPSStatusSP.getName());
        deleteProperty(GPSUpdateSP.getName());
        deleteProperty(AltDecPecSP.getName());
        deleteProperty(AzRaPecSP.getName());
        deleteProperty(SelenSyncSP.getName());
        deleteProperty(AltDecBacklashSP.getName());
        deleteProperty(AzRaBacklashSP.getName());
        deleteProperty(OTATempNP.getName());
        deleteProperty(OTAUpdateSP.getName());
    }

    return true;
}

bool LX200GPS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;
    char msg[64];

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /* GPS Power */
        if (GPSPowerSP.isNameMatch(name))
        {
            int ret = 0;

            if (!GPSPowerSP.update(states, names, n))
                return false;

            index = GPSPowerSP.findOnSwitchIndex();
            if (index == 0)
                ret = turnGPSOn(PortFD);
            else
                ret = turnGPSOff(PortFD);

            GPSPowerSP.setState(IPS_OK);
            GPSPowerSP.apply(index == 0 ? "GPS System is ON" : "GPS System is OFF");
            return true;
        }

        /* GPS Status Update */
        if (GPSStatusSP.isNameMatch(name))
        {
            int ret = 0;

            if (!GPSStatusSP.update(states, names, n))
                return false;

            index = GPSStatusSP.findOnSwitchIndex();

            if (index == 0)
            {
                ret = gpsSleep(PortFD);
                strncpy(msg, "GPS system is in sleep mode.", 64);
            }
            else if (index == 1)
            {
                ret = gpsWakeUp(PortFD);
                strncpy(msg, "GPS system is reactivated.", 64);
            }
            else
            {
                ret = gpsRestart(PortFD);
                strncpy(msg, "GPS system is restarting...", 64);
                sendScopeTime();
                sendScopeLocation();
            }

            GPSStatusSP.setState(IPS_OK);
            GPSStatusSP.apply("%s", msg);
            return true;
        }

        /* GPS Update */
        if (GPSUpdateSP.isNameMatch(name))
        {
            if (!GPSUpdateSP.update(states, names, n))
                return false;

            index = GPSUpdateSP.findOnSwitchIndex();

            GPSUpdateSP.setState(IPS_OK);

            if (index == 0)
            {
                GPSUpdateSP.apply("Updating GPS system. This operation might take few minutes to complete...");
                if (updateGPS_System(PortFD))
                {
                    GPSUpdateSP.apply("GPS system update successful.");
                    sendScopeTime();
                    sendScopeLocation();
                }
                else
                {
                    GPSUpdateSP.setState(IPS_IDLE);
                    GPSUpdateSP.apply("GPS system update failed.");
                }
            }
            else
            {
                sendScopeTime();
                sendScopeLocation();
                GPSUpdateSP.apply("Client time and location is synced to LX200 GPS Data.");
            }
            return true;
        }

        /* Alt Dec Periodic Error correction */
        if (AltDecPecSP.isNameMatch(name))
        {
            int ret = 0;

            if (!AltDecPecSP.update(states, names, n))
                return false;

            index = AltDecPecSP.findOnSwitchIndex();

            if (index == 0)
            {
                ret = enableDecAltPec(PortFD);
                strncpy(msg, "Alt/Dec Compensation Enabled.", 64);
            }
            else
            {
                ret = disableDecAltPec(PortFD);
                strncpy(msg, "Alt/Dec Compensation Disabled.", 64);
            }

            AltDecPecSP.setState(IPS_OK);
            AltDecPecSP.apply("%s", msg);

            return true;
        }

        /* Az RA periodic error correction */
        if (AzRaPecSP.isNameMatch(name))
        {
            int ret = 0;

            if (!AzRaPecSP.update(states, names, n))
                return false;

            index = AzRaPecSP.findOnSwitchIndex();

            if (index == 0)
            {
                ret = enableRaAzPec(PortFD);
                strncpy(msg, "Ra/Az Compensation Enabled.", 64);
            }
            else
            {
                ret = disableRaAzPec(PortFD);
                strncpy(msg, "Ra/Az Compensation Disabled.", 64);
            }

            AzRaPecSP.setState(IPS_OK);
            AzRaPecSP.apply("%s", msg);

            return true;
        }

        if (AltDecBacklashSP.isNameMatch(name))
        {
            int ret = 0;

            ret = activateAltDecAntiBackSlash(PortFD);
            AltDecBacklashSP.setState(IPS_OK);
            AltDecBacklashSP.apply("Alt/Dec Anti-backlash enabled");
            return true;
        }

        if (AzRaBacklashSP.isNameMatch(name))
        {
            int ret = 0;

            ret = activateAzRaAntiBackSlash(PortFD);
            AzRaBacklashSP.setState(IPS_OK);
            AzRaBacklashSP.apply("Az/Ra Anti-backlash enabled");
            return true;
        }

        if (OTAUpdateSP.isNameMatch(name))
        {
            OTAUpdateSP.reset();

            if (getOTATemp(PortFD, &OTATempNP[0].value) < 0)
            {
                OTAUpdateSP.setState(IPS_ALERT);
                OTATempNP.setState(IPS_ALERT);
                OTATempNP.apply("Error: OTA temperature read timed out.");
                return false;
            }
            else
            {
                OTAUpdateSP.setState(IPS_OK);
                OTATempNP.setState(IPS_OK);
                OTATempNP.apply();
                OTAUpdateSP.apply();
                return true;
            }
        }
    }

    return LX200Autostar::ISNewSwitch(dev, name, states, names, n);
}

bool LX200GPS::updateTime(ln_date *utc, double utc_offset)
{
    ln_zonedate ltm;

    if (isSimulation())
        return true;

    JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %.2f", JD);

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600);

    LOGF_DEBUG("Local time is %02d:%02d:%02g", ltm.hours, ltm.minutes, ltm.seconds);

    // Set Local Time
    if (setLocalTime24(ltm.hours, ltm.minutes, ltm.seconds) == false)
    {
        LOG_ERROR("Error setting local time time.");
        return false;
    }

    // UTC Date, it's not Local for LX200GPS
    if (setLocalDate(utc->days, utc->months, utc->years) == false)
    {
        LOG_ERROR("Error setting UTC date.");
        return false;
    }

    // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
    // is the opposite of the standard definition of UTC offset!
    if (setUTCOffset(utc_offset) == false)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }

    LOG_INFO("Time updated, updating planetary data...");
    return true;
}

bool LX200GPS::UnPark()
{
    int ret = 0;

    ret = initTelescope(PortFD);
    TrackState = SCOPE_IDLE;
    return true;
}
