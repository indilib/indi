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
    }

    return true;
}

bool PACInterface::processSwitch(const char *dev, const char *name,
                                 ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

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
        int prevIndex = AZReverseSP.findOnSwitchIndex();
        AZReverseSP.update(states, names, n);

        if (ReverseAZ(AZReverseSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED))
        {
            AZReverseSP.setState(IPS_OK);
            m_DefaultDevice->saveConfig(AZReverseSP);
        }
        else
        {
            AZReverseSP.reset();
            AZReverseSP[prevIndex].setState(ISS_ON);
            AZReverseSP.setState(IPS_ALERT);
        }
        AZReverseSP.apply();
        return true;
    }

    if (ALTReverseSP.isNameMatch(name))
    {
        int prevIndex = ALTReverseSP.findOnSwitchIndex();
        ALTReverseSP.update(states, names, n);

        if (ReverseALT(ALTReverseSP.findOnSwitchIndex() == DefaultDevice::INDI_ENABLED))
        {
            ALTReverseSP.setState(IPS_OK);
            m_DefaultDevice->saveConfig(ALTReverseSP);
        }
        else
        {
            ALTReverseSP.reset();
            ALTReverseSP[prevIndex].setState(ISS_ON);
            ALTReverseSP.setState(IPS_ALERT);
        }
        ALTReverseSP.apply();
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
        uint16_t currentSpeed = static_cast<uint16_t>(SpeedNP[0].getValue());
        SpeedNP.update(values, names, n);

        if (SetPACSpeed(static_cast<uint16_t>(SpeedNP[0].getValue())))
        {
            SpeedNP.setState(IPS_OK);
            m_DefaultDevice->saveConfig(SpeedNP);
        }
        else
        {
            SpeedNP[0].setValue(currentSpeed);
            SpeedNP.setState(IPS_ALERT);
        }
        SpeedNP.apply();
        return true;
    }

    if (ManualAdjustmentNP.isNameMatch(name))
    {
        ManualAdjustmentNP.update(values, names, n);

        const double azStep  = ManualAdjustmentNP[MANUAL_AZ].getValue();
        const double altStep = ManualAdjustmentNP[MANUAL_ALT].getValue();

        IPState azState  = IPS_OK;
        IPState altState = IPS_OK;

        if (azStep != 0.0)
            azState = MoveAZ(azStep);

        if (altStep != 0.0)
            altState = MoveALT(altStep);

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

}
