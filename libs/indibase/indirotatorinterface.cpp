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

namespace INDI
{

RotatorInterface::RotatorInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

void RotatorInterface::initProperties(const char *groupName)
{
    // Rotator Angle
    // @INDI_STANDARD_PROPERTY@
    GotoRotatorNP[0].fill("ANGLE", "Angle", "%.2f", 0, 360., 10., 0.);
    GotoRotatorNP.fill(m_defaultDevice->getDeviceName(), "ABS_ROTATOR_ANGLE", "Goto", groupName, IP_RW, 0, IPS_IDLE);

    // Abort Rotator
    // @INDI_STANDARD_PROPERTY@
    AbortRotatorSP[0].fill("ABORT", "Abort", ISS_OFF);
    AbortRotatorSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Sync
    // @INDI_STANDARD_PROPERTY@
    SyncRotatorNP[0].fill("ANGLE", "Angle", "%.2f", 0, 360., 10., 0.);
    SyncRotatorNP.fill(m_defaultDevice->getDeviceName(), "SYNC_ROTATOR_ANGLE", "Sync", groupName, IP_RW, 0, IPS_IDLE);

    // Home Rotator
    // @INDI_STANDARD_PROPERTY@
    HomeRotatorSP[0].fill("HOME", "Start", ISS_OFF);
    HomeRotatorSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_HOME", "Homing", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Reverse Direction
    // @INDI_STANDARD_PROPERTY@
    ReverseRotatorSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    ReverseRotatorSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    ReverseRotatorSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_REVERSE", "Reverse", groupName, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Backlash Compensation
    // @INDI_STANDARD_PROPERTY@
    RotatorBacklashSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    RotatorBacklashSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    RotatorBacklashSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_BACKLASH_TOGGLE", "Backlash", groupName, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation Value
    // @INDI_STANDARD_PROPERTY@
    RotatorBacklashNP[0].fill("ROTATOR_BACKLASH_VALUE", "Steps", "%.f", 0, 1e6, 100, 0);
    RotatorBacklashNP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_BACKLASH_STEPS", "Backlash", groupName, IP_RW, 60, IPS_OK);

    // Rotator Limits
    // @INDI_STANDARD_PROPERTY@
    RotatorLimitsNP[0].fill("ROTATOR_LIMITS_VALUE", "Max Range (degrees)", "%.f", 0, 360, 30, 0);
    RotatorLimitsNP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_LIMITS", "Limits", groupName, IP_RW, 60, IPS_IDLE);
}

bool RotatorInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        ////////////////////////////////////////////
        // Move Absolute Angle
        ////////////////////////////////////////////
        if (GotoRotatorNP.isNameMatch(name))
        {
            if (values[0] == GotoRotatorNP[0].getValue())
            {
                GotoRotatorNP.setState(IPS_OK);
                GotoRotatorNP.apply();
                return true;
            }

            // If value is outside safe zone, then prevent motion
            if (RotatorLimitsNP[0].getValue() > 0 && ((values[0] < 180
                    && (std::abs(values[0] - m_RotatorOffset)) > RotatorLimitsNP[0].getValue()) ||
                    (values[0] > 180 && (std::abs(values[0] - m_RotatorOffset)) < (360 - RotatorLimitsNP[0].getValue()))))
            {
                GotoRotatorNP.setState(IPS_ALERT);
                DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR,
                             "Rotator target %.2f exceeds safe limits of %.2f degrees...", values[0], RotatorLimitsNP[0].getValue());
                GotoRotatorNP.apply();
            }
            else
            {
                GotoRotatorNP.setState(MoveRotator(values[0]));
                GotoRotatorNP.apply();
                if (GotoRotatorNP.getState() == IPS_BUSY)
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator moving to %.2f degrees...", values[0]);
            }
            return true;
        }
        ////////////////////////////////////////////
        // Sync
        ////////////////////////////////////////////
        else if (SyncRotatorNP.isNameMatch(name))
        {
            if (values[0] == GotoRotatorNP[0].getValue())
            {
                SyncRotatorNP.setState(IPS_OK);
                SyncRotatorNP.apply();
                return true;
            }

            bool rc = SyncRotator(values[0]);
            SyncRotatorNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (rc)
            {
                SyncRotatorNP[0].setValue(values[0]);
                // Always reset offset after a sync
                m_RotatorOffset = values[0];
            }

            SyncRotatorNP.apply();
            return true;
        }
        ////////////////////////////////////////////
        // Backlash value
        ////////////////////////////////////////////
        else if (RotatorBacklashNP.isNameMatch(name))
        {
            if (RotatorBacklashSP[DefaultDevice::INDI_ENABLED].getState() != ISS_ON)
            {
                RotatorBacklashNP.setState(IPS_IDLE);
                DEBUGDEVICE(dev, Logger::DBG_WARNING, "Rotatorer backlash must be enabled first.");
            }
            else
            {
                uint32_t steps = static_cast<uint32_t>(values[0]);
                if (SetRotatorBacklash(steps))
                {
                    RotatorBacklashNP[0].setValue(values[0]);
                    RotatorBacklashNP.setState(IPS_OK);
                }
                else
                    RotatorBacklashNP.setState(IPS_ALERT);
            }
            RotatorBacklashNP.apply();
            return true;
        }
        ////////////////////////////////////////////
        // Limits
        ////////////////////////////////////////////
        else if (RotatorLimitsNP.isNameMatch(name))
        {
            RotatorLimitsNP.update(values, names, n);
            RotatorLimitsNP.setState(IPS_OK);
            RotatorLimitsNP.apply();
            if (RotatorLimitsNP[0].getValue() == 0)
                DEBUGDEVICE(dev, Logger::DBG_SESSION, "Rotator limits are disabled.");
            m_RotatorOffset = GotoRotatorNP[0].getValue();
            return true;
        }
    }

    return false;
}

