/*
    LX200 16"
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

#include "lx200_16.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <cstring>
#include <unistd.h>

#define LX16_TAB "GPS/16 inch Features"

LX200_16::LX200_16() : LX200GPS()
{
    MaxReticleFlashRate = 3;
}

const char *LX200_16::getDefaultName()
{
    return (const char *)"LX200 16";
}

bool LX200_16::initProperties()
{
    LX200GPS::initProperties();

    FanStatusSP[0].fill("On", "", ISS_OFF);
    FanStatusSP[1].fill("Off", "", ISS_OFF);
    FanStatusSP.fill(getDeviceName(), "Fan", "", LX16_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    HomeSearchSP[0].fill("Save Home", "", ISS_OFF);
    HomeSearchSP[1].fill("Set Home", "", ISS_OFF);
    HomeSearchSP.fill(getDeviceName(), "Home", "", LX16_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    FieldDeRotatorSP[0].fill("On", "", ISS_OFF);
    FieldDeRotatorSP[1].fill("Off", "", ISS_OFF);
    FieldDeRotatorSP.fill(getDeviceName(), "Field De-Rotator", "", LX16_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    HorizontalCoordsNP[0].fill("ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    HorizontalCoordsNP[1].fill("AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    HorizontalCoordsNP.fill(getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    return true;
}

void LX200_16::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    // process parent first
    LX200GPS::ISGetProperties(dev);

    /*
    if (isConnected())
    {
        defineProperty(HorizontalCoordsNP);
        defineProperty(FanStatusSP);
        defineProperty(HomeSearchSP);
        defineProperty(FieldDeRotatorSP);
    }
    */
}

bool LX200_16::updateProperties()
{
    // process parent first
    LX200GPS::updateProperties();

    if (isConnected())
    {
        defineProperty(HorizontalCoordsNP);
        defineProperty(FanStatusSP);
        defineProperty(HomeSearchSP);
        defineProperty(FieldDeRotatorSP);
    }
    else
    {
        deleteProperty(HorizontalCoordsNP.getName());
        deleteProperty(FanStatusSP.getName());
        deleteProperty(HomeSearchSP.getName());
        deleteProperty(FieldDeRotatorSP.getName());
    }

    return true;
}

bool LX200_16::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    double newAlt = 0, newAz = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (HorizontalCoordsNP.isNameMatch(name))
        {
            int i = 0, nset = 0;

            for (nset = i = 0; i < n; i++)
            {
                INumber *horp = HorizontalCoordsNP.findWidgetByName(names[i]);
                if (horp == &HorizontalCoordsNP[0])
                {
                    newAlt = values[i];
                    nset += newAlt >= -90. && newAlt <= 90.0;
                }
                else if (horp == &HorizontalCoordsNP[1])
                {
                    newAz = values[i];
                    nset += newAz >= 0. && newAz <= 360.0;
                }
            }

            if (nset == 2)
            {
                if (!isSimulation() && (setObjAz(PortFD, newAz) < 0 || setObjAlt(PortFD, newAlt) < 0))
                {
                    HorizontalCoordsNP.setState(IPS_ALERT);
                    HorizontalCoordsNP.apply("Error setting Alt/Az.");
                    return false;
                }
                targetAZ  = newAz;
                targetALT = newAlt;

                return handleAltAzSlew();
            }
            else
            {
                HorizontalCoordsNP.setState(IPS_ALERT);
                HorizontalCoordsNP.apply("Altitude or Azimuth missing or invalid");
                return false;
            }
        }
    }

    LX200GPS::ISNewNumber(dev, name, values, names, n);
    return true;
}

bool LX200_16::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (FanStatusSP.isNameMatch(name))
        {
            FanStatusSP.reset();
            FanStatusSP.update(states, names, n);
            index = FanStatusSP.findOnSwitchIndex();

            if (index == 0)
            {
                if (turnFanOn(PortFD) < 0)
                {
                    FanStatusSP.setState(IPS_ALERT);
                    FanStatusSP.apply("Error changing fan status.");
                    return false;
                }
            }
            else
            {
                if (turnFanOff(PortFD) < 0)
                {
                    FanStatusSP.setState(IPS_ALERT);
                    FanStatusSP.apply("Error changing fan status.");
                    return false;
                }
            }

            FanStatusSP.setState(IPS_OK);
            FanStatusSP.apply(index == 0 ? "Fan is ON" : "Fan is OFF");
            return true;
        }

        if (HomeSearchSP.isNameMatch(name))
        {
            int ret = 0;

            HomeSearchSP.reset();
            HomeSearchSP.update(states, names, n);
            index = HomeSearchSP.findOnSwitchIndex();

            if (index == 0)
                ret = seekHomeAndSave(PortFD);
            else
                ret = seekHomeAndSet(PortFD);

            HomeSearchSP.setState(IPS_BUSY);
            HomeSearchSP.apply(index == 0 ? "Seek Home and Save" : "Seek Home and Set");
            return true;
        }

        if (FieldDeRotatorSP.isNameMatch(name))
        {
            int ret = 0;

            FieldDeRotatorSP.reset();
            FieldDeRotatorSP.update(states, names, n);
            index = FieldDeRotatorSP.findOnSwitchIndex();

            if (index == 0)
                ret = turnFieldDeRotatorOn(PortFD);
            else
                ret = turnFieldDeRotatorOff(PortFD);

            FieldDeRotatorSP.setState(IPS_OK);
            FieldDeRotatorSP.apply(index == 0 ? "Field deRotator is ON" : "Field deRotator is OFF");
            return true;
        }
    }

    return LX200GPS::ISNewSwitch(dev, name, states, names, n);
}

