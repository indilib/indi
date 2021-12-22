/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Televue FocusMaster Driver.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "focusmaster.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <unistd.h>

#define POLLMS_OVERRIDE     1000 /* 1000 ms */
#define FOCUSMASTER_TIMEOUT 1000 /* 1000 ms */
#define MAX_FM_BUF          16

#define FOCUS_SETTINGS_TAB "Settings"

// We declare an auto pointer to FocusMaster.
std::unique_ptr<FocusMaster> focusMaster(new FocusMaster());

FocusMaster::FocusMaster()
{
    FI::SetCapability(FOCUSER_CAN_ABORT);
    setConnection(CONNECTION_NONE);
}

bool FocusMaster::Connect()
{    
    handle = hid_open(0x134A, 0x9030, nullptr);

    if (handle == nullptr)
    {
        LOG_ERROR("No FocusMaster focuser found.");
        return false;
    }
    else
    {
        // N.B. Check here if we have the digital readout gadget.

        // if digital readout
        //FI::SetCapability(GetFocuserCapability() | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABS_MOVE);

        SetTimer(POLLMS_OVERRIDE);

    }

    return (handle != nullptr);
}

bool FocusMaster::Disconnect()
{
    hid_close(handle);
    hid_exit();
    return true;
}

const char *FocusMaster::getDefaultName()
{
    return (const char *)"FocusMaster";
}

bool FocusMaster::initProperties()
{
    INDI::Focuser::initProperties();

    // Sync to a particular position
    IUFillNumber(&SyncN[0], "Ticks", "", "%.f", 0, 100000, 100., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "Sync", "", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Full Forward/Reverse Motion
    IUFillSwitch(&FullMotionS[FOCUS_INWARD], "FULL_INWARD", "Full Inward", ISS_OFF);
    IUFillSwitch(&FullMotionS[FOCUS_OUTWARD], "FULL_OUTWARD", "Full Outward", ISS_OFF);
    IUFillSwitchVector(&FullMotionSP, FullMotionS, 2, getDeviceName(), "FULL_MOTION", "Full Motion", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    FocusAbsPosN[0].min = SyncN[0].min = 0;
    FocusAbsPosN[0].max = SyncN[0].max;
    FocusAbsPosN[0].step = SyncN[0].step;
    FocusAbsPosN[0].value = 0;

    FocusRelPosN[0].max   = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 2;
    FocusRelPosN[0].step  = FocusRelPosN[0].max / 100.0;
    FocusRelPosN[0].value = 100;

    addSimulationControl();

    return true;
}

bool FocusMaster::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&FullMotionSP);
        //defineProperty(&SyncNP);
    }
    else
    {
        deleteProperty(FullMotionSP.name);
        //deleteProperty(SyncNP.name);
    }

    return true;
}

void FocusMaster::TimerHit()
{
    if (!isConnected())
        return;

    //uint32_t currentTicks = 0;

    if (FocusTimerNP.s == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (remaining <= 0)
        {
            FocusTimerNP.s       = IPS_OK;
            FocusTimerN[0].value = 0;
            AbortFocuser();
        }
        else
            FocusTimerN[0].value = remaining * 1000.0;

        IDSetNumber(&FocusTimerNP, nullptr);
    }

#if 0
    bool rc = getPosition(&currentTicks);

    if (rc)
        FocusAbsPosN[0].value = currentTicks;    

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (targetPosition == FocusAbsPosN[0].value)
        {
            if (FocusRelPosNP.s == IPS_BUSY)
            {
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, nullptr);
            }

            FocusAbsPosNP.s = IPS_OK;
            LOG_DEBUG("Focuser reached target position.");
        }
    }

    IDSetNumber(&FocusAbsPosNP, nullptr);
#endif

    SetTimer(POLLMS_OVERRIDE);
}

bool FocusMaster::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Full Motion
        if (!strcmp(FullMotionSP.name, name))
        {
            IUUpdateSwitch(&FullMotionSP, states, names, n);
            FocusDirection targetDirection = static_cast<FocusDirection>(IUFindOnSwitchIndex(&FullMotionSP));

            FullMotionSP.s = MoveFocuser(targetDirection, 0, 0);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

}
bool FocusMaster::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Sync
        if (strcmp(SyncNP.name, name) == 0)
        {
            IUUpdateNumber(&SyncNP, values, names, n);
            if (!sync(SyncN[0].value))
                SyncNP.s = IPS_ALERT;
            else
                SyncNP.s = IPS_OK;

            IDSetNumber(&SyncNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState FocusMaster::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);

    uint8_t command[MAX_FM_BUF] = {0};

    if (dir == FOCUS_INWARD)
    {
        command[0] = 0x31;
        command[1] = 0x21;
    }
    else
    {
        command[0] = 0x32;
        command[1] = 0x22;
    }

    sendCommand(command);

    gettimeofday(&focusMoveStart, nullptr);
    focusMoveRequest = duration / 1000.0;

    if (duration > 0 && duration <= POLLMS_OVERRIDE)
    {
        usleep(duration * 1000);
        AbortFocuser();
        return IPS_OK;
    }

    return IPS_BUSY;
}

IPState FocusMaster::MoveAbsFocuser(uint32_t targetTicks)
{
    INDI_UNUSED(targetTicks);

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState FocusMaster::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t finalTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));

    return MoveAbsFocuser(finalTicks);
}

bool FocusMaster::sendCommand(const uint8_t *command, char *response)
{
    INDI_UNUSED(response);

    int rc = hid_write(handle, command, 2);

    LOGF_DEBUG("CMD <%#02X %#02X>", command[0], command[1]);

    if (rc < 0)
    {
        LOGF_ERROR("<%#02X %#02X>: Error writing to device %s", command[0], command[1], hid_error(handle));
        return false;
    }

    return true;
}

bool FocusMaster::setPosition(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    return false;
}

bool FocusMaster::getPosition(uint32_t *ticks)
{
    INDI_UNUSED(ticks);
    return false;
}

bool FocusMaster::AbortFocuser()
{
    uint8_t command[MAX_FM_BUF] = {0};

    command[0] = 0x30;
    command[1] = 0x30;

    LOG_DEBUG("Aborting Focuser...");

    bool rc = sendCommand(command);

    if (rc)
    {
        if (FullMotionSP.s == IPS_BUSY)
        {
            IUResetSwitch(&FullMotionSP);
            FullMotionSP.s = IPS_IDLE;
            IDSetSwitch(&FullMotionSP, nullptr);
        }

        if (FocusMotionSP.s == IPS_BUSY)
        {
            IUResetSwitch(&FocusMotionSP);
            FocusMotionSP.s = IPS_IDLE;
            IDSetSwitch(&FocusMotionSP, nullptr);
        }
    }

    return rc;
}

bool FocusMaster::sync(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    return false;
}

float FocusMaster::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}
