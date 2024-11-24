/*******************************************************************************
 Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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
#include "dome_simulator.h"

#include "indicom.h"

#include <cmath>
#include <memory>
#include <unistd.h>

// We declare an auto pointer to domeSim.
static std::unique_ptr<DomeSim> domeSim(new DomeSim());

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
DomeSim::DomeSim()
{
    targetAz        = 0;
    prev_az         = 0;
    prev_alt        = 0;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::initProperties()
{
    INDI::Dome::initProperties();

    SpeedNP[Dome].fill("DOME", "Dome (deg/s)", "%.2f", 0.1, 10, 1, 5);
    SpeedNP[Shutter].fill("SHUTTER", "Shutter (m/s)", "%.2f", 0.01, 1, 0.1, 0.1);
    SpeedNP.fill(getDeviceName(), "SPEEDS", "Speeds", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    SetParkDataType(PARK_AZ);

    addAuxControls();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::SetupParms()
{
    targetAz     = 0;
    DomeAbsPosNP[0].setValue(0);
    DomeAbsPosNP.apply();

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(90);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(90);
        SetAxis1ParkDefault(90);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *DomeSim::getDefaultName()
{
    return "Dome Simulator";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(SpeedNP);
        SetupParms();
    }
    else
    {
        deleteProperty(SpeedNP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::Connect()
{
    SetTimer(1000);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::Disconnect()
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SpeedNP.isNameMatch(name))
        {
            SpeedNP.update(values, names, n);
            SpeedNP.setState(IPS_OK);
            SpeedNP.apply();
            saveConfig(SpeedNP);
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void DomeSim::TimerHit()
{
    if (!isConnected())
        return;

    auto domeSpeed = SpeedNP[Dome].getValue();
    auto shutterSpeed = SpeedNP[Shutter].getValue();

    if (DomeAbsPosNP.getState() == IPS_BUSY)
    {
        // Find shortest distance given target degree
        double a = targetAz;
        double b = DomeAbsPosNP[0].getValue();
        int sign = (a - b >= 0 && a - b <= 180) || (a - b <= -180 && a - b >= -360) ? 1 : -1;
        double diff = domeSpeed * sign;
        b += diff;
        DomeAbsPosNP[0].setValue(range360(b));

        if (std::abs(targetAz - DomeAbsPosNP[0].getValue()) <= domeSpeed)
        {
            DomeAbsPosNP[0].setValue(targetAz);
            LOG_INFO("Dome reached requested azimuth angle.");

            if (getDomeState() == DOME_PARKING)
                SetParked(true);
            else if (getDomeState() == DOME_UNPARKING)
                SetParked(false);
            else
                setDomeState(DOME_SYNCED);
        }

        DomeAbsPosNP.apply();
    }

    // Dome shutter, we decrease by shutter speed in meters/second
    if (DomeShutterSP.getState() == IPS_BUSY)
    {
        m_ShutterDistance -= shutterSpeed;

        if (m_ShutterDistance <= 0)
        {
            m_ShutterDistance = 0;
            DomeShutterSP.setState(IPS_OK);
            LOGF_INFO("Shutter is %s.", (DomeShutterSP[0].getState() == ISS_ON ? "open" : "closed"));
            DomeShutterSP.apply();

            if (getDomeState() == DOME_UNPARKING)
                SetParked(false);
        }
    }

    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        targetAz = (dir == DOME_CW) ? 1e6 : -1e6;
        DomeAbsPosNP.setState(IPS_BUSY);
    }
    else
    {
        targetAz = 0;
        DomeAbsPosNP.setState(IPS_IDLE);
    }

    DomeAbsPosNP.apply();
    return ((operation == MOTION_START) ? IPS_BUSY : IPS_OK);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::MoveAbs(double az)
{
    targetAz = az;

    // Requested position is within one cycle, let's declare it done
    if (std::abs(az - DomeAbsPosNP[0].getValue()) < SpeedNP[Dome].getValue())
        return IPS_OK;

    // It will take a few cycles to reach final position
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::MoveRel(double azDiff)
{
    targetAz = range360(DomeAbsPosNP[0].getValue() + azDiff);

    // Requested position is within one cycle, let's declare it done
    if (std::abs(targetAz - DomeAbsPosNP[0].value) < SpeedNP[Dome].getValue())
        return IPS_OK;

    // It will take a few cycles to reach final position
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::Park()
{
    targetAz = DomeParamNP[0].getValue();
    Dome::ControlShutter(SHUTTER_CLOSE);
    Dome::MoveAbs(GetAxis1Park());

    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::UnPark()
{
    return Dome::ControlShutter(SHUTTER_OPEN);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomeSim::ControlShutter(ShutterOperation operation)
{
    INDI_UNUSED(operation);
    // If shutter width = 1 meter, then we only need to open half of that since we presume
    // two sides are moving at the same speed closing or opening.
    m_ShutterDistance = DomeMeasurementsNP[DM_SHUTTER_WIDTH].getValue() / 2;
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::Abort()
{
    // If we abort while in the middle of opening/closing shutter, alert.
    if (DomeShutterSP.getState() == IPS_BUSY)
    {
        DomeShutterSP.setState(IPS_ALERT);
        LOG_ERROR("Shutter operation aborted. Status: unknown.");
        DomeShutterSP.apply();
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DomeSim::saveConfigItems(FILE * fp)
{
    Dome::saveConfigItems(fp);
    SpeedNP.save(fp);
    return true;
}