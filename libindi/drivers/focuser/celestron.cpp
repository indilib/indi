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

void ISGetProperties(const char * dev)
{
    celestronSCT->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    celestronSCT->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    celestronSCT->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    celestronSCT->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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

void ISSnoopDevice(XMLEle * root)
{
    celestronSCT->ISSnoopDevice(root);
}

CelestronSCT::CelestronSCT()
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
    FocusBacklashN[0].min = -500;
    FocusBacklashN[0].max = 500;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 0;

    //    IUFillNumber(&FocusBacklashN[0], "STEPS", "Steps", "%.f", -500., 500., 1., 0.);
    //    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash",
    //                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser min limit
    IUFillNumber(&FocusMinPosN[0], "FOCUS_MIN_VALUE", "Steps", "%.f", 0, 40000., 1., 0.);
    IUFillNumberVector(&FocusMinPosNP, FocusMinPosN, 1, getDeviceName(), "FOCUS_MIN", "Min. Position",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // focuser calibration
    IUFillSwitch(&CalibrateS[0], "START", "Start Calibration", ISS_OFF);
    IUFillSwitch(&CalibrateS[1], "STOP", "Stop Calibration", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 2, getDeviceName(), "CALIBRATE", "Calibrate control",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillText(&CalibrateStateT[0], "CALIBRATE_STATE", "Calibrate state", "");
    IUFillTextVector(&CalibrateStateTP, CalibrateStateT, 1, getDeviceName(), "CALIBRATE_STATE", "Calibrate State",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Speed range
    // CR no need to have adjustable speed, how to remove?
    FocusSpeedN[0].min   = 1;
    FocusSpeedN[0].max   = 3;
    FocusSpeedN[0].value = 1;

    // From online screenshots, seems maximum value is 60,000 steps
    // max and min positions can be read from a calibrated focuser

    // Relative Position Range
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 30000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    // Absolute Postition Range
    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 60000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    // Maximum Position Settings
    FocusMaxPosN[0].max   = 60000;
    FocusMaxPosN[0].min   = 1000;
    FocusMaxPosN[0].value = 60000;
    FocusMaxPosNP.p = IP_RO;

    // Poll every 500ms
    setDefaultPollingPeriod(500);

    // Add debugging support
    addDebugControl();

    // Set default baud rate to 19200
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    communicator.setDeviceName(getDeviceName());

    // Defualt port to /dev/ttyACM0
    //serialConnection->setDefaultPort("/dev/ttyACM0");

    //LOG_INFO("initProperties end");
    return true;
}

bool CelestronSCT::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineNumber(&FocusBacklashNP);

        defineNumber(&FocusMinPosNP);

        defineSwitch(&CalibrateSP);
        defineText(&CalibrateStateTP);

        if (getStartupParameters())
            LOG_INFO("Celestron SCT focuser paramaters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to retrieve some focuser parameters. Check logs.");

    }
    else
    {
        //deleteProperty(FocusBacklashNP.name);
        deleteProperty(FocusMinPosNP.name);
        deleteProperty(CalibrateSP.name);
        deleteProperty(CalibrateStateTP.name);
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

    LOG_INFO("Error retreiving data from Celestron SCT, please ensure Celestron SCT controller is powered and the port is correct.");
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

    if (reply.size() == 4)
    {
        LOGF_INFO("Firmware Version %i.%i.%i", reply[0], reply [1], (reply[2] << 8) + reply[3]);
    }
    else
        LOGF_INFO("Firmware Version %i.%i", reply[0], reply [1]);
    return true;
}

bool CelestronSCT::readPosition()
{
    Aux::buffer reply;
    if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_GET_POSITION, reply))
        return false;

    int position = (reply[0] << 16) + (reply[1] << 8) + reply[2];
    LOGF_DEBUG("Position %i", position);
    FocusAbsPosN[0].value = position;
    return true;
}

bool CelestronSCT::isMoving()
{
    Aux::buffer reply(1);
    if (!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_SLEW_DONE, reply))
        return false;
    return reply[0] != static_cast<uint8_t>(0xFF);
}

