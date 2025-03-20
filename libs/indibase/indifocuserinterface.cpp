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
    // Focuser Speed
    FocusSpeedNP[0].fill("FOCUS_SPEED_VALUE", "Focus Speed", "%3.0f", 0.0, 255.0, 1.0, 255.0);
    FocusSpeedNP.fill(m_defaultDevice->getDeviceName(), "FOCUS_SPEED", "Speed", groupName, IP_RW, 60, IPS_OK);

    // Focus Timer
    FocusTimerNP[0].fill("FOCUS_TIMER_VALUE", "Focus Timer (ms)", "%4.0f", 0.0, 5000.0, 50.0, 1000.0);
    FocusTimerNP.fill(m_defaultDevice->getDeviceName(), "FOCUS_TIMER", "Timer", groupName, IP_RW, 60, IPS_OK);
    lastTimerValue = 1000.0;

    // Focus Motion
    FocusMotionSP[FOCUS_INWARD].fill("FOCUS_INWARD", "Focus In", ISS_ON);
    FocusMotionSP[FOCUS_OUTWARD].fill("FOCUS_OUTWARD", "Focus Out", ISS_OFF);
    FocusMotionSP.fill(m_defaultDevice->getDeviceName(), "FOCUS_MOTION", "Direction", groupName, IP_RW, ISR_1OFMANY, 60,
                       IPS_OK);

    // Absolute Position
    FocusAbsPosNP[0].fill("FOCUS_ABSOLUTE_POSITION", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    FocusAbsPosNP.fill(m_defaultDevice->getDeviceName(), "ABS_FOCUS_POSITION", "Absolute Position", groupName, IP_RW, 60,
                       IPS_OK);

    // Relative Position
    FocusRelPosNP[0].fill("FOCUS_RELATIVE_POSITION", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    FocusRelPosNP.fill(m_defaultDevice->getDeviceName(), "REL_FOCUS_POSITION", "Relative Position", groupName, IP_RW, 60,
                       IPS_OK);

    // Sync
    FocusSyncNP[0].fill("FOCUS_SYNC_VALUE", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    FocusSyncNP.fill(m_defaultDevice->getDeviceName(), "FOCUS_SYNC", "Sync", groupName, IP_RW, 60, IPS_OK);

    // Maximum Position
    FocusMaxPosNP[0].fill("FOCUS_MAX_VALUE", "Steps", "%.f", 1e3, 1e6, 1e4, 1e5);
    FocusMaxPosNP.fill(m_defaultDevice->getDeviceName(), "FOCUS_MAX", "Max. Position", groupName, IP_RW, 60, IPS_OK);

    // Abort
    FocusAbortSP[0].fill("ABORT", "Abort", ISS_OFF);
    FocusAbortSP.fill(m_defaultDevice->getDeviceName(), "FOCUS_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1, 60,
                      IPS_IDLE);

    // Reverse
    FocusReverseSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    FocusReverseSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    FocusReverseSP.fill(m_defaultDevice->getDeviceName(), "FOCUS_REVERSE_MOTION", "Reverse Motion", groupName, IP_RW,
                        ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation
    FocusBacklashSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    FocusBacklashSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    FocusBacklashSP.fill(m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_TOGGLE", "Backlash", groupName, IP_RW, ISR_1OFMANY,
                         60, IPS_IDLE);

    // Backlash Compensation Value
    FocusBacklashNP[0].fill("FOCUS_BACKLASH_VALUE", "Steps", "%.f", 0, 1e6, 100, 0);
    FocusBacklashNP.fill(m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_STEPS", "Backlash", groupName, IP_RW, 60, IPS_OK);
}

bool FocuserInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        //  Now we add our focusser specific stuff
        m_defaultDevice->defineProperty(FocusMotionSP);

        if (HasVariableSpeed())
        {
            m_defaultDevice->defineProperty(FocusSpeedNP);

            // We only define Focus Timer if we can not absolute move
            if (CanAbsMove() == false)
                m_defaultDevice->defineProperty(FocusTimerNP);
        }
        if (CanRelMove())
            m_defaultDevice->defineProperty(FocusRelPosNP);
        if (CanAbsMove())
        {
            m_defaultDevice->defineProperty(FocusAbsPosNP);
            m_defaultDevice->defineProperty(FocusMaxPosNP);
        }
        if (CanAbort())
            m_defaultDevice->defineProperty(FocusAbortSP);
        if (CanSync())
            m_defaultDevice->defineProperty(FocusSyncNP);
        if (CanReverse())
            m_defaultDevice->defineProperty(FocusReverseSP);
        if (HasBacklash())
        {
            m_defaultDevice->defineProperty(FocusBacklashSP);
            m_defaultDevice->defineProperty(FocusBacklashNP);
        }
    }
    else
    {
        m_defaultDevice->deleteProperty(FocusMotionSP);
        if (HasVariableSpeed())
        {
            m_defaultDevice->deleteProperty(FocusSpeedNP);

            if (CanAbsMove() == false)
                m_defaultDevice->deleteProperty(FocusTimerNP);
        }
        if (CanRelMove())
            m_defaultDevice->deleteProperty(FocusRelPosNP);
        if (CanAbsMove())
        {
            m_defaultDevice->deleteProperty(FocusAbsPosNP);
            m_defaultDevice->deleteProperty(FocusMaxPosNP);
        }
        if (CanAbort())
            m_defaultDevice->deleteProperty(FocusAbortSP);
        if (CanSync())
            m_defaultDevice->deleteProperty(FocusSyncNP);
        if (CanReverse())
            m_defaultDevice->deleteProperty(FocusReverseSP);
        if (HasBacklash())
        {
            m_defaultDevice->deleteProperty(FocusBacklashSP);
            m_defaultDevice->deleteProperty(FocusBacklashNP);
        }
    }

    return true;
}

bool FocuserInterface::processNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        // Move focuser based on requested timeout
        if (FocusTimerNP.isNameMatch(name))
        {
            FocusDirection dir;
            int speed;
            int t;

            //  first we get all the numbers just sent to us
            FocusTimerNP.update(values, names, n);

            //  Now lets find what we need for this move
            speed = FocusSpeedNP[0].getValue();

            if (FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON)
                dir = FOCUS_INWARD;
            else
                dir = FOCUS_OUTWARD;

            t              = FocusTimerNP[0].getValue();
            lastTimerValue = t;

            FocusTimerNP.setState(MoveFocuser(dir, speed, t));
            FocusTimerNP.apply();
            return true;
        }

        // Set variable focus speed
        if (FocusSpeedNP.isNameMatch(name))
        {
            FocusSpeedNP.setState(IPS_OK);
            int current_speed = FocusSpeedNP[0].getValue();
            FocusSpeedNP.update(values, names, n);

            if (SetFocuserSpeed(FocusSpeedNP[0].getValue()) == false)
            {
                FocusSpeedNP[0].setValue(current_speed);
                FocusSpeedNP.setState(IPS_ALERT);
                m_defaultDevice->saveConfig(true, FocusSpeedNP.getName());
            }

            //  Update client display
            FocusSpeedNP.apply();
            return true;
        }

        // Update Maximum Position allowed
        if (FocusMaxPosNP.isNameMatch(name))
        {
            uint32_t maxTravel = rint(values[0]);
            if (SetFocuserMaxPosition(maxTravel))
            {
                FocusMaxPosNP.update(values, names, n);

                FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getValue());
                FocusSyncNP[0].setMax(FocusMaxPosNP[0].getValue());

                FocusAbsPosNP[0].setStep(FocusMaxPosNP[0].getValue() / 50.0);
                FocusSyncNP[0].setStep(FocusMaxPosNP[0].getValue() / 50.0);

                FocusAbsPosNP[0].setMin(0);
                FocusSyncNP[0].setMin(0);

                FocusRelPosNP[0].setMax(FocusMaxPosNP[0].getValue() / 2);
                FocusRelPosNP[0].setStep(FocusMaxPosNP[0].getValue() / 100.0);
                FocusRelPosNP[0].setMin(0);

                FocusAbsPosNP.updateMinMax();
                FocusRelPosNP.updateMinMax();
                FocusSyncNP.updateMinMax();

                m_defaultDevice->saveConfig(true, FocusMaxPosNP.getName());

                FocusMaxPosNP.setState(IPS_OK);
            }
            else
                FocusMaxPosNP.setState(IPS_ALERT);

            FocusMaxPosNP.apply();
            return true;
        }

        // Sync
        if (FocusSyncNP.isNameMatch(name))
        {
            if (SyncFocuser(rint(values[0])))
            {
                FocusSyncNP[0].setValue(rint(values[0]));
                FocusAbsPosNP[0].setValue(rint(values[0]));
                FocusSyncNP.setState(IPS_OK);
                FocusSyncNP.apply();
                FocusAbsPosNP.apply();
            }
            else
            {
                FocusSyncNP.setState(IPS_ALERT);
                FocusSyncNP.apply();
            }
            return true;
        }

        // Set backlash value
        if (FocusBacklashNP.isNameMatch(name))
        {
            if (FocusBacklashSP[DefaultDevice::INDI_ENABLED].getState() != ISS_ON)
            {
                FocusBacklashNP.setState(IPS_IDLE);

                // Only warn if there is non-zero backlash value.
                if (values[0] > 0)
                    DEBUGDEVICE(dev, Logger::DBG_WARNING, "Focuser backlash must be enabled first.");
            }
            else
            {
                uint32_t steps = static_cast<uint32_t>(values[0]);
                if (SetFocuserBacklash(steps))
                {
                    FocusBacklashNP[0].setValue(values[0]);
                    FocusBacklashNP.setState(IPS_OK);
                    m_defaultDevice->saveConfig(true, FocusBacklashNP.getName());
                }
                else
                    FocusBacklashNP.setState(IPS_ALERT);
            }
            FocusBacklashNP.apply();
            return true;
        }

        // Update Absolute Focuser Position
        if (FocusAbsPosNP.isNameMatch(name))
        {
            int newPos = rint(values[0]);

            if (newPos < FocusAbsPosNP[0].getMin())
            {
                FocusAbsPosNP.setState(IPS_ALERT);
                FocusAbsPosNP.apply();
                DEBUGFDEVICE(dev, Logger::DBG_ERROR, "Requested position out of bound. Focus minimum position is %g",
                             FocusAbsPosNP[0].getMin());
                return true;
            }
            else if (newPos > FocusAbsPosNP[0].getMax())
            {
                FocusAbsPosNP.setState(IPS_ALERT);
                FocusAbsPosNP.apply();
                DEBUGFDEVICE(dev, Logger::DBG_ERROR, "Requested position out of bound. Focus maximum position is %g",
                             FocusAbsPosNP[0].getMax());
                return true;
            }

            IPState ret;

            if ((ret = MoveAbsFocuser(newPos)) == IPS_OK)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusAbsPosNP.update(values, names, n);
                DEBUGFDEVICE(dev, Logger::DBG_SESSION, "Focuser moved to position %d", newPos);
                FocusAbsPosNP.apply();
                return true;
            }
            else if (ret == IPS_BUSY)
            {
                FocusAbsPosNP.setState(IPS_BUSY);
                DEBUGFDEVICE(dev, Logger::DBG_SESSION, "Focuser is moving to position %d", newPos);
                FocusAbsPosNP.apply();
                return true;
            }

            FocusAbsPosNP.setState(IPS_ALERT);
            DEBUGDEVICE(dev, Logger::DBG_ERROR, "Focuser failed to move to new requested position.");
            FocusAbsPosNP.apply();
            return true;
        }

        // Update Relative focuser steps. This moves the focuser CW/CCW by this number of steps.
        if (FocusRelPosNP.isNameMatch(name))
        {
            int newPos = rint(values[0]);

            if (newPos <= 0)
            {
                DEBUGDEVICE(dev, Logger::DBG_ERROR, "Relative ticks value must be greater than zero.");
                FocusRelPosNP.setState(IPS_ALERT);
                FocusRelPosNP.apply();
                return true;
            }

            IPState ret;

            if (CanAbsMove())
            {
                if (FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON)
                {
                    if (FocusAbsPosNP[0].getValue() - newPos < FocusAbsPosNP[0].getMin())
                    {
                        FocusRelPosNP.setState(IPS_ALERT);
                        FocusRelPosNP.apply();
                        DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                                     "Requested position out of bound. Focus minimum position is %g", FocusAbsPosNP[0].getMin());
                        return true;
                    }
                }
                else
                {
                    if (FocusAbsPosNP[0].getValue() + newPos > FocusAbsPosNP[0].getMax())
                    {
                        FocusRelPosNP.setState(IPS_ALERT);
                        FocusRelPosNP.apply();
                        DEBUGFDEVICE(dev, Logger::DBG_ERROR,
                                     "Requested position out of bound. Focus maximum position is %g", FocusAbsPosNP[0].getMax());
                        return true;
                    }
                }
            }

            if ((ret = MoveRelFocuser((FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD),
                                      newPos)) == IPS_OK)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.update(values, names, n);
                FocusRelPosNP.apply("Focuser moved %d steps %s", newPos,
                                    FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON ? "inward" : "outward");
                FocusAbsPosNP.apply();
                return true;
            }
            else if (ret == IPS_BUSY)
            {
                FocusRelPosNP.update(values, names, n);
                FocusRelPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply("Focuser is moving %d steps %s...", newPos,
                                    FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON ? "inward" : "outward");
                FocusRelPosNP.apply();
                return true;
            }

            FocusRelPosNP.setState(IPS_ALERT);
            DEBUGDEVICE(dev, Logger::DBG_ERROR, "Focuser failed to move to new requested position.");
            FocusRelPosNP.apply();
            return true;
        }
    }

    return false;
}