bool LX200_16::handleAltAzSlew()
{
    const struct timespec timeout = {0, 100000000L};
    char altStr[64], azStr[64];

    if (HorizontalCoordsNP.getState() == IPS_BUSY)
    {
        abortSlew(PortFD);

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation() && slewToAltAz(PortFD))
    {
        HorizontalCoordsNP.setState(IPS_ALERT);
        HorizontalCoordsNP.apply("Slew is not possible.");
        return false;
    }

    HorizontalCoordsNP.setState(IPS_BUSY);
    fs_sexa(azStr, targetAZ, 2, 3600);
    fs_sexa(altStr, targetALT, 2, 3600);

    TrackState = SCOPE_SLEWING;
    HorizontalCoordsNP.apply("Slewing to Alt %s - Az %s", altStr, azStr);
    return true;
}

bool LX200_16::ReadScopeStatus()
{
    int searchResult = 0;
    double dx, dy;

    LX200Generic::ReadScopeStatus();

    switch (HomeSearchSP.getState())
    {
        case IPS_IDLE:
            break;

        case IPS_BUSY:

            if (isSimulation())
                searchResult = 1;
            else if (getHomeSearchStatus(PortFD, &searchResult) < 0)
            {
                HomeSearchSP.setState(IPS_ALERT);
                HomeSearchSP.apply("Error updating home search status.");
                return false;
            }

            if (searchResult == 0)
            {
                HomeSearchSP.setState(IPS_ALERT);
                HomeSearchSP.apply("Home search failed.");
            }
            else if (searchResult == 1)
            {
                HomeSearchSP.setState(IPS_OK);
                HomeSearchSP.apply("Home search successful.");
            }
            else if (searchResult == 2)
                HomeSearchSP.apply("Home search in progress...");
            else
            {
                HomeSearchSP.setState(IPS_ALERT);
                HomeSearchSP.apply("Home search error.");
            }
            break;

        case IPS_OK:
            break;
        case IPS_ALERT:
            break;
    }

    switch (HorizontalCoordsNP.getState())
    {
        case IPS_IDLE:
            break;

        case IPS_BUSY:

            if (isSimulation())
            {
                currentAZ  = targetAZ;
                currentALT = targetALT;
                TrackState = SCOPE_TRACKING;
                return true;
            }
            if (getLX200Az(PortFD, &currentAZ) < 0 || getLX200Alt(PortFD, &currentALT) < 0)
            {
                HorizontalCoordsNP.setState(IPS_ALERT);
                HorizontalCoordsNP.apply("Error geting Alt/Az.");
                return false;
            }

            dx = targetAZ - currentAZ;
            dy = targetALT - currentALT;

            HorizontalCoordsNP[0].setValue(currentALT);
            HorizontalCoordsNP[1].setValue(currentAZ);

            // accuracy threshold (3'), can be changed as desired.
            if (fabs(dx) <= 0.05 && fabs(dy) <= 0.05)
            {
                HorizontalCoordsNP.setState(IPS_OK);
                currentAZ            = targetAZ;
                currentALT           = targetALT;
                HorizontalCoordsNP.apply("Slew is complete.");
            }
            else
                HorizontalCoordsNP.apply();
            break;

        case IPS_OK:
            break;

        case IPS_ALERT:
            break;
    }

    return true;
}

void LX200_16::getBasicData()
{
    LX200GPS::getBasicData();

    if (!isSimulation())
    {
        getLX200Az(PortFD, &currentAZ);
        getLX200Alt(PortFD, &currentALT);
        HorizontalCoordsNP[0].setValue(currentALT);
        HorizontalCoordsNP[1].setValue(currentAZ);
        HorizontalCoordsNP.apply();
    }
}
