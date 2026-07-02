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
#include <cmath>

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
    AbortRotatorSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1,
                        0, IPS_IDLE);

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
    ReverseRotatorSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_REVERSE", "Reverse", groupName, IP_RW, ISR_1OFMANY, 0,
                          IPS_IDLE);

    // Backlash Compensation
    // @INDI_STANDARD_PROPERTY@
    RotatorBacklashSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    RotatorBacklashSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    RotatorBacklashSP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_BACKLASH_TOGGLE", "Backlash", groupName, IP_RW,
                           ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation Value
    // @INDI_STANDARD_PROPERTY@
    RotatorBacklashNP[0].fill("ROTATOR_BACKLASH_VALUE", "Steps", "%.f", 0, 1e6, 100, 0);
    RotatorBacklashNP.fill(m_defaultDevice->getDeviceName(), "ROTATOR_BACKLASH_STEPS", "Backlash", groupName, IP_RW, 60,
                           IPS_OK);

    // Rotator Limits
    // @INDI_STANDARD_PROPERTY@
    RotatorLimitsNP[0].fill("ROTATOR_LIMITS_VALUE", "Safe Range (degrees)", "%.f", 0, 360, 30, 0);
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
            moveRotatorSafely(values[0]);
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
                // Sync redefines the cable-neutral reference point.
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
                RotatorBacklashNP.apply();
                return true;
            }

            m_defaultDevice->updateProperty(RotatorBacklashNP, values, names, n, [this, values]()
            {
                uint32_t steps = static_cast<uint32_t>(values[0]);
                return SetRotatorBacklash(steps);
            }, true);
            return true;
        }
        ////////////////////////////////////////////
        // Limits
        ////////////////////////////////////////////
        else if (RotatorLimitsNP.isNameMatch(name))
        {
            m_defaultDevice->updateProperty(RotatorLimitsNP, values, names, n, [this, values, dev]()
            {
                if (values[0] == 0)
                {
                    DEBUGDEVICE(dev, Logger::DBG_SESSION, "Rotator limits are disabled.");
                }
                else
                {
                    // Use the current hardware position as reference if not yet initialized.
                    double ref = (m_RotatorOffset >= 0) ? m_RotatorOffset : GotoRotatorNP[0].getValue();
                    double half       = values[0] / 2.0;
                    double startAngle = ref - half;
                    if (startAngle < 0) startAngle += 360.0;
                    double endAngle   = ref + half;
                    if (endAngle >= 360) endAngle -= 360.0;
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION,
                                 "Rotator limits set to %.f degrees. Safe range: %.2f to %.2f degrees (reference: %.2f°).",
                                 values[0], startAngle, endAngle, ref);
                }
                return true;
            }, true);
            return true;
        }
    }

    return false;
}

std::optional<std::pair<double,double> >
RotatorInterface::calculateBestPath(double angle)
{
    double current = GotoRotatorNP[0].getValue();

    // First calculate the shortest -180-180° distance to the target
    double delta = range180(angle - current);

    if (RotatorLimitsNP[0].getValue() <= 0)
        /* no safe-zone defined, so rotating to shortest distance is fine. */
        return {{angle, delta}};

    // Need to ensure that we stay inside the safe zone.  This means that we
    // never take a shorter path through the non-safe zone.  If the requested
    // angle is *outside* the safe zone, then prevent motion.

    // The safe zone is a symmetric window of ±(limit/2) degrees centered on the
    // cable-neutral reference point (m_RotatorOffset). This prevents cable wrap
    // regardless of how many incremental moves are commanded.
    // limit=0 disables the check entirely.
    double limit  = RotatorLimitsNP[0].getValue();

    // for most of the following tests, we ignore the 0-360° requirement and do
    // things on a ±∞ scale centered on the safe-zone offset to avoid
    // ambiguities.  When the motion is finally returned, this is done on the
    // correct scale and zero definition.
    // Define safe zone, centered around safe-zone offset
    double safe_zone[2] {-limit/2, +limit/2};

    double _curr   = range180(current - m_RotatorOffset);
    double _target = range180(  angle - m_RotatorOffset);

    if (_curr < safe_zone[0] || safe_zone[1] < _curr)
    {
        DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR,
                     "Rotator position (%.2f) already exceeds safe limits! "
                     "Disable safe-zone, move device, then re-enable safe-zone "
                     "(±%.2f° from reference %.2f°)",
                     current, limit, m_RotatorOffset);
        return {};
    }
    if (_target < safe_zone[0] || safe_zone[1] < _target)
    {
        DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR,
                     "Rotator target (%.2f) exceeds safe limits! "
                     "(±%.2f° from reference %.2f°)",
                     angle, limit, m_RotatorOffset);
        return {};
    }
    delta = _target - _curr; // do *not* fix for shortest distance

    return {{angle, delta}};
}

bool RotatorInterface::moveRotatorSafely(double angle)
{
    double current_angle = GotoRotatorNP[0].getValue();
    if (angle == current_angle)
    {
        GotoRotatorNP.setState(IPS_OK);
        GotoRotatorNP.apply();
        return true;
    }

    // Lazily initialize the cable-neutral reference from the first known hardware
    // position. By the time the first goto is commanded, GotoRotatorNP[0] will
    // have been populated (either in the driver's updateProperties or via TimerHit).
    if (m_RotatorOffset < 0)
        m_RotatorOffset = current_angle;

    auto best_path = calculateBestPath(angle);
    if (!best_path)
    {
        /* Reject move request and stay inside safe zone (or tell user to fix a
         * bad starting condition). */
        GotoRotatorNP.setState(IPS_ALERT);
        GotoRotatorNP.apply();
        return false;
    }
    auto [target_angle, delta] = best_path.value();

    GotoRotatorNP.setState(MoveRotator(target_angle, delta));
    GotoRotatorNP.apply();
    if (GotoRotatorNP.getState() == IPS_BUSY)
        DEBUGFDEVICE(m_defaultDevice->getDeviceName(),
                     Logger::DBG_SESSION,
                     "Rotator moving by Δ=%.2f° to %.2f°...",
                     delta, target_angle);
    return true;
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
            m_defaultDevice->updateProperty(ReverseRotatorSP, states, names, n, [this, names]()
            {
                bool enabled = ReverseRotatorSP[DefaultDevice::INDI_ENABLED].isNameMatch(names[0]) == ISS_ON;

                if (ReverseRotator(enabled))
                {
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator direction is %s.",
                                 (enabled ? "reversed" : "normal"));
                    return true;
                }
                DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator reverse direction failed.");
                return false;
            }, true);

            return true;
        }
        ////////////////////////////////////////////
        // Backlash enable/disable
        ////////////////////////////////////////////
        else if (RotatorBacklashSP.isNameMatch(name))
        {
            m_defaultDevice->updateProperty(RotatorBacklashSP, states, names, n, [this, names]()
            {
                bool enabled = RotatorBacklashSP[DefaultDevice::INDI_ENABLED].isNameMatch(names[0]) == ISS_ON;

                if (SetRotatorBacklashEnabled(enabled))
                {
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Rotator backlash is %s.",
                                 (enabled ? "enabled" : "disabled"));
                    return true;
                }
                DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Failed to set trigger rotator backlash.");
                return false;
            }, true);

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
