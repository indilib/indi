/*
    Focuser Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indifocuserinterface.h"

#include "indilogger.h"

#include <cstring>
#include <cmath>

namespace INDI
{

FocuserInterface::FocuserInterface(DefaultDevice * defaultDevice) : m_defaultDevice(defaultDevice)
{
}

void FocuserInterface::initProperties(const char * groupName)
{
    IUFillNumber(&FocusSpeedN[0], "FOCUS_SPEED_VALUE", "Focus Speed", "%3.0f", 0.0, 255.0, 1.0, 255.0);
    IUFillNumberVector(&FocusSpeedNP, FocusSpeedN, 1, m_defaultDevice->getDeviceName(), "FOCUS_SPEED", "Speed", groupName, IP_RW, 60, IPS_OK);

    IUFillNumber(&FocusTimerN[0], "FOCUS_TIMER_VALUE", "Focus Timer (ms)", "%4.0f", 0.0, 5000.0, 50.0, 1000.0);
    IUFillNumberVector(&FocusTimerNP, FocusTimerN, 1, m_defaultDevice->getDeviceName(), "FOCUS_TIMER", "Timer", groupName, IP_RW, 60, IPS_OK);
    lastTimerValue = 1000.0;

    IUFillSwitch(&FocusMotionS[0], "FOCUS_INWARD", "Focus In", ISS_ON);
    IUFillSwitch(&FocusMotionS[1], "FOCUS_OUTWARD", "Focus Out", ISS_OFF);
    IUFillSwitchVector(&FocusMotionSP, FocusMotionS, 2, m_defaultDevice->getDeviceName(), "FOCUS_MOTION", "Direction", groupName, IP_RW,
                       ISR_1OFMANY, 60, IPS_OK);

    // Absolute Position
    IUFillNumber(&FocusAbsPosN[0], "FOCUS_ABSOLUTE_POSITION", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusAbsPosNP, FocusAbsPosN, 1, m_defaultDevice->getDeviceName(), "ABS_FOCUS_POSITION", "Absolute Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Relative Position
    IUFillNumber(&FocusRelPosN[0], "FOCUS_RELATIVE_POSITION", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusRelPosNP, FocusRelPosN, 1, m_defaultDevice->getDeviceName(), "REL_FOCUS_POSITION", "Relative Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Sync
    IUFillNumber(&FocusSyncN[0], "FOCUS_SYNC_VALUE", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusSyncNP, FocusSyncN, 1, m_defaultDevice->getDeviceName(), "FOCUS_SYNC", "Sync",
                       groupName, IP_RW, 60, IPS_OK);

    // Maximum Position
    IUFillNumber(&FocusMaxPosN[0], "FOCUS_MAX_VALUE", "Steps", "%.f", 1e3, 1e6, 1e4, 1e5);
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, m_defaultDevice->getDeviceName(), "FOCUS_MAX", "Max. Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Abort
    IUFillSwitch(&FocusAbortS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&FocusAbortSP, FocusAbortS, 1, m_defaultDevice->getDeviceName(), "FOCUS_ABORT_MOTION", "Abort Motion", groupName, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    // Revese
    IUFillSwitch(&FocusReverseS[REVERSED_ENABLED], "ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&FocusReverseS[REVERSED_DISABLED], "DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&FocusReverseSP, FocusReverseS, 2, m_defaultDevice->getDeviceName(), "FOCUS_REVERSE_MOTION", "Reverse Motion", groupName, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation
    IUFillSwitch(&FocusBacklashS[REVERSED_ENABLED], "ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&FocusBacklashS[REVERSED_DISABLED], "DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&FocusBacklashSP, FocusBacklashS, 2, m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_TOGGLE", "Backlash", groupName, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation Value
    IUFillNumber(&FocusBacklashN[0], "FOCUS_BACKLASH_VALUE", "Steps", "%.f", 0, 1e6, 100, 0);
    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_STEPS", "Backlash",
                       groupName, IP_RW, 60, IPS_OK);
}

bool FocuserInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        //  Now we add our focusser specific stuff
        m_defaultDevice->defineSwitch(&FocusMotionSP);

        if (HasVariableSpeed())
        {
            m_defaultDevice->defineNumber(&FocusSpeedNP);

            // We only define Focus Timer if we can not absolute move
            if (CanAbsMove() == false)
                m_defaultDevice->defineNumber(&FocusTimerNP);
        }
        if (CanRelMove())
            m_defaultDevice->defineNumber(&FocusRelPosNP);
        if (CanAbsMove())
        {
            m_defaultDevice->defineNumber(&FocusAbsPosNP);
            m_defaultDevice->defineNumber(&FocusMaxPosNP);
        }
        if (CanAbort())
            m_defaultDevice->defineSwitch(&FocusAbortSP);
        if (CanSync())
            m_defaultDevice->defineNumber(&FocusSyncNP);
        if (CanReverse())
            m_defaultDevice->defineSwitch(&FocusReverseSP);
        if (HasBacklash())
        {
            m_defaultDevice->defineSwitch(&FocusBacklashSP);
            m_defaultDevice->defineNumber(&FocusBacklashNP);
        }
    }
    else
    {
        m_defaultDevice->deleteProperty(FocusMotionSP.name);
        if (HasVariableSpeed())
        {
            m_defaultDevice->deleteProperty(FocusSpeedNP.name);

            if (CanAbsMove() == false)
                m_defaultDevice->deleteProperty(FocusTimerNP.name);
        }
        if (CanRelMove())
            m_defaultDevice->deleteProperty(FocusRelPosNP.name);
        if (CanAbsMove())
        {
            m_defaultDevice->deleteProperty(FocusAbsPosNP.name);
            m_defaultDevice->deleteProperty(FocusMaxPosNP.name);
        }
        if (CanAbort())
            m_defaultDevice->deleteProperty(FocusAbortSP.name);
        if (CanSync())
            m_defaultDevice->deleteProperty(FocusSyncNP.name);
        if (CanReverse())
            m_defaultDevice->deleteProperty(FocusReverseSP.name);
        if (HasBacklash())
        {
            m_defaultDevice->deleteProperty(FocusBacklashSP.name);
            m_defaultDevice->deleteProperty(FocusBacklashNP.name);
        }
    }

    return true;
}

bool FocuserInterface::processNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    // Move focuser based on requested timeout
    if (!strcmp(name, FocusTimerNP.name))
    {
        FocusDirection dir;
        int speed;
        int t;

        //  first we get all the numbers just sent to us
        IUUpdateNumber(&FocusTimerNP, values, names, n);

        //  Now lets find what we need for this move
        speed = FocusSpeedN[0].value;

        if (FocusMotionS[0].s == ISS_ON)
            dir = FOCUS_INWARD;
        else
            dir = FOCUS_OUTWARD;

        t              = FocusTimerN[0].value;
        lastTimerValue = t;

        FocusTimerNP.s = MoveFocuser(dir, speed, t);
        IDSetNumber(&FocusTimerNP, nullptr);
        return true;
    }

    // Set variable focus speed
    if (!strcmp(name, FocusSpeedNP.name))
    {
        FocusSpeedNP.s    = IPS_OK;
        int current_speed = FocusSpeedN[0].value;
        IUUpdateNumber(&FocusSpeedNP, values, names, n);

        if (SetFocuserSpeed(FocusSpeedN[0].value) == false)
        {
            FocusSpeedN[0].value = current_speed;
            FocusSpeedNP.s       = IPS_ALERT;
        }

        //  Update client display
        IDSetNumber(&FocusSpeedNP, nullptr);
        return true;
    }

    // Update Maximum Position allowed
    if (!strcmp(name, FocusMaxPosNP.name))
    {
        uint32_t maxTravel = rint(values[0]);
        if (SetFocuserMaxPosition(maxTravel))
        {
            IUUpdateNumber(&FocusMaxPosNP, values, names, n);

            FocusAbsPosN[0].max = FocusSyncN[0].max = FocusMaxPosN[0].value;
            FocusAbsPosN[0].step = FocusSyncN[0].step = FocusMaxPosN[0].value / 50.0;
            FocusAbsPosN[0].min = FocusSyncN[0].min = 0;

            FocusRelPosN[0].max  = FocusMaxPosN[0].value / 2;
            FocusRelPosN[0].step = FocusMaxPosN[0].value / 100.0;
            FocusRelPosN[0].min  = 0;

            IUUpdateMinMax(&FocusAbsPosNP);
            IUUpdateMinMax(&FocusRelPosNP);
            IUUpdateMinMax(&FocusSyncNP);

            FocusMaxPosNP.s = IPS_OK;
        }
        else
            FocusMaxPosNP.s = IPS_ALERT;

        IDSetNumber(&FocusMaxPosNP, nullptr);
        return true;
    }

    // Sync
    if (!strcmp(name, FocusSyncNP.name))
    {
        if (SyncFocuser(rint(values[0])))
        {
            FocusSyncN[0].value = FocusAbsPosN[0].value = rint(values[0]);
            FocusSyncNP.s = IPS_OK;
            IDSetNumber(&FocusSyncNP, nullptr);
            IDSetNumber(&FocusAbsPosNP, nullptr);
        }
        else
        {
            FocusSyncNP.s = IPS_ALERT;
            IDSetNumber(&FocusSyncNP, nullptr);
        }
        return true;
    }

    // Set backlash value
    if (!strcmp(name, FocusBacklashNP.name))
    {
        if (FocusBacklashS[BACKLASH_ENABLED].s != ISS_ON)
        {
            FocusBacklashNP.s = IPS_IDLE;
            DEBUGDEVICE(dev, Logger::DBG_WARNING, "Focuser backlash must be enabled first.");
        }
        else
        {
            uint32_t steps = static_cast<uint32_t>(values[0]);
            if (SetFocuserBacklash(steps))
            {
                FocusBacklashN[0].value = values[0];
                FocusBacklashNP.s = IPS_OK;
            }
            else
                FocusBacklashNP.s = IPS_ALERT;
        }
        IDSetNumber(&FocusBacklashNP, nullptr);
        return true;
    }

    // Update Absolute Focuser Position
    if (!strcmp(name, FocusAbsPosNP.name))
    {
        int newPos = rint(values[0]);

        if (newPos < FocusAbsPosN[0].min)
        {
            FocusAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            DEBUGFDEVICE(dev, Logger::DBG_ERROR, "Requested position out of bound. Focus minimum position is %g",
                         FocusAbsPosN[0].min);
            return false;
        }
        else if (newPos > FocusAbsPosN[0].max)
        {
            FocusAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            DEBUGFDEVICE(dev, Logger::DBG_ERROR, "Requested position out of bound. Focus maximum position is %g",
                         FocusAbsPosN[0].max);
            return false;
        }

        IPState ret;

        if ((ret = MoveAbsFocuser(newPos)) == IPS_OK)
        {
            FocusAbsPosNP.s = IPS_OK;
            IUUpdateNumber(&FocusAbsPosNP, values, names, n);
            DEBUGFDEVICE(dev, Logger::DBG_SESSION, "Focuser moved to position %d", newPos);
            IDSetNumber(&FocusAbsPosNP, nullptr);
            return true;
        }
        else if (ret == IPS_BUSY)
        {
            FocusAbsPosNP.s = IPS_BUSY;
            DEBUGFDEVICE(dev, Logger::DBG_SESSION, "Focuser is moving to position %d", newPos);
            IDSetNumber(&FocusAbsPosNP, nullptr);
            return true;
        }

        FocusAbsPosNP.s = IPS_ALERT;
        DEBUGDEVICE(dev, Logger::DBG_ERROR, "Focuser failed to move to new requested position.");
        IDSetNumber(&FocusAbsPosNP, nullptr);
        return false;
    }

    // Update Relative focuser steps. This moves the focuser CW/CCW by this number of steps.
    if (!strcmp(name, FocusRelPosNP.name))
    {
        int newPos = rint(values[0]);

        if (newPos <= 0)
        {
            DEBUGDEVICE(dev, Logger::DBG_ERROR, "Relative ticks value must be greater than zero.");
            FocusRelPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusRelPosNP, nullptr);
            return false;
        }

        IPState ret;

        if (CanAbsMove())
        {
            if (FocusMotionS[0].s == ISS_ON)
            {
                if (FocusAbsPosN[0].value - newPos < FocusAbsPosN[0].min)
                {
                    FocusRelPosNP.s = IPS_ALERT;
                    IDSetNumber(&FocusRelPosNP, nullptr);
                    DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                                 "Requested position out of bound. Focus minimum position is %g", FocusAbsPosN[0].min);
                    return false;
                }
            }
            else
            {
                if (FocusAbsPosN[0].value + newPos > FocusAbsPosN[0].max)
                {
                    FocusRelPosNP.s = IPS_ALERT;
                    IDSetNumber(&FocusRelPosNP, nullptr);
                    DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                                 "Requested position out of bound. Focus maximum position is %g", FocusAbsPosN[0].max);
                    return false;
                }
            }
        }

        if ((ret = MoveRelFocuser((FocusMotionS[0].s == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD), newPos)) == IPS_OK)
        {
            FocusRelPosNP.s = FocusAbsPosNP.s = IPS_OK;
            IUUpdateNumber(&FocusRelPosNP, values, names, n);
            IDSetNumber(&FocusRelPosNP, "Focuser moved %d steps %s", newPos,
                        FocusMotionS[0].s == ISS_ON ? "inward" : "outward");
            IDSetNumber(&FocusAbsPosNP, nullptr);
            return true;
        }
        else if (ret == IPS_BUSY)
        {
            IUUpdateNumber(&FocusRelPosNP, values, names, n);
            FocusRelPosNP.s = FocusAbsPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusAbsPosNP, "Focuser is moving %d steps %s...", newPos,
                        FocusMotionS[0].s == ISS_ON ? "inward" : "outward");
            IDSetNumber(&FocusAbsPosNP, nullptr);
            return true;
        }

        FocusRelPosNP.s = IPS_ALERT;
        DEBUGDEVICE(dev, Logger::DBG_ERROR, "Focuser failed to move to new requested position.");
        IDSetNumber(&FocusRelPosNP, nullptr);
        return false;
    }

    return false;
}

bool FocuserInterface::processSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    INDI_UNUSED(dev);
    //  This one is for focus motion
    if (!strcmp(name, FocusMotionSP.name))
    {
        // Record last direction and state.
        FocusDirection prevDirection = FocusMotionS[FOCUS_INWARD].s == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD;
        IPState prevState = FocusMotionSP.s;

        IUUpdateSwitch(&FocusMotionSP, states, names, n);

        FocusDirection targetDirection = FocusMotionS[FOCUS_INWARD].s == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD;

        if (CanRelMove() || CanAbsMove() || HasVariableSpeed())
        {
            FocusMotionSP.s = IPS_OK;
        }
        // If we are dealing with a simple dumb DC focuser, we move in a specific direction in an open-loop fashion until stopped.
        else
        {
            // If we are reversing direction let's issue abort first.
            if (prevDirection != targetDirection && prevState == IPS_BUSY)
                AbortFocuser();

            FocusMotionSP.s = MoveFocuser(targetDirection, 0, 0);
        }

        IDSetSwitch(&FocusMotionSP, nullptr);

        return true;
    }

    // Backlash compensation
    if (!strcmp(name, FocusBacklashSP.name))
    {
        bool enable = !strcmp(FocusBacklashS[BACKLASH_ENABLED].name, IUFindOnSwitchName(states, names, n));

        if (SetFocuserBacklashEnabled(enable))
        {
            IUUpdateSwitch(&FocusBacklashSP, states, names, n);
            FocusBacklashSP.s = IPS_OK;
        }
        else
            FocusBacklashSP.s = IPS_ALERT;

        IDSetSwitch(&FocusBacklashSP, nullptr);
        return true;
    }

    // Abort Focuser
    if (!strcmp(name, FocusAbortSP.name))
    {
        IUResetSwitch(&FocusAbortSP);

        if (AbortFocuser())
        {
            FocusAbortSP.s = IPS_OK;
            if (CanAbsMove() && FocusAbsPosNP.s != IPS_IDLE)
            {
                FocusAbsPosNP.s = IPS_IDLE;
                IDSetNumber(&FocusAbsPosNP, nullptr);
            }
            if (CanRelMove() && FocusRelPosNP.s != IPS_IDLE)
            {
                FocusRelPosNP.s = IPS_IDLE;
                IDSetNumber(&FocusRelPosNP, nullptr);
            }
        }
        else
            FocusAbortSP.s = IPS_ALERT;

        IDSetSwitch(&FocusAbortSP, nullptr);
        return true;
    }

    // Reverse Motion
    if (!strcmp(name, FocusReverseSP.name))
    {
        bool enabled = !strcmp("ENABLED", IUFindOnSwitchName(states, names, n));

        if (ReverseFocuser(enabled))
        {
            IUUpdateSwitch(&FocusReverseSP, states, names, n);
            FocusReverseSP.s = IPS_OK;
        }
        else
        {
            FocusReverseSP.s = IPS_ALERT;
        }

        IDSetSwitch(&FocusReverseSP, nullptr);
        return true;
    }

    return false;
}

IPState FocuserInterface::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(speed);
    INDI_UNUSED(duration);
    // Must be implemented by child class
    return IPS_ALERT;
}

IPState FocuserInterface::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(ticks);
    // Must be implemented by child class
    return IPS_ALERT;
}

IPState FocuserInterface::MoveAbsFocuser(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    // Must be implemented by child class
    return IPS_ALERT;
}

bool FocuserInterface::AbortFocuser()
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Focuser does not support abort motion.");
    return false;
}

bool FocuserInterface::ReverseFocuser(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Focuser does not support reverse motion.");
    return false;
}

bool FocuserInterface::SyncFocuser(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Focuser does not support syncing.");
    return false;
}

bool FocuserInterface::SetFocuserSpeed(int speed)
{
    INDI_UNUSED(speed);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Focuser does not support variable speed.");
    return false;
}

bool FocuserInterface::SetFocuserMaxPosition(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    return true;
}

bool FocuserInterface::SetFocuserBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Focuser does not support backlash compensation.");
    return false;
}

bool FocuserInterface::SetFocuserBacklashEnabled(bool enabled)
{
    // If disabled, set the focuser backlash to zero.
    if (enabled)
        return SetFocuserBacklash(static_cast<int32_t>(FocusBacklashN[0].value));
    else
        return SetFocuserBacklash(0);
}

bool FocuserInterface::saveConfigItems(FILE * fp)
{
    if (CanAbsMove())
        IUSaveConfigNumber(fp, &FocusMaxPosNP);
    if (CanReverse())
        IUSaveConfigSwitch(fp, &FocusReverseSP);
    if (HasBacklash())
    {
        IUSaveConfigSwitch(fp, &FocusBacklashSP);
        IUSaveConfigNumber(fp, &FocusBacklashNP);
    }

    return true;
}

}
