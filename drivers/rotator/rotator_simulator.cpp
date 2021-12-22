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
    // We do not have absolute ticks
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC);
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
    m_TargetAngle = range360(angle);
    return IPS_BUSY;
}

bool RotatorSimulator::SyncRotator(double angle)
{
    INDI_UNUSED(angle);
    return true;
}

bool RotatorSimulator::AbortRotator()
{
    return true;
}

void RotatorSimulator::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (GotoRotatorNP.s == IPS_BUSY)
    {
        if (std::fabs(m_TargetAngle - GotoRotatorN[0].value) <= ROTATION_RATE)
        {
            GotoRotatorN[0].value = m_TargetAngle;
            GotoRotatorNP.s = IPS_OK;
        }
        else
        {
            // Find shortest distance given target degree
            double a = m_TargetAngle;
            double b = GotoRotatorN[0].value;
            int sign = (a - b >= 0 && a - b <= 180) || (a - b <= -180 && a - b >= -360) ? 1 : -1;
            double diff = ROTATION_RATE * sign;
            GotoRotatorN[0].value += diff;
            GotoRotatorN[0].value = range360(GotoRotatorN[0].value);
        }

        IDSetNumber(&GotoRotatorNP, nullptr);
    }
    SetTimer(getCurrentPollingPeriod());
}