bool FocuserInterface::processSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        //  This one is for focus motion
        if (FocusMotionSP.isNameMatch(name))
        {
            // Record last direction and state.
            FocusDirection prevDirection = FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD;
            IPState prevState = FocusMotionSP.getState();

            FocusMotionSP.update(states, names, n);

            FocusDirection targetDirection = FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD;

            if (CanRelMove() || CanAbsMove() || HasVariableSpeed())
            {
                FocusMotionSP.setState(IPS_OK);
            }
            // If we are dealing with a simple dumb DC focuser, we move in a specific direction in an open-loop fashion until stopped.
            else
            {
                // If we are reversing direction let's issue abort first.
                if (prevDirection != targetDirection && prevState == IPS_BUSY)
                    AbortFocuser();

                FocusMotionSP.setState(MoveFocuser(targetDirection, 0, 0));
            }

            FocusMotionSP.apply();

            return true;
        }

        // Backlash compensation
        else if (FocusBacklashSP.isNameMatch(name))
        {
            int prevIndex = FocusBacklashSP.findOnSwitchIndex();
            FocusBacklashSP.update(states, names, n);

            if (SetFocuserBacklashEnabled(FocusBacklashSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED))
            {
                FocusBacklashSP.update(states, names, n);
                FocusBacklashSP.setState(IPS_OK);
                m_defaultDevice->saveConfig(true, FocusBacklashSP.getName());
            }
            else
            {
                FocusBacklashSP.reset();
                FocusBacklashSP[prevIndex].setState(ISS_ON);
                FocusBacklashSP.setState(IPS_ALERT);
            }

            FocusBacklashSP.apply();
            return true;
        }

        // Abort Focuser
        else if (FocusAbortSP.isNameMatch(name))
        {
            FocusAbortSP.reset();

            if (AbortFocuser())
            {
                FocusAbortSP.setState(IPS_OK);
                if (CanAbsMove() && FocusAbsPosNP.getState() != IPS_IDLE)
                {
                    FocusAbsPosNP.setState(IPS_IDLE);
                    FocusAbsPosNP.apply();
                }
                if (CanRelMove() && FocusRelPosNP.getState() != IPS_IDLE)
                {
                    FocusRelPosNP.setState(IPS_IDLE);
                    FocusRelPosNP.apply();
                }
            }
            else
                FocusAbortSP.setState(IPS_ALERT);

            FocusAbortSP.apply();
            return true;
        }

        // Reverse Motion
        else if (FocusReverseSP.isNameMatch(name))
        {
            int prevIndex = FocusReverseSP.findOnSwitchIndex();
            FocusReverseSP.update(states, names, n);

            if (ReverseFocuser(FocusReverseSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED))
            {
                FocusReverseSP.setState(IPS_OK);
                m_defaultDevice->saveConfig(true, FocusReverseSP.getName());
            }
            else
            {
                FocusReverseSP.reset();
                FocusReverseSP[prevIndex].setState(ISS_ON);
                FocusReverseSP.setState(IPS_ALERT);
            }

            FocusReverseSP.apply();
            return true;
        }
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
        return SetFocuserBacklash(static_cast<int32_t>(FocusBacklashNP[0].getValue()));
    else
        return SetFocuserBacklash(0);
}

bool FocuserInterface::saveConfigItems(FILE * fp)
{
    if (CanAbsMove())
        FocusMaxPosNP.save(fp);
    if (CanReverse())
        FocusReverseSP.save(fp);
    if (HasBacklash())
    {
        FocusBacklashSP.save(fp);
        FocusBacklashNP.save(fp);
    }

    return true;
}

}
