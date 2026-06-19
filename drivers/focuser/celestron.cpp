/*
    Celestron Focuser for SCT and EDGEHD

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2019 Chris Rowland

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

#include "celestron.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<CelestronSCT> celestronSCT(new CelestronSCT());

CelestronSCT::CelestronSCT() :
    truePosMax(0xffffffff),
    truePosMin(0),
    backlashMove(false),
    finalPosition(0),
    calibrateInProgress(false),
    calibrateState(0),
    focuserIsCalibrated(false)

{
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    // CR variable speed and sync removed
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_BACKLASH);

    communicator.source = Aux::Target::APP;
}

bool CelestronSCT::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser backlash
    // CR this is a value, positive or negative to define the direction.  It will need to be implemented
    // in the driver.
    FocusBacklashNP[0].setMin(-500);
    FocusBacklashNP[0].setMax(500);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(0);

    //    IUFillNumber(&FocusBacklashN[0], "STEPS", "Steps", "%.f", -500., 500., 1., 0.);
    //    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash",
    //                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // // Focuser min limit
    // IUFillNumber(&FocusMinPosN[0], "FOCUS_MIN_VALUE", "Steps", "%.f", 0, 40000., 1., 0.);
    // IUFillNumberVector(&FocusMinPosNP, FocusMinPosN, 1, getDeviceName(), "FOCUS_MIN", "Min. Position",
    //                    MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // focuser calibration
    CalibrateSP[START].fill("START", "Start Calibration", ISS_OFF);
    CalibrateSP[STOP].fill("STOP", "Stop Calibration", ISS_OFF);
    CalibrateSP.fill(getDeviceName(), "CALIBRATE", "Calibrate control",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    CalibrateStateTP[0].fill("CALIBRATE_STATE", "Calibrate state", "");
    CalibrateStateTP.fill(getDeviceName(), "CALIBRATE_STATE", "Calibrate State",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Speed range
    // CR no need to have adjustable speed, how to remove?
    FocusSpeedNP[0].setMin(0);
    FocusSpeedNP[0].setMax(3);
    FocusSpeedNP[0].setValue(1);

    // From online screenshots, seems maximum value is 60,000 steps
    // max and min positions can be read from a calibrated focuser

    // Relative Position Range
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(30000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    // Absolute Position Range
    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(60000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    // Maximum Position Settings
    FocusMaxPosNP[0].setMax(60000);
    FocusMaxPosNP[0].setMin(1000);
    FocusMaxPosNP[0].setValue(60000);
    FocusMaxPosNP.setPermission(IP_RO);

    // Poll every 500ms
    setDefaultPollingPeriod(500);

    // Add debugging support
    addDebugControl();

    // Set default baud rate to 9600
    // On aarch64 19200 or more seems to crash the whole USB hub.
    // stty -a says the baud rate is 9600
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);

    communicator.setDeviceName(getDeviceName());

    // Default port to /dev/ttyACM0
    //serialConnection->setDefaultPort("/dev/ttyACM0");

    //LOG_INFO("initProperties end");
    return true;
}

bool CelestronSCT::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineProperty(&FocusBacklashNP);

        // defineProperty(&FocusMinPosNP);

        defineProperty(CalibrateSP);
        defineProperty(CalibrateStateTP);

        if (getStartupParameters())
            LOG_INFO("Celestron SCT focuser parameters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to retrieve some focuser parameters. Check logs.");

    }
    else
    {
        //deleteProperty(FocusBacklashNP.name);
        // deleteProperty(FocusMinPosNP.name);
        deleteProperty(CalibrateSP);
        deleteProperty(CalibrateStateTP);
    }

    return true;
}

bool CelestronSCT::Handshake()
{
    if (Ack())
    {
        LOG_INFO("Celestron SCT Focuser is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retrieving data from Celestron SCT, please ensure Celestron SCT controller is powered and the port is correct.");
    return false;
}

const char * CelestronSCT::getDefaultName()
{
    return "Celestron SCT";
}

bool CelestronSCT::Ack()
{
    // send simple command to focuser and check response to make sure
    // it is online and responding
    // use get firmware version command
    Aux::buffer reply;
    if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::GET_VER, reply))
        return false;

    // Check minimum size for firmware version
    if (reply.empty())
    {
        LOG_ERROR("Empty response from focuser");
        return false;
    }

    // Ensure we have at least 2 bytes for major.minor version
    if (reply.size() < 2)
    {
        LOG_ERROR("Incomplete firmware version response");
        return false;
    }

    if (reply.size() >= 4)
    {
        // Full version with build number
        uint16_t build = (reply[2] << 8);
        if (reply.size() > 4)
            build += reply[3];
        LOGF_INFO("Firmware Version %d.%d.%d", reply[0], reply[1], build);
    }
    else
    {
        // Just major.minor version
        LOGF_INFO("Firmware Version %d.%d", reply[0], reply[1]);
    }
    return true;
}

bool CelestronSCT::readPosition()
{
    Aux::buffer reply;
    if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_GET_POSITION, reply))
        return false;

    // Position response should be 3 bytes
    if (reply.size() < 3)
    {
        LOG_ERROR("Invalid position response size");
        return false;
    }

    int truePos = (reply[0] << 16) + (reply[1] << 8) + reply[2];
    LOGF_DEBUG("True Position %d", truePos);
    FocusAbsPosNP[0].setValue(absPos(truePos));
    return true;
}

bool CelestronSCT::isMoving()
{
    Aux::buffer reply;
    if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_SLEW_DONE, reply))
    {
        LOG_ERROR("Failed to get motion status");
        return false;
    }

    if (reply.empty())
    {
        LOG_ERROR("Empty motion status response");
        return false;
    }

    return reply[0] != static_cast<uint8_t>(0xFF);
}

// read the focuser limits from the hardware
bool CelestronSCT::readLimits()
{
    Aux::buffer reply;
    if(!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::FOC_GET_HS_POSITIONS, reply))
        return false;

    // Limits response should be 8 bytes (4 bytes each for min and max positions)
    if (reply.size() < 8)
    {
        LOG_ERROR("Invalid limits response size");
        return false;
    }

    truePosMin = (reply[0] << 24) + (reply[1] << 16) + (reply[2] << 8) + reply[3];
    truePosMax = (reply[4] << 24) + (reply[5] << 16) + (reply[6] << 8) + reply[7];

    // check on integrity of values
    if (truePosMax <= truePosMin)
    {
        focuserIsCalibrated = false;
        LOGF_INFO("Focus range %i to %i invalid", truePosMin, truePosMax);
        return false;
    }

    // absolute direction is reverse from true
    FocusAbsPosNP[0].setMax(absPos(truePosMin));
    FocusMaxPosNP[0].setValue(absPos(truePosMin));
    FocusAbsPosNP.setState(IPS_OK);
    FocusMaxPosNP.setState(IPS_OK);
    FocusAbsPosNP.updateMinMax();
    FocusMaxPosNP.apply();

    // FocusMinPosNP[0].setValue(lo);
    // FocusMinPosNP.s = IPS_OK;
    // IDSetNumber(&FocusMinPosNP, nullptr);

    focuserIsCalibrated = true;
    LOGF_INFO("Focus range %i to %i valid", truePosMin, truePosMax);

    return true;
}

//bool CelestronSCT::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
//{
//    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
//    {
//        // Backlash
//        if (!strcmp(name, FocusBacklashNP.name))
//        {
//            // just update the number
//            IUUpdateNumber(&FocusBacklashNP, values, names, n);
//            FocusBacklashNP.s = IPS_OK;
//            IDSetNumber(&FocusBacklashNP, nullptr);
//            return true;
//        }
//    }
//    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
//}

bool CelestronSCT::ISNewSwitch(const char * dev, const char * name, ISState *states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CalibrateSP.isNameMatch(name))
        {
            CalibrateSP.update(states, names, n);
            int index = CalibrateSP.findOnSwitchIndex();
            Aux::buffer data = {1};
            switch(index)
            {
                case 0:
                    // start calibrate
                    LOG_INFO("Focuser Calibrate start");
                    calibrateInProgress = true;
                    calibrateState = -1;
                    break;
                case 1:
                    // abort calibrate
                    LOG_INFO("Focuser Calibrate abort");
                    data[0] = 0;
                    break;
                default:
                    return false;
            }
            communicator.commandBlind(PortFD, Aux::Target::FOCUSER, Aux::Command::FOC_CALIB_ENABLE, data);
            usleep(500000);
            CalibrateSP.setState(IPS_BUSY);
            CalibrateSP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool CelestronSCT::getStartupParameters()
{
    bool rc1 = false, rc2 = false;

    if ( (rc1 = readPosition()))
        FocusAbsPosNP.apply();

    if ( !(rc2 = readLimits()))
    {
        LOG_WARN("Focuser not calibrated, You MUST calibrate before moves are allowed.");
    }

    return (rc1 && rc2);
}

IPState CelestronSCT::MoveAbsFocuser(uint32_t targetTicks)
{
    // Send command to focuser
    // If OK and moving, return IPS_BUSY (CR don't see this, it seems to just start a new move)
    // If OK and motion already done (was very small), return IPS_OK
    // If error, return IPS_ALERT

    if (!focuserIsCalibrated)
    {
        LOG_ERROR("Move not allowed because focuser is not calibrated.");
        return IPS_ALERT;
    }
    if (calibrateInProgress)
    {
        LOG_WARN("Move not allowed because a calibration is in progress");
        return IPS_ALERT;
    }

    // the focuser seems happy to move 500 steps past the soft limit so don't check backlash
    if (targetTicks > FocusMaxPosNP[0].getValue())
        // targetTicks < FocusMinPosN[0].value)

    {
        LOGF_ERROR("Move to %i not allowed because it is out of range", targetTicks);
        return IPS_ALERT;
    }

    uint32_t position = targetTicks;

    // implement backlash
    int delta = targetTicks - FocusAbsPosNP[0].getValue();
    if ((FocusBacklashNP[0].getValue() < 0 && delta > 0) || (FocusBacklashNP[0].getValue() > 0 && delta < 0))
    {
        backlashMove = true;
        finalPosition = position;
        position -= FocusBacklashNP[0].getValue();
    }

    if (!startMove(position))
        return IPS_ALERT;

    return IPS_BUSY;
}

bool CelestronSCT::startMove(uint32_t absPos)
{

    int position = truePos(absPos);

    Aux::buffer data =
    {
        static_cast<uint8_t>((position >> 16) & 0xFF),
        static_cast<uint8_t>((position >> 8) & 0xFF),
        static_cast<uint8_t>(position & 0xFF)
    };

    LOGF_DEBUG("startMove to true position %i, %x %x %x", position, data[0], data[1], data[2]);

    return communicator.commandBlind(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_GOTO_FAST, data);
}

IPState CelestronSCT::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    return MoveAbsFocuser(newPosition);
}

void CelestronSCT::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // Check position
    double lastPosition = FocusAbsPosNP[0].getValue();
    bool rc = readPosition();
    if (rc)
    {
        // Only update if there is actual change
        if (fabs(lastPosition - FocusAbsPosNP[0].getValue()) > 1)
            FocusAbsPosNP.apply();
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        // CR The backlash handling will probably have to be done here, if the move state
        // shows that a backlash move has been done then the final move needs to be started
        // and the states left at IPS_BUSY

        // There are two ways to know when focuser motion is over
        // define class variable uint32_t m_TargetPosition and set it in MoveAbsFocuser(..) function
        // then compare current value to m_TargetPosition
        // The other way is to have a function that calls a focuser specific function about motion
        if (!isMoving())
        {
            if (backlashMove)
            {
                backlashMove = false;
                if (startMove(finalPosition))
                    LOGF_INFO("Backlash move to %i", finalPosition);
                else
                    LOG_INFO("Backlash move failed");
            }
            else
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    if (calibrateInProgress)
    {
        usleep(500000);     // slowing things down while calibrating seems to help
        // check the calibration state
        Aux::buffer reply;
        if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::FOC_CALIB_DONE, reply))
        {
            LOG_ERROR("Failed to get calibration status");
            return;
        }

        // Need at least 2 bytes for complete flag and state
        if (reply.size() < 2)
        {
            LOG_ERROR("Invalid calibration status response size");
            return;
        }

        bool complete = reply[0] > 0;
        int state = reply[1];

        if (complete || state == 0)
        {
            // a completed calibration returns complete as true, an aborted calibration sets the status to zero

            const char *msg = complete ? "Calibrate complete" : "Calibrate aborted";
            LOG_INFO(msg);
            calibrateInProgress = false;
            CalibrateSP[STOP].setState(ISS_OFF);
            CalibrateSP.setState(IPS_OK);
            CalibrateStateTP[0].setText(msg);
            CalibrateSP.apply();
            CalibrateStateTP.apply();
            // read the new limits
            if (complete && readLimits())
            {
                FocusAbsPosNP.updateMinMax();
                FocusMaxPosNP.apply();
                // IDSetNumber(&FocusMinPosNP, nullptr);
            }
        }
        else
        {
            if (state != calibrateState)
            {
                calibrateState = state;
                char str[20];
                snprintf(str, 20, "Calibrate state %i", state);
                CalibrateStateTP[0].setText(str);
                CalibrateStateTP.apply(nullptr);
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool CelestronSCT::AbortFocuser()
{
    if (calibrateInProgress)
    {
        LOG_WARN("Abort move not allowed when calibrating, use abort calibration to stop");
        return false;
    }
    // send a command to move at rate 0
    Aux::buffer data = {0};
    return communicator.commandBlind(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_MOVE_POS, data);
}

bool CelestronSCT::SetFocuserBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    return true;
}
