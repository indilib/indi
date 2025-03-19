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
    IUFillSwitchVector(&FullMotionSP, FullMotionS, 2, getDeviceName(), "FULL_MOTION", "Full Motion", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    FocusAbsPosNP[0].setMin(SyncNP[0].setMin(0));
    FocusAbsPosNP[0].setMax(SyncNP[0].getMax());
    FocusAbsPosNP[0].setStep(SyncN[0].step);
    FocusAbsPosNP[0].setValue(0);

    FocusRelPosNP[0].setMax((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 2);
    FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 100.0);
    FocusRelPosNP[0].setValue(100);

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

    if (FocusTimerNP.getState() == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (remaining <= 0)
        {
            FocusTimerNP.s       = IPS_OK;
            FocusTimerNP[0].setValue(0);
            AbortFocuser();
        }
        else
            FocusTimerNP[0].setValue(remaining * 1000.0);

        FocusTimerNP.apply();
    }

#if 0
    bool rc = getPosition(&currentTicks);

    if (rc)
        FocusAbsPosNP[0].setValue(currentTicks);

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (targetPosition == FocusAbsPosNP[0].getValue())
        {
            if (FocusRelPosNP.getState() == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();
            }

            FocusAbsPosNP.setState(IPS_OK);
            LOG_DEBUG("Focuser reached target position.");
        }
    }

    FocusAbsPosNP.apply();
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

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState FocusMaster::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t finalTicks = FocusAbsPosNP[0].getValue() + (ticks * (dir == FOCUS_INWARD ? -1 : 1));

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
            FocusMotionSP.reset();
            FocusMotionSP.setState(IPS_IDLE);
            FocusMotionSP.apply();
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
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}
