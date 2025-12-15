/*******************************************************************************
 Rigel Systems Dome INDI Driver

 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 Based on Protocol extracted from https://github.com/rpineau/RigelDome

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

#include "rigel_dome.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <regex>
#include <memory>
#include <termios.h>

// We declare an auto pointer to RigelDome.
static std::unique_ptr<RigelDome> rigelDome(new RigelDome());

#define DOME_CMD          9    /* Dome command in bytes */
#define DOME_BUF          16   /* Dome command buffer */
#define DOME_TIMEOUT      3    /* 3 seconds comm timeout */

#define SIM_SHUTTER_TIMER 5.0 /* Simulated Shutter closes/open in 5 seconds */
#define SIM_FLAP_TIMER    5.0 /* Simulated Flap closes/open in 3 seconds */
#define SIM_DOME_HI_SPEED 5.0 /* Simulated dome speed 5.0 degrees per second, constant */
#define SIM_DOME_LO_SPEED 0.5 /* Simulated dome speed 0.5 degrees per second, constant */

RigelDome::RigelDome()
{
    setVersion(1, 0);

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_REL_MOVE |
                      DOME_CAN_PARK |
                      DOME_CAN_SYNC);
}

bool RigelDome::initProperties()
{
    INDI::Dome::initProperties();

    OperationSP[OPERATION_FIND_HOME].fill("OPERATION_FIND_HOME", "Find Home", ISS_OFF);
    OperationSP[OPERATION_CALIBRATE].fill("OPERATION_CALIBRATE", "Calibrate", ISS_OFF);
    OperationSP.fill(getDeviceName(), "OPERATION", "Operation", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    InfoTP[INFO_FIRMWARE].fill("FIRMWARE", "Version", "NA");
    InfoTP[INFO_MODEL].fill("MODEL", "Model", "NA");
    InfoTP[INFO_TICKS].fill("TICKS_PER_REV", "Ticks/Rev", "NA");
    InfoTP[INFO_BATTERY].fill("BATTERY", "Battery", "NA");
    InfoTP.fill(getDeviceName(), "FIRMWARE_INFO", "Info", INFO_TAB, IP_RO, 60, IPS_IDLE);

    HomePositionNP[AXIS_AZ].fill("HOME_AZ", "AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    HomePositionNP.fill(getDeviceName(), "DOME_HOME_POSITION", "Home Position", SITE_TAB, IP_RW, 60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    SetParkDataType(PARK_AZ);
    addAuxControls();

    // Initialize Pulsar Dome Drive workaround variables
    previousAngle = -1;
    stuckAngleCounter = 0;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RigelDome::getStartupValues()
{
    targetAz = 0;

    InfoTP.setState((readFirmware() && readModel() && readStepsPerRevolution()) ? IPS_OK : IPS_ALERT);
    if (HasShutter())
        readBatteryLevels();
    InfoTP.apply();

    if (readPosition())
        DomeAbsPosNP.apply();

    if (readShutterStatus())
        DomeShutterSP.apply();

    if (readHomePosition())
        HomePositionNP.apply();

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RigelDome::Handshake()
{
    char res[DRIVER_LEN] = {0};
    sendCommand("PULSAR", res); // send dummy command to flush serial line

    bool rc = readShutterStatus();
    if (rc)
    {
        if (m_rawShutterState != S_NotFitted)
            SetDomeCapability(GetDomeCapability() | DOME_HAS_SHUTTER);
    }

    return rc;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *RigelDome::getDefaultName()
{
    return "Rigel Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RigelDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(OperationSP);
        defineProperty(InfoTP);
        defineProperty(HomePositionNP);

        getStartupValues();
    }
    else
    {
        deleteProperty(OperationSP);
        deleteProperty(InfoTP);
        deleteProperty(HomePositionNP);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RigelDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (OperationSP.isNameMatch(name))
        {
            const char *requestedOperation = IUFindOnSwitchName(states, names, n);
            if (OperationSP[OPERATION_FIND_HOME].isNameMatch(requestedOperation))
            {
                if (home())
                {
                    OperationSP.reset();
                    OperationSP[OPERATION_FIND_HOME].s = ISS_ON;
                    OperationSP.setState(IPS_BUSY);
                    LOG_INFO("Dome is moving to home position...");
                    setDomeState(DOME_MOVING);
                }
                else
                {
                    OperationSP.setState(IPS_ALERT);
                }
            }
            else if (OperationSP[OPERATION_CALIBRATE].isNameMatch(requestedOperation))
            {
                if (calibrate())
                {
                    OperationSP.reset();
                    OperationSP[OPERATION_CALIBRATE].s = ISS_ON;
                    OperationSP.setState(IPS_BUSY);
                    LOG_INFO("Dome is calibrating...");
                    setDomeState(DOME_MOVING);
                }
                else
                {
                    OperationSP.setState(IPS_ALERT);
                }
            }

            OperationSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RigelDome::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (HomePositionNP.isNameMatch(name))
        {
            HomePositionNP.update(values, names, n);
            HomePositionNP.setState(IPS_OK);
            auto homeAz = HomePositionNP[AXIS_AZ].getValue();
            setHome(homeAz);
            LOGF_INFO("Setting home position to %3.1f", homeAz);
            HomePositionNP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
void RigelDome::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    readState();

    bool isMoving = (m_rawMotorState != M_Idle && m_rawMotorState != M_MovingAtSideral);

    /*
        Pulsar Dome Drive workaround to fix issue where the motor seems to
        be struggling as it tries to get to the final position.
        This improved version tracks consecutive cycles with no angle change
        and only stops the motor after multiple cycles if we're close to target.
    */
    double currentAngle = DomeAbsPosNP[0].getValue();

    if (previousAngle >= 0 && isMoving)
    {
        // Check if angle has changed (with small tolerance for floating point comparison)
        if (std::abs(currentAngle - previousAngle) < 0.01)
        {
            stuckAngleCounter++;
            LOGF_DEBUG("Angle unchanged for %d consecutive cycle(s): %.3f degrees", stuckAngleCounter, currentAngle);

            // Only stop if stuck for multiple cycles AND close to target
            if (stuckAngleCounter >= STUCK_THRESHOLD)
            {
                double diff = std::abs(targetAz - currentAngle);
                // Account for wrap-around (e.g., target 1, current 359)
                if (diff > 180)
                    diff = 360.0 - diff;

                if (diff < ANGLE_TOLERANCE)
                {
                    char res[DRIVER_LEN] = {0};
                    if (sendCommand("STOP", res))
                    {
                        LOGF_INFO("Motor stopped: position unchanged for %d cycles, within %.1f° of target",
                                  STUCK_THRESHOLD, ANGLE_TOLERANCE);
                        stuckAngleCounter = 0;
                        setDomeState(DOME_SYNCED);
                        SetTimer(getCurrentPollingPeriod());
                        return;
                    }
                }
                else
                {
                    LOGF_WARN("Motor stuck but %.1f° away from target - allowing more time", diff);
                }
            }
        }
        else
        {
            // Angle changed, reset counter
            stuckAngleCounter = 0;
        }
    }

    previousAngle = currentAngle;
    /* END Pulsar Dome Drive workaround */

    if (DomeAbsPosNP.getState() == IPS_BUSY && isMoving == false)
    {
        bool isHoming = (OperationSP.getState() == IPS_BUSY && OperationSP[OPERATION_FIND_HOME].getState() == ISS_ON);
        bool isCalibrating = (OperationSP.getState() == IPS_BUSY && OperationSP[OPERATION_CALIBRATE].getState() == ISS_ON);

        if (isHoming)
        {
            LOG_INFO("Dome completed homing...");
            OperationSP.reset();
            OperationSP.setState(IPS_OK);
            OperationSP.apply();
            setDomeState(DOME_SYNCED);
        }
        else if (isCalibrating)
        {
            LOG_INFO("Dome completed calibration...");
            OperationSP.reset();
            OperationSP.setState(IPS_OK);
            OperationSP.apply();
            setDomeState(DOME_SYNCED);
        }
        else if (getDomeState() == DOME_PARKING)
        {
            SetParked(true);
            LOG_INFO("Dome is parked.");
        }
        else
        {
            LOGF_INFO("Dome reached requested azimuth: %.3f Degrees", DomeAbsPosNP[0].getValue());
            setDomeState(DOME_SYNCED);
        }
    }
    else
    {
        if (DomeAbsPosNP.getState() != IPS_BUSY && isMoving)
            DomeAbsPosNP.setState(IPS_BUSY);

        DomeAbsPosNP.apply();
    }

    if (HasShutter())
    {
        ShutterState newShutterState = parseShutterState(m_rawShutterState);
        if (newShutterState != getShutterState())
        {
            setShutterState(newShutterState);
        }

        if (readBatteryLevels())
            InfoTP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::MoveAbs(double az)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, "GO %3.1f", az);
    if (sendCommand(cmd, res) == false)
        return IPS_ALERT;

    // Reset stuck counter for new movement
    stuckAngleCounter = 0;

    return (res[0] == 'A') ? IPS_BUSY : IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::MoveRel(double azDiff)
{
    targetAz = range360(DomeAbsPosNP[0].getValue() + azDiff);
    // It will take a few cycles to reach final position
    return MoveAbs(targetAz);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::Sync(double az)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, "ANGLE K %3.1f", az);
    if (sendCommand(cmd, res) == false)
        return false;

    return (res[0] == 'A');
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::Park()
{
    if (readShutterStatus() && ShutterParkPolicySP[SHUTTER_CLOSE_ON_PARK].getState() == ISS_ON)
    {
        if(ControlShutter(SHUTTER_CLOSE))
        {
            LOG_INFO("Shutter close on park");
        }
        else
            return IPS_ALERT;
    }

    targetAz = GetAxis1Park();
    if (setParkAz(targetAz))
    {
        // Reset stuck counter for parking movement
        stuckAngleCounter = 0;

        char res[DRIVER_LEN] = {0};
        if (sendCommand("GO P", res) == false)
            return IPS_ALERT;
        return (res[0] == 'A' ? IPS_BUSY : IPS_ALERT);
    }

    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::home()
{
    // Reset stuck counter for homing movement
    stuckAngleCounter = 0;

    char res[DRIVER_LEN] = {0};
    if (sendCommand("GO H", res) == false)
        return false;

    return (res[0] == 'A');
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::calibrate()
{
    // Reset stuck counter for calibration movement
    stuckAngleCounter = 0;

    char res[DRIVER_LEN] = {0};
    if (sendCommand("CALIBRATE", res) == false)
        return false;

    return (res[0] == 'A');
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::setHome(double az)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, "HOME %3.1f", az);
    if (sendCommand(cmd, res) == false)
        return false;

    return (res[0] == 'A');
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::UnPark()
{
    if (readShutterStatus() && ShutterParkPolicySP[SHUTTER_OPEN_ON_UNPARK].getState() == ISS_ON)
    {
        if(ControlShutter(SHUTTER_OPEN))
        {
            LOG_INFO("Shutter open on unpark");
        }
        else
            return IPS_ALERT;
    }

    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::ControlShutter(ShutterOperation operation)
{
    char res[DRIVER_LEN] = {0};

    if (operation == SHUTTER_OPEN)
    {
        m_TargetShutter = operation;
        sendCommand("OPEN", res);
    }
    else
    {
        m_TargetShutter = operation;
        sendCommand("CLOSE", res);
    }

    return (res[0] == 'A') ? IPS_BUSY : IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::Abort()
{
    if (sendCommand("STOP"))
    {
        // Reset stuck counter when aborting movement
        stuckAngleCounter = 0;

        if (OperationSP.getState() == IPS_BUSY)
        {
            LOGF_INFO("%s is aborted.", OperationSP[OPERATION_CALIBRATE].getState() == ISS_ON ? "Calibration" : "Finding home");
            OperationSP.reset();
            OperationSP.setState(IPS_ALERT);
            OperationSP.apply();
        }
        else if (getShutterState() == SHUTTER_MOVING)
        {
            DomeShutterSP.setState(IPS_ALERT);
            DomeShutterSP.apply();
            LOG_WARN("Shutter motion aborted!");
        }
        else
        {
            LOG_WARN("Dome motion aborted.");
        }

        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Connect/Disconnect shutter
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::setShutterConnected(bool enabled)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "BBOND %d", enabled ? 1 : 0);
    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////
/// Check if shutter is connected
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::isShutterConnected()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("BBOND", res) == false)
        return false;
    bool rc = (std::stoi(res) != 0);
    LOGF_DEBUG("Shutter is %s.", (rc ? "connected" : "disconnected"));
    return rc;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readStepsPerRevolution()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("ENCREV", res) == false)
        return false;

    InfoTP[INFO_TICKS].setText(res);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readBatteryLevels()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("BAT", res) == false)
        return false;

    int percent = 0;
    double volts = 0;

    if (sscanf(res, "%d %lf", &percent, &volts) != 2)
        return false;

    char levels[DRIVER_LEN] = {0};
    snprintf(levels, DRIVER_LEN, "%.2fv (%d%%)", volts / 1000.0, percent);
    InfoTP[INFO_BATTERY].setText(levels);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read position
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readPosition()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("ANGLE", res) == false)
        return false;

    DomeAbsPosNP[0].setValue(std::stod(res));
    return true;
}

bool RigelDome::readHomePosition()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("HOME", res) == false)
        return false;

    HomePositionNP[0].setValue(std::stod(res));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read position
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::setParkAz(double az)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, "PARK %3.1f", az);
    if (sendCommand(cmd, res) == false)
        return false;

    return res[0] == 'A';
}

/////////////////////////////////////////////////////////////////////////////
/// Read state
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readState()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("V", res) == false)
        return false;

    std::vector<std::string> fields = split(res, "\t");
    if (fields.size() < 13)
        return false;

    DomeAbsPosNP[0].setValue(std::stod(fields[0]));
    m_rawMotorState = static_cast<RigelMotorState>(std::stoi(fields[1]));
    m_rawShutterState = static_cast<RigelShutterState>(std::stoi(fields[5]));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Parse raw shutter state to INDI::Dome shutter state
/////////////////////////////////////////////////////////////////////////////
INDI::Dome::ShutterState RigelDome::parseShutterState(int state)
{
    switch (state)
    {
        case S_Open:
            return SHUTTER_OPENED;

        case S_Opening:
            return SHUTTER_MOVING;

        case S_Closed:
            return SHUTTER_CLOSED;

        case S_Closing:
            return SHUTTER_MOVING;

        case S_Error:
            return SHUTTER_ERROR;

        default:
            return SHUTTER_UNKNOWN;
    }
}
/////////////////////////////////////////////////////////////////////////////
/// Check if shutter is connected
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readShutterStatus()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("SHUTTER", res) == false)
        return false;

    m_rawShutterState = static_cast<RigelShutterState>(std::stoi(res));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Firmware
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readFirmware()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("VER", res) == false)
        return false;

    InfoTP[INFO_FIRMWARE].setText(res);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Model
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::readModel()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("PULSAR", res) == false)
        return false;

    InfoTP[INFO_MODEL].setText(res);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool RigelDome::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);

        char formatted_command[DRIVER_LEN] = {0};
        snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
        rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void RigelDome::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> RigelDome::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RigelDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    double target;

    if (operation == MOTION_START)
    {
        target = DomeAbsPosNP[0].getValue();
        if(dir == DOME_CW)
        {
            target += 5;
        }
        else
        {
            target -= 5;
        }

        if(target < 0)
            target += 360;
        if(target >= 360)
            target -= 360;
    }
    else
    {
        target = DomeAbsPosNP[0].getValue();
    }

    MoveAbs(target);

    return ((operation == MOTION_START) ? IPS_BUSY : IPS_OK);

}