bool RotatorInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        ////////////////////////////////////////////
        // Abort
        ////////////////////////////////////////////
        if (AbortRotatorSP.isNameMatch(name))
        {
            AbortRotatorSP.setState(AbortRotator() ? IPS_OK : IPS_ALERT);
            AbortRotatorSP.apply();
            if (AbortRotatorSP.getState() == IPS_OK)
            {
                if (GotoRotatorNP.getState() != IPS_OK)
                {
                    GotoRotatorNP.setState(IPS_OK);
                    GotoRotatorNP.apply();
                }
            }
            return true;
        }
        ////////////////////////////////////////////
        // Home
        ////////////////////////////////////////////
        else if (HomeRotatorSP.isNameMatch(name))
        {
            HomeRotatorSP.setState(HomeRotator());
            HomeRotatorSP.reset();
            if (HomeRotatorSP.getState() == IPS_BUSY)
                HomeRotatorSP[0].setState(ISS_ON);
            HomeRotatorSP.apply();
            return true;
        }
        ////////////////////////////////////////////
        // Reverse Rotator
        ////////////////////////////////////////////
        else if (ReverseRotatorSP.isNameMatch(name))
        {
            int prevIndex = ReverseRotatorSP.findOnSwitchIndex();
            ReverseRotatorSP.update(states, names, n);
            const bool enabled = ReverseRotatorSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED;

            if (ReverseRotator(enabled))
            {
                ReverseRotatorSP.update(states, names, n);
                ReverseRotatorSP.setState(IPS_OK);
                DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator direction is %s.",
                             (enabled ? "reversed" : "normal"));
            }
            else
            {
                ReverseRotatorSP.reset();
                ReverseRotatorSP[prevIndex].setState(ISS_ON);
                ReverseRotatorSP.setState(IPS_ALERT);
                DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator reverse direction failed.");
            }

            ReverseRotatorSP.apply();
            return true;
        }
        ////////////////////////////////////////////
        // Backlash enable/disable
        ////////////////////////////////////////////
        else if (RotatorBacklashSP.isNameMatch(name))
        {
            int prevIndex = RotatorBacklashSP.findOnSwitchIndex();
            RotatorBacklashSP.update(states, names, n);
            const bool enabled = RotatorBacklashSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED;

            if (SetRotatorBacklashEnabled(enabled))
            {
                RotatorBacklashSP.setState(IPS_OK);
                DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator backlash is %s.",
                             (enabled ? "enabled" : "disabled"));
            }
            else
            {
                RotatorBacklashSP.reset();
                RotatorBacklashSP[prevIndex].setState(ISS_ON);
                RotatorBacklashSP.setState(IPS_ALERT);
                DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Failed to set trigger rotator backlash.");
            }

            RotatorBacklashSP.apply();
            return true;
        }
    }

    return false;
}

bool RotatorInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        m_defaultDevice->defineProperty(GotoRotatorNP);

        if (CanAbort())
            m_defaultDevice->defineProperty(AbortRotatorSP);
        if (CanSync())
            m_defaultDevice->defineProperty(SyncRotatorNP);
        if (CanHome())
            m_defaultDevice->defineProperty(HomeRotatorSP);
        if (CanReverse())
            m_defaultDevice->defineProperty(ReverseRotatorSP);
        if (HasBacklash())
        {
            m_defaultDevice->defineProperty(RotatorBacklashSP);
            m_defaultDevice->defineProperty(RotatorBacklashNP);
        }
        m_defaultDevice->defineProperty(RotatorLimitsNP);
    }
    else
    {
        m_defaultDevice->deleteProperty(GotoRotatorNP);

        if (CanAbort())
            m_defaultDevice->deleteProperty(AbortRotatorSP);
        if (CanSync())
            m_defaultDevice->deleteProperty(SyncRotatorNP);
        if (CanHome())
            m_defaultDevice->deleteProperty(HomeRotatorSP);
        if (CanReverse())
            m_defaultDevice->deleteProperty(ReverseRotatorSP);
        if (HasBacklash())
        {
            m_defaultDevice->deleteProperty(RotatorBacklashSP);
            m_defaultDevice->deleteProperty(RotatorBacklashNP);
        }
        m_defaultDevice->deleteProperty(RotatorLimitsNP);
    }

    return true;
}

bool RotatorInterface::SyncRotator(double angle)
{
    INDI_UNUSED(angle);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Rotator does not support syncing.");
    return false;
}

IPState RotatorInterface::HomeRotator()
{
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Rotator does not support homing.");
    return IPS_ALERT;
}

bool RotatorInterface::AbortRotator()
{
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Rotator does not support abort.");
    return false;
}

bool RotatorInterface::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Rotator does not support reverse.");
    return false;
}

bool RotatorInterface::SetRotatorBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Rotator does not support backlash compensation.");
    return false;
}

bool RotatorInterface::SetRotatorBacklashEnabled(bool enabled)
{
    // If disabled, set the Rotatorer backlash to zero.
    if (enabled)
        return SetRotatorBacklash(static_cast<int32_t>(RotatorBacklashNP[0].getValue()));
    else
        return SetRotatorBacklash(0);
}

bool RotatorInterface::saveConfigItems(FILE *fp)
{
    if (CanReverse())
    {
        ReverseRotatorSP.save(fp);
    }
    if (HasBacklash())
    {
        RotatorBacklashSP.save(fp);
        RotatorBacklashNP.save(fp);
    }
    RotatorLimitsNP.save(fp);

    return true;
}

}
