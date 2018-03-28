/*
    Avalon Star GO Focuser
    Copyright (C) 2018 Christopher Contaxis (chrconta@gmail.com) and
    Wolfgang Reissenberger (sterne-jaeger@t-online.de)

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

#include "lx200stargofocuser.h"

/**
 * @brief Constructor
 * @param defaultDevice the telescope
 * @param name device name
 */
LX200StarGoFocuser::LX200StarGoFocuser(INDI::DefaultDevice* defaultDevice, const char *name) : INDI::Focuser ()
{
    baseDevice = defaultDevice;
    deviceName = name;
    setDeviceName(name);
    // we reuse the telescope connection
    setConnection(CONNECTION_NONE);
}

/**
 * @brief Initialize the focuser UI controls
 * @param groupName tab where the UI controls are grouped
 */
void LX200StarGoFocuser::initProperties(const char *groupName)
{
    INDI::FocuserInterface::initProperties(groupName);

    IUFillNumber(&INDI::FocuserInterface::FocusSpeedN[0], "FOCUS_SPEED_VALUE", "Focus Speed", "%0.0f", 0, 10, 1, 2);
    IUFillNumberVector(&FocusSpeedNP, INDI::FocuserInterface::FocusSpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Speed", FOCUS_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&FocusMotionS[0], "FOCUS_INWARD", "Focus In", ISS_ON);
    IUFillSwitch(&FocusMotionS[1], "FOCUS_OUTWARD", "Focus Out", ISS_OFF);
    IUFillSwitchVector(&FocusMotionSP, FocusMotionS, 2, getDeviceName(), "FOCUS_MOTION", "Direction", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);

    IUFillNumber(&FocusTimerN[0], "FOCUS_TIMER_VALUE", "Focus Timer (ms)", "%4.0f", 0.0, 5000.0, 50.0, 1000.0);
    IUFillNumberVector(&FocusTimerNP, FocusTimerN, 1, getDeviceName(), "FOCUS_TIMER", "Timer", FOCUS_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&FocusAbsPosN[0], "FOCUS_ABSOLUTE_POSITION", "Ticks", "%4.0f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusAbsPosNP, FocusAbsPosN, 1, getDeviceName(), "ABS_FOCUS_POSITION", "Absolute Position", FOCUS_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&FocusRelPosN[0], "FOCUS_RELATIVE_POSITION", "Ticks", "%4.0f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusRelPosNP, FocusRelPosN, 1, getDeviceName(), "REL_FOCUS_POSITION", "Relative Position", FOCUS_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&INDI::FocuserInterface::AbortS[0], "FOCUS_ABORT", "Focus Abort", ISS_OFF);
    IUFillSwitchVector(&AbortSP, INDI::FocuserInterface::AbortS, 1, getDeviceName(), "FOCUS_ABORT_MOTION", "Abort Motion", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&FocusSyncPosN[0], "FOCUS_SYNC_POSITION_VALUE", "Ticks", "%4.0f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusSyncPosNP, FocusSyncPosN, 1, getDeviceName(), "FOCUS_SYNC_POSITION", "Sync", FOCUS_TAB, IP_WO, 0, IPS_OK);

}

/**
 * @brief Fill the UI controls with current values
 * @return true iff everything went fine
 */

bool LX200StarGoFocuser::updateProperties()
{
    if (! INDI::FocuserInterface::updateProperties())
    {
        return false;
    } else {
        if (baseDevice->isConnected()) {
            defineNumber(&FocusSpeedNP);
            defineSwitch(&FocusMotionSP);
            defineNumber(&FocusTimerNP);
            defineNumber(&FocusAbsPosNP);
            defineNumber(&FocusRelPosNP);
            defineSwitch(&AbortSP);
            defineNumber(&FocusSyncPosNP);
        }
        else {
            deleteProperty(FocusSpeedNP.name);
            deleteProperty(FocusMotionSP.name);
            deleteProperty(FocusTimerNP.name);
            deleteProperty(FocusAbsPosNP.name);
            deleteProperty(FocusRelPosNP.name);
            deleteProperty(AbortSP.name);
            deleteProperty(FocusSyncPosNP.name);
        }
        return true;

    }
}

const char *LX200StarGoFocuser::getDefaultName()
{
    return deviceName;
}

