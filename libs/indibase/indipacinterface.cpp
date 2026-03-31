/*
    PAC Interface (Polar Alignment Correction)
    Copyright (C) 2026 Joaquin Rodriguez (jrhuerta@gmail.com)
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indipacinterface.h"
#include "defaultdevice.h"
#include "indilogger.h"

#include <cstring>

namespace INDI
{

PACInterface::PACInterface(DefaultDevice *defaultDevice)
    : m_DefaultDevice(defaultDevice)
{
}

void PACInterface::initProperties(const char *group)
{
    // Manual adjustment: write non-zero value to move that axis immediately.
    // AZ step: positive = East, negative = West.
    // ALT step: positive = North (increase altitude), negative = South (decrease altitude).
    ManualAdjustmentNP[MANUAL_AZ].fill("MANUAL_AZ_STEP", "Azimuth Step (deg)", "%.4f", -10, 10, 0.1, 0);
    ManualAdjustmentNP[MANUAL_ALT].fill("MANUAL_ALT_STEP", "Altitude Step (deg)", "%.4f", -10, 10, 0.1, 0);
    ManualAdjustmentNP.fill(m_DefaultDevice->getDeviceName(), "PAC_MANUAL_ADJUSTMENT", "Manual Adjustment",
                            group, IP_WO, 0, IPS_IDLE);

    // Abort motion
    AbortSP[0].fill("ABORT", "Abort", ISS_OFF);
    AbortSP.fill(m_DefaultDevice->getDeviceName(), "PAC_ABORT_MOTION", "Abort Motion",
                 group, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Current position (only shown when PAC_HAS_POSITION is set)
    PositionNP[POSITION_AZ].fill("POSITION_AZ", "Azimuth (deg)", "%.4f", -360, 360, 0, 0);
    PositionNP[POSITION_ALT].fill("POSITION_ALT", "Altitude (deg)", "%.4f", -90, 90, 0, 0);
    PositionNP.fill(m_DefaultDevice->getDeviceName(), "PAC_POSITION", "Position",
                    group, IP_RO, 0, IPS_IDLE);

    // Motor speed (only shown when PAC_HAS_SPEED is set)
    SpeedNP[0].fill("PAC_SPEED_VALUE", "Speed", "%.0f", 1, 10, 1, 1);
    SpeedNP.fill(m_DefaultDevice->getDeviceName(), "PAC_SPEED", "Speed", group, IP_RW, 60, IPS_OK);

    // Azimuth reverse (only shown when PAC_CAN_REVERSE is set)
    AZReverseSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    AZReverseSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    AZReverseSP.fill(m_DefaultDevice->getDeviceName(), "PAC_AZ_REVERSE", "Azimuth Reverse", group, IP_RW,
                     ISR_1OFMANY, 60, IPS_IDLE);

    // Altitude reverse (only shown when PAC_CAN_REVERSE is set)
    ALTReverseSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    ALTReverseSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    ALTReverseSP.fill(m_DefaultDevice->getDeviceName(), "PAC_ALT_REVERSE", "Altitude Reverse", group, IP_RW,
                      ISR_1OFMANY, 60, IPS_IDLE);

    // Home management (only shown when PAC_CAN_HOME is set)
    HomeSP[HOME_SET].fill("HOME_SET", "Set Home", ISS_OFF);
    HomeSP[HOME_GO].fill("HOME_GO", "Return Home", ISS_OFF);
    HomeSP[HOME_RESET].fill("HOME_RESET", "Reset Home", ISS_OFF);
    HomeSP.fill(m_DefaultDevice->getDeviceName(), "PAC_HOME", "Home", group, IP_RW,
                ISR_ATMOST1, 60, IPS_IDLE);

    // Sync position (only shown when PAC_CAN_SYNC is set)
    SyncNP[SYNC_AZ].fill("SYNC_AZ", "Azimuth (deg)", "%.4f", -360, 360, 0, 0);
    SyncNP[SYNC_ALT].fill("SYNC_ALT", "Altitude (deg)", "%.4f", -90, 90, 0, 0);
    SyncNP.fill(m_DefaultDevice->getDeviceName(), "PAC_SYNC", "Sync Position",
                group, IP_RW, 60, IPS_IDLE);

    // Backlash enable/disable (only shown when PAC_HAS_BACKLASH is set)
    BacklashSP[DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    BacklashSP[DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    BacklashSP.fill(m_DefaultDevice->getDeviceName(), "PAC_BACKLASH_ENABLED", "Backlash", group, IP_RW,
                    ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash steps (only shown when PAC_HAS_BACKLASH is set)
    BacklashNP[BACKLASH_AZ].fill("BACKLASH_AZ", "Azimuth (steps)", "%.0f", 0, 10000, 1, 0);
    BacklashNP[BACKLASH_ALT].fill("BACKLASH_ALT", "Altitude (steps)", "%.0f", 0, 10000, 1, 0);
    BacklashNP.fill(m_DefaultDevice->getDeviceName(), "PAC_BACKLASH_STEPS", "Backlash Steps",
                    group, IP_RW, 60, IPS_IDLE);
}

bool PACInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
    {
        m_DefaultDevice->defineProperty(ManualAdjustmentNP);
        m_DefaultDevice->defineProperty(AbortSP);
        if (HasPosition())
            m_DefaultDevice->defineProperty(PositionNP);
        if (HasSpeed())
            m_DefaultDevice->defineProperty(SpeedNP);
        if (CanReverse())
        {
            m_DefaultDevice->defineProperty(AZReverseSP);
            m_DefaultDevice->defineProperty(ALTReverseSP);
        }
        if (CanHome())
            m_DefaultDevice->defineProperty(HomeSP);
        if (CanSync())
            m_DefaultDevice->defineProperty(SyncNP);
        if (HasBacklash())
        {
            m_DefaultDevice->defineProperty(BacklashSP);
            m_DefaultDevice->defineProperty(BacklashNP);
        }
    }
    else
    {
        m_DefaultDevice->deleteProperty(ManualAdjustmentNP);
        m_DefaultDevice->deleteProperty(AbortSP);
        if (HasPosition())
            m_DefaultDevice->deleteProperty(PositionNP);
        if (HasSpeed())
            m_DefaultDevice->deleteProperty(SpeedNP);
        if (CanReverse())
        {
            m_DefaultDevice->deleteProperty(AZReverseSP);
            m_DefaultDevice->deleteProperty(ALTReverseSP);
        }
        if (CanHome())
            m_DefaultDevice->deleteProperty(HomeSP);
        if (CanSync())
            m_DefaultDevice->deleteProperty(SyncNP);
        if (HasBacklash())
        {
            m_DefaultDevice->deleteProperty(BacklashSP);
            m_DefaultDevice->deleteProperty(BacklashNP);
        }
    }

    return true;
}

bool PACInterface::processSwitch(const char *dev, const char *name,
                                 ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);

    if (AbortSP.isNameMatch(name))
    {
        AbortSP.reset();
        if (AbortMotion())
        {
            AbortSP.setState(IPS_OK);
            if (ManualAdjustmentNP.getState() == IPS_BUSY)
            {
                ManualAdjustmentNP.setState(IPS_IDLE);
                ManualAdjustmentNP.apply();
            }
            if (HasPosition() && PositionNP.getState() == IPS_BUSY)
            {
                PositionNP.setState(IPS_IDLE);
                PositionNP.apply();
            }
        }
        else
        {
            AbortSP.setState(IPS_ALERT);
        }
        AbortSP.apply();
        return true;
    }

    if (AZReverseSP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(AZReverseSP, states, names, n, [this, states]()
        {
            return ReverseAZ(states[0] == ISS_ON);
        }, true);
        return true;
    }

    if (ALTReverseSP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(ALTReverseSP, states, names, n, [this, states]()
        {
            return ReverseALT(states[0] == ISS_ON);
        }, true);
        return true;
    }

    if (CanHome() && HomeSP.isNameMatch(name))
    {
        HomeSP.update(states, names, n);
        int cmd = HomeSP.findOnSwitchIndex();
        HomeSP.reset();

        if (cmd == HOME_SET)
        {
            if (SetHome())
            {
                HomeSP.setState(IPS_OK);
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_SESSION, "Home position set.");
            }
            else
            {
                HomeSP.setState(IPS_ALERT);
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to set home position.");
            }
        }
        else if (cmd == HOME_GO)
        {
            IPState rc = GoHome();
            HomeSP.setState(rc);
            if (rc == IPS_BUSY)
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_SESSION, "Returning to home position...");
            else if (rc == IPS_OK)
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_SESSION, "Returned to home position.");
            else
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to return to home position.");
        }
        else if (cmd == HOME_RESET)
        {
            if (ResetHome())
            {
                HomeSP.setState(IPS_OK);
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_SESSION, "Home position reset.");
            }
            else
            {
                HomeSP.setState(IPS_ALERT);
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to reset home position.");
            }
        }

        HomeSP.apply();
        return true;
    }

    if (HasBacklash() && BacklashSP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(BacklashSP, states, names, n, [this, states]()
        {
            return SetBacklashEnabled(states[0] == ISS_ON);
        }, true);
        return true;
    }

    return false;
}

bool PACInterface::processNumber(const char *dev, const char *name,
                                 double values[], char *names[], int n)
{
    INDI_UNUSED(dev);

    if (SpeedNP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(SpeedNP, values, names, n, [this, values]()
        {
            return SetPACSpeed(static_cast<uint16_t>(values[0]));
        }, true);
        return true;
    }

    if (ManualAdjustmentNP.isNameMatch(name))
    {
        ManualAdjustmentNP.update(values, names, n);

        const double azStep  = ManualAdjustmentNP[MANUAL_AZ].getValue();
        const double altStep = ManualAdjustmentNP[MANUAL_ALT].getValue();

        IPState azState  = IPS_OK;
        IPState altState = IPS_OK;

        if (azStep != 0.0 && altStep != 0.0)
        {
            // Both axes requested — delegate to the driver's coordinated move
            IPState state = MoveBoth(azStep, altStep);
            azState = altState = state;
        }
        else
        {
            if (azStep  != 0.0) azState  = MoveAZ(azStep);
            if (altStep != 0.0) altState = MoveALT(altStep);
        }

        // Overall state: worst of the two axes.
        if (azState == IPS_ALERT || altState == IPS_ALERT)
            ManualAdjustmentNP.setState(IPS_ALERT);
        else if (azState == IPS_BUSY || altState == IPS_BUSY)
            ManualAdjustmentNP.setState(IPS_BUSY);
        else
            ManualAdjustmentNP.setState(IPS_OK);

        ManualAdjustmentNP.apply();
        return true;
    }

    if (CanSync() && SyncNP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(SyncNP, values, names, n, [this, values, names, n]()
        {
            SyncNP.update(values, names, n);
            bool azOk  = SyncAZ(SyncNP[SYNC_AZ].getValue());
            bool altOk = SyncALT(SyncNP[SYNC_ALT].getValue());
            return (azOk && altOk);
        }, true);
        return true;
    }

    if (HasBacklash() && BacklashNP.isNameMatch(name))
    {
        m_DefaultDevice->updateProperty(BacklashNP, values, names, n, [this, values, names, n]()
        {
            BacklashNP.update(values, names, n);
            bool azOk  = SetBacklashAZ(static_cast<int32_t>(BacklashNP[BACKLASH_AZ].getValue()));
            bool altOk = SetBacklashALT(static_cast<int32_t>(BacklashNP[BACKLASH_ALT].getValue()));
            return (azOk && altOk);
        }, true);
        return true;
    }

    return false;
}

bool PACInterface::saveConfigItems(FILE *fp)
{
    if (HasSpeed())
        SpeedNP.save(fp);
    if (CanReverse())
    {
        AZReverseSP.save(fp);
        ALTReverseSP.save(fp);
    }
    if (CanHome())
        HomeSP.save(fp);
    if (CanSync())
        SyncNP.save(fp);
    if (HasBacklash())
    {
        BacklashSP.save(fp);
        BacklashNP.save(fp);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Default virtual implementations
// ---------------------------------------------------------------------------

IPState PACInterface::MoveAZ(double degrees)
{
    INDI_UNUSED(degrees);
    return IPS_ALERT;
}

IPState PACInterface::MoveALT(double degrees)
{
    INDI_UNUSED(degrees);
    return IPS_ALERT;
}

IPState PACInterface::MoveBoth(double azDegrees, double altDegrees)
{
    // Default: call individual axis moves in sequence.
    // Drivers that support a single combined command should override this.
    IPState azState  = MoveAZ(azDegrees);
    IPState altState = MoveALT(altDegrees);
    if (azState == IPS_ALERT || altState == IPS_ALERT) return IPS_ALERT;
    if (azState == IPS_BUSY  || altState == IPS_BUSY)  return IPS_BUSY;
    return IPS_OK;
}

bool PACInterface::AbortMotion()
{
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support aborting motion.");
    return false;
}

bool PACInterface::SetPACSpeed(uint16_t speed)
{
    INDI_UNUSED(speed);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support variable speed.");
    return false;
}

bool PACInterface::ReverseAZ(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support reversing azimuth direction.");
    return false;
}

bool PACInterface::ReverseALT(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support reversing altitude direction.");
    return false;
}

bool PACInterface::SetHome()
{
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support setting home.");
    return false;
}

IPState PACInterface::GoHome()
{
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support returning to home.");
    return IPS_ALERT;
}

bool PACInterface::ResetHome()
{
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support resetting home.");
    return false;
}

bool PACInterface::SyncAZ(double degrees)
{
    INDI_UNUSED(degrees);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support syncing azimuth position.");
    return false;
}

bool PACInterface::SyncALT(double degrees)
{
    INDI_UNUSED(degrees);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support syncing altitude position.");
    return false;
}

bool PACInterface::SetBacklashEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support backlash compensation.");
    return false;
}

bool PACInterface::SetBacklashAZ(int32_t steps)
{
    INDI_UNUSED(steps);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support setting azimuth backlash.");
    return false;
}

bool PACInterface::SetBacklashALT(int32_t steps)
{
    INDI_UNUSED(steps);
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), INDI::Logger::DBG_ERROR,
                "PAC device does not support setting altitude backlash.");
    return false;
}

}