// read the focuser limits from the hardware
bool CelestronSCT::readLimits()
{
    Aux::buffer reply(8);
    if(!communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::FOC_GET_HS_POSITIONS, reply))
        return false;

    int lo = (reply[0] << 24) + (reply[1] << 16) + (reply[2] << 8) + reply[3];
    int hi = (reply[4] << 24) + (reply[5] << 16) + (reply[6] << 8) + reply[7];

    FocusAbsPosN[0].max = hi;
    FocusAbsPosN[0].min = lo;
    FocusAbsPosNP.s = IPS_OK;
    IUUpdateMinMax(&FocusAbsPosNP);

    FocusMaxPosN[0].value = hi;
    FocusMaxPosNP.s = IPS_OK;
    IDSetNumber(&FocusMaxPosNP, nullptr);

    FocusMinPosN[0].value = lo;
    FocusMinPosNP.s = IPS_OK;
    IDSetNumber(&FocusMinPosNP, nullptr);

    // check on integrity of values, they must be sensible and the range must be more than 2 turns
    if (hi > 0 && lo > 0 && hi - lo > 2000 && hi <= 60000 && lo < 50000)
    {
        focuserIsCalibrated = true;
        LOGF_INFO("Focus range %i to %i valid", hi, lo);
    }
    else
    {
        focuserIsCalibrated = false;
        LOGF_INFO("Focus range %i to %i invalid", hi, lo);
        return false;
    }

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
        if (!strcmp(name, CalibrateSP.name))
        {
            IUUpdateSwitch(&CalibrateSP, states, names, n);
            int index = IUFindOnSwitchIndex(&CalibrateSP);
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
            CalibrateSP.s = IPS_BUSY;
            IDSetSwitch(&CalibrateSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool CelestronSCT::getStartupParameters()
{
    bool rc1 = false, rc2 = false;

    if ( (rc1 = readPosition()))
        IDSetNumber(&FocusAbsPosNP, nullptr);

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
        LOG_WARN("Move not allowed because a calinration is in progress");
        return IPS_ALERT;
    }

    // the focuser seems happy to move 500 steps past the soft limit so don't check backlash
    if (targetTicks > FocusMaxPosN[0].value ||
            targetTicks < FocusMinPosN[0].value)
    {
        LOGF_ERROR("Move to %i not allowed because it is out of range", targetTicks);
        return IPS_ALERT;
    }

    uint32_t position = targetTicks;

    // implement backlash
    int delta = targetTicks - FocusAbsPosN[0].value;
    if ((FocusBacklashN[0].value < 0 && delta > 0) ||
            (FocusBacklashN[0].value > 0 && delta < 0))
    {
        backlashMove = true;
        finalPosition = position;
        position -= FocusBacklashN[0].value;
    }

    if (!startMove(position))
        return IPS_ALERT;

    return IPS_BUSY;
}

bool CelestronSCT::startMove(uint32_t position)
{
    Aux::buffer data =
    {
        static_cast<uint8_t>((position >> 16) & 0xFF),
        static_cast<uint8_t>((position >> 8) & 0xFF),
        static_cast<uint8_t>(position & 0xFF)
    };

    LOGF_DEBUG("startMove %i, %x %x %x", position, data[0], data[1], data[2]);

    return communicator.commandBlind(PortFD, Aux::Target::FOCUSER, Aux::Command::MC_GOTO_FAST, data);
}

IPState CelestronSCT::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    return MoveAbsFocuser(newPosition);
}

void CelestronSCT::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    // Check position
    double lastPosition = FocusAbsPosN[0].value;
    bool rc = readPosition();
    if (rc)
    {
        // Only update if there is actual change
        if (fabs(lastPosition - FocusAbsPosN[0].value) > 1)
            IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
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
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    if (calibrateInProgress)
    {
        usleep(500000);     // slowing things down while calibrating seems to help
        // check the calibration state
        Aux::buffer reply;
        communicator.sendCommand(PortFD, Aux::Target::FOCUSER, Aux::Command::FOC_CALIB_DONE, reply);
        bool complete = reply[0] > 0;
        int state = reply[1];

        if (complete || state == 0)
        {
            // a completed calibration returns complete as true, an aborted calibration sets the status to zero

            const char *msg = complete ? "Calibrate complete" : "Calibrate aborted";
            LOG_INFO(msg);
            calibrateInProgress = false;
            CalibrateS[1].s = ISS_OFF;
            CalibrateSP.s = IPS_OK;
            //CalibrateStateT[0].text = (char *)msg;
            IUSaveText(&CalibrateStateT[0], msg);
            IDSetSwitch(&CalibrateSP, nullptr);
            IDSetText(&CalibrateStateTP, nullptr);
            // read the new limits
            if (complete && readLimits())
            {
                IUUpdateMinMax(&FocusAbsPosNP);
                IDSetNumber(&FocusMaxPosNP, nullptr);
                IDSetNumber(&FocusMinPosNP, nullptr);
            }
        }
        else
        {
            if (state != calibrateState)
            {
                calibrateState = state;
                char str[20];
                snprintf(str, 20, "Calibrate state %i", state);
                CalibrateStateT[0].text = str;
                IDSetText(&CalibrateStateTP, nullptr);
            }
        }
    }

    SetTimer(POLLMS);
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

