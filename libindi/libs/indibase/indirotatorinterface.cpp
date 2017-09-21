/*
    Rotator Interface
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indirotatorinterface.h"
#include "defaultdevice.h"

#include "indilogger.h"

#include <cstring>

INDI::RotatorInterface::RotatorInterface()
{
}

void INDI::RotatorInterface::initProperties(INDI::DefaultDevice *defaultDevice, const char *groupName)
{
    strncpy(rotatorName, defaultDevice->getDeviceName(), MAXINDIDEVICE);

    // Rotator Angle
    IUFillNumber(&GotoRotatorN[0], "ANGLE", "Angle", "%.2f", 0, 360., 10., 0.);
    IUFillNumberVector(&GotoRotatorNP, GotoRotatorN, 1, defaultDevice->getDeviceName(), "ABS_ROTATOR_ANGLE", "Goto", groupName, IP_RW, 0, IPS_IDLE );

    // Abort Rotator
    IUFillSwitch(&AbortRotatorS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortRotatorSP, AbortRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Sync
    IUFillNumber(&SyncRotatorN[0], "ANGLE", "Angle", "%.2f", 0, 360., 10., 0.);
    IUFillNumberVector(&SyncRotatorNP, SyncRotatorN, 1, defaultDevice->getDeviceName(), "SYNC_ROTATOR_ANGLE", "Sync", groupName, IP_RW, 0, IPS_IDLE );

    // Home Rotator
    IUFillSwitch(&HomeRotatorS[0], "HOME", "Start", ISS_OFF);
    IUFillSwitchVector(&HomeRotatorSP, HomeRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_HOME", "Homing", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Reverse Direction
    IUFillSwitch(&ReverseRotatorS[REVERSE_ENABLED], "REVERSE_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&ReverseRotatorS[REVERSE_DISABLED], "REVERSE_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&ReverseRotatorSP, ReverseRotatorS, 2, defaultDevice->getDeviceName(), "ROTATOR_REVERSE", "Reverse", groupName, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);
}

bool INDI::RotatorInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, rotatorName) == 0)
    {
        ////////////////////////////////////////////
        // Sync
        ////////////////////////////////////////////
        if (strcmp(name, SyncRotatorNP.name) == 0)
        {
            bool rc = SyncRotator(values[0]);
            SyncRotatorNP.s = rc ? IPS_OK : IPS_ALERT;
            if (rc)
                SyncRotatorN[0].value = values[0];

            IDSetNumber(&SyncRotatorNP, nullptr);
            return true;
        }        
        ////////////////////////////////////////////
        // Move Absolute Angle
        ////////////////////////////////////////////
        else if (strcmp(name, GotoRotatorNP.name) == 0)
        {
            GotoRotatorNP.s = MoveRotator(values[0]);
            IDSetNumber(&GotoRotatorNP, nullptr);
            if (GotoRotatorNP.s == IPS_BUSY)
                DEBUGFDEVICE(rotatorName, INDI::Logger::DBG_SESSION, "Rotator moving to %.2f degrees...", values[0]);
            return true;
        }
    }

    return false;
}

bool INDI::RotatorInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, rotatorName) == 0)
    {
        ////////////////////////////////////////////
        // Abort
        ////////////////////////////////////////////
        if (strcmp(name, AbortRotatorSP.name) == 0)
        {
            AbortRotatorSP.s = AbortRotator() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&AbortRotatorSP, nullptr);
            if (AbortRotatorSP.s == IPS_OK)
            {
                if (GotoRotatorNP.s != IPS_OK)
                {
                    GotoRotatorNP.s = IPS_OK;
                    IDSetNumber(&GotoRotatorNP, nullptr);
                }
            }
            return true;
        }
        ////////////////////////////////////////////
        // Home
        ////////////////////////////////////////////
        else if (strcmp(name, HomeRotatorSP.name) == 0)
        {
            HomeRotatorSP.s = HomeRotator();
            IUResetSwitch(&HomeRotatorSP);
            if (HomeRotatorSP.s == IPS_BUSY)
                HomeRotatorS[0].s = ISS_ON;
            IDSetSwitch(&HomeRotatorSP, nullptr);
            return true;
        }
        ////////////////////////////////////////////
        // Reverse Rotator
        ////////////////////////////////////////////
        else if (strcmp(name, ReverseRotatorSP.name) == 0)
        {
            bool rc = false;
            bool enabled = (!strcmp(IUFindOnSwitchName(states, names, n), "ENABLED"));
            rc = ReverseRotator(enabled);

            if (rc)
            {
                IUUpdateSwitch(&ReverseRotatorSP, states, names, n);
                ReverseRotatorSP.s = IPS_OK;
                DEBUGFDEVICE(rotatorName, INDI::Logger::DBG_SESSION, "Rotator direction is %s.", (enabled ? "reversed" : "normal"));
            }
            else
            {
                ReverseRotatorSP.s = IPS_ALERT;
                DEBUGDEVICE(rotatorName, INDI::Logger::DBG_SESSION, "Rotator reverse direction failed.");
            }

            IDSetSwitch(&ReverseRotatorSP, nullptr);
            return true;
        }
    }

    return false;
}

bool INDI::RotatorInterface::updateProperties(INDI::DefaultDevice *defaultDevice)
{
    if (defaultDevice->isConnected())
    {
        defaultDevice->defineNumber(&GotoRotatorNP);

        if (CanAbort())
            defaultDevice->defineSwitch(&AbortRotatorSP);
        if (CanSync())
            defaultDevice->defineNumber(&SyncRotatorNP);
        if (CanHome())
            defaultDevice->defineSwitch(&HomeRotatorSP);
    }
    else
    {        
        defaultDevice->deleteProperty(GotoRotatorNP.name);

        if (CanAbort())
            defaultDevice->deleteProperty(AbortRotatorSP.name);
        if (CanSync())
            defaultDevice->deleteProperty(SyncRotatorNP.name);
        if (CanHome())
            defaultDevice->deleteProperty(HomeRotatorSP.name);
    }

    return true;
}

bool INDI::RotatorInterface::SyncRotator(double angle)
{
    INDI_UNUSED(angle);
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support syncing.");
    return false;
}

IPState INDI::RotatorInterface::HomeRotator()
{
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support homing.");
    return IPS_ALERT;
}

bool INDI::RotatorInterface::AbortRotator()
{
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support abort.");
    return false;
}

bool INDI::RotatorInterface::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support reverse.");
    return false;
}
