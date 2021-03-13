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

void ISGetProperties(const char *dev)
{
    rotatorSimulator->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    rotatorSimulator->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int n)
{
    rotatorSimulator->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    rotatorSimulator->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    rotatorSimulator->ISSnoopDevice(root);
}

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
            GotoRotatorNP[0].value += diff;
            GotoRotatorNP[0].setValue(range360(GotoRotatorNP[0].value));
        }

        GotoRotatorNP.apply();
    }
    SetTimer(getCurrentPollingPeriod());
}
