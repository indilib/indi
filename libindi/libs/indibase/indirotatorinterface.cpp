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

    // Rotator GOTO
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 0., 0., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, defaultDevice->getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", groupName, IP_RW, 0, IPS_IDLE );

    // Rotator Position Angle
    IUFillNumber(&RotatorPositionAngleN[0], "ANGLE", "Degrees", "%.2f", -180, 180., 10., 0.);
    IUFillNumberVector(&RotatorPositionAngleNP, RotatorPositionAngleN, 1, defaultDevice->getDeviceName(), "ABS_ROTATOR_ANGLE", "Position Angle", groupName, IP_RW, 0, IPS_IDLE );

    // Rotator Angle Settings
    IUFillNumber(&RotatorAngleSettingN[0], "MUL", "x MUL", "%.2f", 0.01, 10., 1., 1.);
    IUFillNumber(&RotatorAngleSettingN[1], "ADD", "+ OFFSET", "%.2f", -180., 180., 10., 0.);
    IUFillNumberVector(&RotatorAngleSettingNP, RotatorAngleSettingN, 2, defaultDevice->getDeviceName(), "ABS_ROTATOR_ANGLE", "Position Angle", groupName, IP_RW, 0, IPS_IDLE );

    // Abort Rotator
    IUFillSwitch(&AbortRotatorS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortRotatorSP, AbortRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Sync
    IUFillNumber(&SyncRotatorN[0], "ROTATOR_SYNC_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&SyncRotatorNP, SyncRotatorN, 1, defaultDevice->getDeviceName(), "SYNC_ROTATOR", "Sync", groupName, IP_RW, 0, IPS_IDLE );

    // Home Rotator
    IUFillSwitch(&HomeRotatorS[0], "HOME", "Abort", ISS_OFF);
    IUFillSwitchVector(&HomeRotatorSP, HomeRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_HOME", "HOME", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);




}

bool INDI::RotatorInterface::processRotatorNumber(const char *dev, const char *name, double values[], char *names[], int n)
{

    return false;
}

bool INDI::RotatorInterface::processRotatorSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    return false;
}

bool INDI::RotatorInterface::updateProperties(INDI::DefaultDevice *defaultDevice)
{
    if (defaultDevice->isConnected())
    {
        defaultDevice->defineNumber(&RotatorAbsPosNP);
        defaultDevice->defineNumber(&RotatorPositionAngleNP);
        defaultDevice->defineNumber(&RotatorAngleSettingNP);

        if (CanAbort())
            defaultDevice->defineSwitch(&AbortRotatorSP);
        if (CanSync())
            defaultDevice->defineNumber(&SyncRotatorNP);
        if (CanHome())
            defaultDevice->defineSwitch(&HomeRotatorSP);
    }
    else
    {
        defaultDevice->deleteProperty(RotatorAbsPosNP.name);
        defaultDevice->deleteProperty(RotatorPositionAngleNP.name);
        defaultDevice->deleteProperty(RotatorAngleSettingNP.name);

        if (CanAbort())
            defaultDevice->deleteProperty(AbortRotatorSP.name);
        if (CanSync())
            defaultDevice->deleteProperty(SyncRotatorNP.name);
        if (CanHome())
            defaultDevice->deleteProperty(HomeRotatorSP.name);
    }

    return true;
}

bool INDI::RotatorInterface::saveRotatorConfig(FILE *fp)
{
    IUSaveConfigNumber(fp, &RotatorAngleSettingNP);
    return true;
}

bool INDI::RotatorInterface::SyncRotator(uint32_t ticks)
{
    INDI_UNUSED(ticks);
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
