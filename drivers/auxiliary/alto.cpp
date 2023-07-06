/*
    ALTO driver
    Copyright (C) 2023 Jasem Mutlaq
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

#include "alto.h"

#include <memory>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>

static std::unique_ptr<ALTO> sesto(new ALTO());

ALTO::ALTO() : DustCapInterface()
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::initProperties()
{

    INDI::DefaultDevice::initProperties();

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    // Calibrate Toggle
    CalibrateToggleSP[INDI_ENABLED].fill("INDI_ENABLED", "Start", ISS_OFF);
    CalibrateToggleSP[INDI_DISABLED].fill("INDI_DISABLED", "Stop", ISS_OFF);
    CalibrateToggleSP.fill(getDeviceName(), "CALIBRATE_TOGGLE", "Calibrate", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                           IPS_IDLE);

    // Calibrate Speed
    MotionSpeedSP[Slow].fill("SLOW", "Slow", ISS_OFF);
    MotionSpeedSP[Fast].fill("FAST", "Fast", ISS_ON);
    MotionSpeedSP.fill(getDeviceName(), "MOTION_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Calibrate Command
    MotionCommandSP[Open].fill("OPEN", "Open", ISS_OFF);
    MotionCommandSP[Close].fill("CLOSE", "Close", ISS_OFF);
    MotionCommandSP[Stop].fill("STOP", "Stop", ISS_OFF);
    MotionCommandSP.fill(getDeviceName(), "MOTION_COMMAND", "Command", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Position
    PositionNP[0].fill("POSITION", "Steps", "%.f", 0, 100, 10, 0);
    PositionNP.fill(getDeviceName(), "POSITION_STEPS", "Position", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Check parking status
        uint8_t value = 0;
        try
        {
            if (m_ALTO->getPosition(value))
            {
                ParkCapS[CAP_PARK].s = value == 0 ? ISS_ON : ISS_OFF;
                ParkCapS[CAP_UNPARK].s = value == 0 ? ISS_OFF : ISS_ON;
            }
        }
        catch (json::exception &e)
        {
            LOGF_ERROR("%s %d", e.what(), e.id);
        }

        defineProperty(&ParkCapSP);
        defineProperty(PositionNP);
        defineProperty(MotionSpeedSP);
        defineProperty(MotionCommandSP);
        defineProperty(CalibrateToggleSP);
    }
    else
    {
        deleteProperty(ParkCapSP.name);
        deleteProperty(PositionNP);
        deleteProperty(MotionSpeedSP);
        deleteProperty(MotionCommandSP);
        deleteProperty(CalibrateToggleSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::Handshake()
{
    PortFD = serialConnection->getPortFD();
    m_ALTO.reset(new PrimalucaLabs::ALTO(getDeviceName(), PortFD));
    std::string model;
    if (m_ALTO->getModel(model))
    {
        LOGF_INFO("%s is online. Detected model %s", getDeviceName(), model.c_str());
        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure ALTO is powered and the port is correct.");
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *ALTO::getDefaultName()
{
    return "ALTO";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Position
    if (PositionNP.isNameMatch(name))
    {
        m_TargetPosition = values[0];

        try
        {
            auto rc = m_ALTO->setPosition(m_TargetPosition);
            PositionNP.setState(rc ? IPS_BUSY : IPS_ALERT);
            PositionNP.apply();
        }
        catch (json::exception &e)
        {
            LOGF_ERROR("%s %d", e.what(), e.id);
        }

        return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (processDustCapSwitch(dev, name, states, names, n))
        return true;

    // Motion Speed
    if (MotionSpeedSP.isNameMatch(name))
    {
        MotionSpeedSP.update(states, names, n);
        MotionSpeedSP.setState(IPS_OK);
        MotionSpeedSP.apply();
        saveConfig(true, MotionSpeedSP.getName());
        return true;
    }

    // Motion Command
    if (MotionCommandSP.isNameMatch(name))
    {
        MotionCommandSP.update(states, names, n);
        auto command = MotionCommandSP.findOnSwitchIndex();
        auto rc = false;
        switch (command)
        {
            case Open:
                rc = m_ALTO->open(MotionSpeedSP[Fast].getState() == ISS_ON);
                break;
            case Close:
                rc = m_ALTO->close(MotionSpeedSP[Fast].getState() == ISS_ON);
                break;
            case Stop:
                rc = m_ALTO->stop();
                if (m_CalibrationStatus == findClosePosition)
                {
                    m_ALTO->storeClosedPosition();
                    LOG_INFO("Close position recorded. Open cover to maximum position then click stop.");
                    m_CalibrationStatus = findOpenPosition;
                }
                else if (m_CalibrationStatus == findOpenPosition)
                {
                    m_ALTO->storeOpenPosition();
                    LOG_INFO("Open position recorded. Calibration completed.");
                    m_CalibrationStatus = Idle;
                    CalibrateToggleSP.reset();
                    CalibrateToggleSP.setState(IPS_IDLE);
                    CalibrateToggleSP.apply();
                }
                break;
        }

        MotionCommandSP.reset();
        MotionCommandSP.setState(rc ? (command == Stop ? IPS_IDLE : IPS_BUSY) : IPS_ALERT);
        MotionCommandSP.apply();
        return true;
    }

    // Calibrate
    if (CalibrateToggleSP.isNameMatch(name))
    {
        CalibrateToggleSP.update(states, names, n);
        auto isToggled = CalibrateToggleSP[INDI_ENABLED].getState() == ISS_ON;
        CalibrateToggleSP.setState(isToggled ? IPS_BUSY : IPS_IDLE);

        if (isToggled)
        {
            m_ALTO->initCalibration();
            m_CalibrationStatus = findClosePosition;
            LOG_INFO("Calibration started. Close cover to minimum position then click stop.");
        }
        else
        {
            m_CalibrationStatus = Idle;
            LOG_INFO("Calibration complete.");
        }

        CalibrateToggleSP.apply();
        return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ALTO::ParkCap()
{
    if (m_ALTO->Park())
    {
        m_TargetPosition = 0;
        PositionNP.setState(IPS_BUSY);
        PositionNP.apply();
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ALTO::UnParkCap()
{
    if (m_ALTO->UnPark())
    {
        m_TargetPosition = 100;
        PositionNP.setState(IPS_BUSY);
        PositionNP.apply();
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ALTO::saveConfigItems(FILE * fp)
{
    MotionSpeedSP.save(fp);
    return INDI::DefaultDevice::saveConfigItems(fp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void ALTO::TimerHit()
{
    // Check Park Cap status
    if (ParkCapSP.s == IPS_BUSY)
    {
        json status;
        try
        {
            m_ALTO->getStatus(status);
            std::string mst = status["MST"];
            if (mst == "stop")
            {
                ParkCapSP.s = IPS_OK;
                IDSetSwitch(&ParkCapSP, nullptr);
            }
        }
        catch (json::exception &e)
        {
            LOGF_ERROR("%s %d", e.what(), e.id);
        }
    }

    // Check position updates
    if (PositionNP.getState() == IPS_BUSY)
    {
        uint8_t newPosition = PositionNP[0].value;
        try
        {
            m_ALTO->getPosition(newPosition);
        }
        catch (json::exception &e)
        {
            LOGF_ERROR("%s %d", e.what(), e.id);
        }

        if (newPosition == m_TargetPosition)
        {
            PositionNP[0].setValue(m_TargetPosition);
            PositionNP.setState(IPS_OK);
            PositionNP.apply();
        }
        else if (newPosition != PositionNP[0].getValue())
        {
            PositionNP[0].setValue(newPosition);
            PositionNP.apply();
        }
    }

    SetTimer(getCurrentPollingPeriod());
}
