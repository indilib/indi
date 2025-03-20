/*
    INDI Rotator Simulator
    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "rotator_simulator.h"
#include "indicom.h"

#include <memory>
#include <cmath>
#include <math.h>

std::unique_ptr<RotatorSimulator> rotatorSimulator(new RotatorSimulator());

RotatorSimulator::RotatorSimulator()
{
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);
}

const char * RotatorSimulator::getDefaultName()
{
    return "Rotator Simulator";
}

bool RotatorSimulator::Connect()
{
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool RotatorSimulator::Disconnect()
{
    return true;
}

IPState RotatorSimulator::MoveRotator(double angle)
{
    if (ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON)
        m_TargetAngle = range360(360 - angle);
    else
        m_TargetAngle = range360(angle);
    return IPS_BUSY;
}

bool RotatorSimulator::SyncRotator(double angle)
{
    GotoRotatorNP[0].setValue(angle);
    GotoRotatorNP.apply();
    return true;
}

bool RotatorSimulator::AbortRotator()
{
    return true;
}

bool RotatorSimulator::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

void RotatorSimulator::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (GotoRotatorNP.getState() == IPS_BUSY)
    {
        if (std::fabs(m_TargetAngle - GotoRotatorNP[0].getValue()) <= ROTATION_RATE)
        {
            GotoRotatorNP[0].setValue(m_TargetAngle);
            GotoRotatorNP.setState(IPS_OK);
        }
        else
        {
            // Find shortest distance given target degree
            double a = m_TargetAngle;
            double b = GotoRotatorNP[0].getValue();
            int sign = (a - b >= 0 && a - b <= 180) || (a - b <= -180 && a - b >= -360) ? 1 : -1;
            double diff = ROTATION_RATE * sign;
            GotoRotatorNP[0].setValue(GotoRotatorNP[0].getValue() + diff);
            GotoRotatorNP[0].setValue(range360(GotoRotatorNP[0].getValue()));
        }

        GotoRotatorNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}
