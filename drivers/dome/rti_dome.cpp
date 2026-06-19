/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 RTI Dome INDI Driver

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

#include "rti_dome.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>

static std::unique_ptr<RTIDome> rtidome(new RTIDome());

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
RTIDome::RTIDome()
{
    m_ShutterState = SHUTTER_UNKNOWN;

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_REL_MOVE |
                      DOME_CAN_PARK |
                      DOME_CAN_SYNC |
                      DOME_HAS_SHUTTER);

    // RTI dome supports both Serial and TCP connections (it has an Ethernet module)
    setDomeConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::initProperties()
{
    INDI::Dome::initProperties();

    // Firmware versions
    FirmwareTP[FIRMWARE_ROTATION].fill("FIRMWARE_ROTATION", "Rotation", "N/A");
    FirmwareTP[FIRMWARE_SHUTTER].fill("FIRMWARE_SHUTTER",  "Shutter",  "N/A");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Dome & Shutter status text
    StatusTP[STATUS_DOME].fill("STATUS_DOME",    "Dome",    "N/A");
    StatusTP[STATUS_SHUTTER].fill("STATUS_SHUTTER", "Shutter", "N/A");
    StatusTP.fill(getDeviceName(), "STATUS", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Motor settings
    SettingsNP[SETTINGS_STEPS_PER_ROTATION].fill("SETTINGS_STEPS_PER_ROTATION", "Steps/Rotation", "%.f",
            1, 0xFFFFFF, 1, 0);
    SettingsNP[SETTINGS_STEP_RATE].fill("SETTINGS_STEP_RATE", "Step Rate (steps/s)", "%.f",
                                        1, 50000, 1, 0);
    SettingsNP[SETTINGS_ACCELERATION].fill("SETTINGS_ACCELERATION", "Acceleration", "%.f",
                                           1, 50000, 1, 0);
    SettingsNP.fill(getDeviceName(), "SETTINGS", "Settings", SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Home position
    HomePositionNP[0].fill("HOME_POSITION", "Az (deg)", "%.2f", 0, 360, 1, 0);
    HomePositionNP.fill(getDeviceName(), "HOME_POSITION", "Home Position", SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Find Home
    HomeSP[HOME_FIND].fill("HOME_FIND", "Find", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Rain action
    RainActionSP[RAIN_DO_NOTHING].fill("RAIN_DO_NOTHING", "Do Nothing", ISS_ON);
    RainActionSP[RAIN_GOTO_HOME].fill("RAIN_GOTO_HOME",  "Go Home",    ISS_OFF);
    RainActionSP[RAIN_GOTO_PARK].fill("RAIN_GOTO_PARK",  "Go Park",    ISS_OFF);
    RainActionSP.fill(getDeviceName(), "RAIN_ACTION", "Rain Action", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Motor direction
    ReversedSP[REVERSED_NORMAL].fill("REVERSED_NORMAL",   "Normal",   ISS_ON);
    ReversedSP[REVERSED_REVERSED].fill("REVERSED_REVERSED", "Reversed", ISS_OFF);
    ReversedSP.fill(getDeviceName(), "MOTOR_REVERSED", "Motor Direction", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Shutter voltage
    ShutterVoltsNP[SHUTTER_VOLTAGE_CURRENT].fill("SHUTTER_VOLTAGE_CURRENT", "Current (V)", "%.2f", 0, 20, 0, 0);
    ShutterVoltsNP[SHUTTER_VOLTAGE_CUTOFF].fill("SHUTTER_VOLTAGE_CUTOFF",  "Cutoff (V)", "%.2f", 0, 20, 0, 0);
    ShutterVoltsNP.fill(getDeviceName(), "SHUTTER_VOLTAGE", "Shutter Voltage", INFO_TAB, IP_RW, 60, IPS_IDLE);

    // RTI dome USB serial is 115200 8N1 by default
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    // RTI dome defaults to 192.168.1.9:2323 when no DHCP server is available
    tcpConnection->setDefaultHost("192.168.1.9");
    tcpConnection->setDefaultPort(2323);

    // Default to TCP (network) connection since that's the most common usage
    setActiveConnection(tcpConnection);

    SetParkDataType(PARK_AZ);
    addAuxControls();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::setupInitialParameters()
{
    if (InitPark())
    {
        SetAxis1ParkDefault(0);
    }
    else
    {
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    // Get rotation firmware only (shutter firmware is fetched later, after
    // we confirm the shutter controller is reachable via H#/o#).
    if (getFirmwareVersion())
        FirmwareTP[FIRMWARE_ROTATION].setText(FirmwareTP[FIRMWARE_ROTATION].getText());

    FirmwareTP.setState(IPS_OK);
    FirmwareTP.apply();

    // Get current dome azimuth
    double az = 0;
    if (getDomeAzimuth(az))
    {
        DomeAbsPosNP[0].setValue(az);
        DomeAbsPosNP.apply();
    }

    // Get steps per rotation, step rate, acceleration
    uint32_t steps = 0, rate = 0, accel = 0;
    bool settingsOk = true;
    if (getStepsPerRotation(steps))
        SettingsNP[SETTINGS_STEPS_PER_ROTATION].setValue(steps);
    else
        settingsOk = false;
    if (getStepRate(rate))
        SettingsNP[SETTINGS_STEP_RATE].setValue(rate);
    else
        settingsOk = false;
    if (getAcceleration(accel))
        SettingsNP[SETTINGS_ACCELERATION].setValue(accel);
    else
        settingsOk = false;

    SettingsNP.setState(settingsOk ? IPS_OK : IPS_ALERT);
    SettingsNP.apply();

    // Get home position
    double homeAz = 0;
    if (getHomePosition(homeAz))
    {
        HomePositionNP[0].setValue(homeAz);
        HomePositionNP.setState(IPS_OK);
    }
    HomePositionNP.apply();

    // Get motor reversed status
    bool rev = false;
    if (getMotorReversed(rev))
    {
        ReversedSP.reset();
        ReversedSP[rev ? REVERSED_REVERSED : REVERSED_NORMAL].setState(ISS_ON);
        ReversedSP.setState(IPS_OK);
        ReversedSP.apply();
    }

    // Get rain action
    int action = 0;
    if (getRainAction(action))
    {
        RainActionSP.reset();
        if (action >= 0 && action <= 2)
            RainActionSP[action].setState(ISS_ON);
        RainActionSP.setState(IPS_OK);
        RainActionSP.apply();
    }

    // Ping the shutter XBee radio, wait briefly, then check if it responded.
    // If the shutter controller is absent (no XBee link), m_bShutterPresent stays
    // false and we will never try to send M# — avoiding constant 3-second timeouts.
    sendShutterHello();
    usleep(250000); // 250 ms — same delay as the X2 plugin
    getShutterPresent(m_bShutterPresent);
    if (m_bShutterPresent)
    {
        LOG_INFO("Shutter controller detected.");
        // Fetch shutter firmware and initial state only when actually present
        getShutterFirmwareVersion();
        FirmwareTP.apply();

        int shutterStatus = 0;
        if (queryShutterState(shutterStatus))
        {
            // Map raw state to INDI ShutterState
            ShutterState ss = SHUTTER_UNKNOWN;
            switch (shutterStatus)
            {
                case 0:
                    ss = SHUTTER_OPENED;
                    break;
                case 1:
                    ss = SHUTTER_CLOSED;
                    break;
                case 2:
                case 3:
                    ss = SHUTTER_MOVING;
                    break;
                default:
                    ss = SHUTTER_UNKNOWN;
                    break;
            }
            setShutterState(ss);
        }

        // Get shutter voltage
        double voltage = 0, cutoff = 0;
        if (getShutterVoltage(voltage, cutoff))
        {
            ShutterVoltsNP[SHUTTER_VOLTAGE_CURRENT].setValue(voltage);
            ShutterVoltsNP[SHUTTER_VOLTAGE_CUTOFF].setValue(cutoff);
            ShutterVoltsNP.setState(IPS_OK);
            ShutterVoltsNP.apply();
        }
    }
    else
    {
        LOG_INFO("No shutter controller detected. Shutter commands will be skipped.");
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::Handshake()
{
    return getFirmwareVersion();
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
const char *RTIDome::getDefaultName()
{
    return "RTI Dome";
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        setupInitialParameters();

        defineProperty(FirmwareTP);
        defineProperty(StatusTP);
        defineProperty(HomeSP);
        defineProperty(HomePositionNP);
        defineProperty(SettingsNP);
        defineProperty(RainActionSP);
        defineProperty(ReversedSP);
        defineProperty(ShutterVoltsNP);
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(StatusTP);
        deleteProperty(HomeSP);
        deleteProperty(HomePositionNP);
        deleteProperty(SettingsNP);
        deleteProperty(RainActionSP);
        deleteProperty(ReversedSP);
        deleteProperty(ShutterVoltsNP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ////////////////////////////////////////////////////////////
        // Home
        ////////////////////////////////////////////////////////////
        if (HomeSP.isNameMatch(name))
        {
            HomeSP.update(states, names, n);
            if (HomeSP[HOME_FIND].getState() == ISS_ON)
            {
                if (findHome())
                {
                    HomeSP.setState(IPS_BUSY);
                    LOG_INFO("Dome finding home position...");
                }
                else
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_ALERT);
                    LOG_ERROR("Failed to start home search.");
                }
            }
            HomeSP.apply();
            return true;
        }

        ////////////////////////////////////////////////////////////
        // Rain Action
        ////////////////////////////////////////////////////////////
        if (RainActionSP.isNameMatch(name))
        {
            RainActionSP.update(states, names, n);
            int action = RainActionSP.findOnSwitchIndex();
            if (setRainAction(action))
            {
                RainActionSP.setState(IPS_OK);
                LOGF_INFO("Rain action set to: %s", RainActionSP.findOnSwitch()->getLabel());
            }
            else
            {
                RainActionSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to set rain action.");
            }
            RainActionSP.apply();
            return true;
        }

        ////////////////////////////////////////////////////////////
        // Motor Direction
        ////////////////////////////////////////////////////////////
        if (ReversedSP.isNameMatch(name))
        {
            ReversedSP.update(states, names, n);
            bool rev = (ReversedSP.findOnSwitchIndex() == REVERSED_REVERSED);
            if (setMotorReversed(rev))
            {
                ReversedSP.setState(IPS_OK);
                LOGF_INFO("Motor direction set to: %s", rev ? "Reversed" : "Normal");
            }
            else
            {
                ReversedSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to set motor direction.");
            }
            ReversedSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ////////////////////////////////////////////////////////////
        // Settings
        ////////////////////////////////////////////////////////////
        if (SettingsNP.isNameMatch(name))
        {
            bool allSet = true;
            for (int i = 0; i < n; i++)
            {
                if (SettingsNP[SETTINGS_STEPS_PER_ROTATION].isNameMatch(names[i]))
                {
                    if (setStepsPerRotation(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_STEPS_PER_ROTATION].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set steps per rotation.");
                    }
                }
                else if (SettingsNP[SETTINGS_STEP_RATE].isNameMatch(names[i]))
                {
                    if (setStepRate(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_STEP_RATE].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set step rate.");
                    }
                }
                else if (SettingsNP[SETTINGS_ACCELERATION].isNameMatch(names[i]))
                {
                    if (setAcceleration(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_ACCELERATION].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set acceleration.");
                    }
                }
            }
            SettingsNP.setState(allSet ? IPS_OK : IPS_ALERT);
            SettingsNP.apply();
            return true;
        }

        ////////////////////////////////////////////////////////////
        // Home Position
        ////////////////////////////////////////////////////////////
        if (HomePositionNP.isNameMatch(name))
        {
            if (setHomePosition(values[0]))
            {
                HomePositionNP[0].setValue(values[0]);
                HomePositionNP.setState(IPS_OK);
                LOGF_INFO("Home position set to %.2f degrees.", values[0]);
            }
            else
            {
                HomePositionNP.setState(IPS_ALERT);
                LOG_ERROR("Failed to set home position.");
            }
            HomePositionNP.apply();
            return true;
        }

        ////////////////////////////////////////////////////////////
        // Shutter Voltage Cutoff
        ////////////////////////////////////////////////////////////
        if (ShutterVoltsNP.isNameMatch(name))
        {
            // Only the cutoff value is settable; current is read-only
            double cutoff = values[SHUTTER_VOLTAGE_CUTOFF];
            char cmd[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN, "K%d", static_cast<int>(cutoff * 100));
            char res[DRIVER_LEN] = {0};
            if (sendCommand(cmd, res))
            {
                // Response is KVVVV,vvvv — re-parse
                double voltage = 0, newCutoff = 0;
                if (getShutterVoltage(voltage, newCutoff))
                {
                    ShutterVoltsNP[SHUTTER_VOLTAGE_CURRENT].setValue(voltage);
                    ShutterVoltsNP[SHUTTER_VOLTAGE_CUTOFF].setValue(newCutoff);
                }
                ShutterVoltsNP.setState(IPS_OK);
            }
            else
            {
                ShutterVoltsNP.setState(IPS_ALERT);
                LOG_ERROR("Failed to set shutter voltage cutoff.");
            }
            ShutterVoltsNP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
void RTIDome::TimerHit()
{
    if (!isConnected())
        return;

    // Poll dome azimuth
    double az = 0;
    if (getDomeAzimuth(az))
    {
        if (std::abs(az - DomeAbsPosNP[0].getValue()) > DOME_AZ_THRESHOLD)
        {
            DomeAbsPosNP[0].setValue(az);
            DomeAbsPosNP.apply();
        }
    }

    // Poll slewing status
    int slewStatus = 0;
    if (getSlewingStatus(slewStatus))
    {
        const char *slewText = (slewStatus == 1) ? "Slewing CW" : (slewStatus == -1) ? "Slewing CCW" : "Idle";

        if (strcmp(StatusTP[STATUS_DOME].getText(), slewText) != 0)
        {
            StatusTP[STATUS_DOME].setText(slewText);
            StatusTP.setState(IPS_OK);
            StatusTP.apply();
        }

        // If dome was moving / parking and is now idle
        if (slewStatus == 0)
        {
            if (getDomeState() == DOME_PARKING)
            {
                SetParked(true);
                LOG_INFO("Dome parked.");
            }
            else if (getDomeState() == DOME_MOVING)
            {
                setDomeState(DOME_IDLE);
            }

            // If home search was in progress
            if (HomeSP.getState() == IPS_BUSY)
            {
                int homedStatus = 0;
                if (getHomedStatus(homedStatus) && homedStatus >= 1)
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                    LOG_INFO("Dome found home position.");
                }
            }
        }
    }

    // Periodically re-check shutter presence (every ~10 ticks ≈ 10 s at 1 s poll)
    // This handles the case where the shutter XBee link comes up after the driver
    // was already connected, or drops out during a session.
    ++m_nShutterPollCounter;
    if (m_nShutterPollCounter >= 10)
    {
        m_nShutterPollCounter = 0;
        bool wasPresent = m_bShutterPresent;
        sendShutterHello();
        getShutterPresent(m_bShutterPresent);
        if (m_bShutterPresent && !wasPresent)
            LOG_INFO("Shutter controller detected.");
        else if (!m_bShutterPresent && wasPresent)
            LOG_WARN("Shutter controller lost.");
    }

    // Only poll shutter state when the shutter controller is reachable.
    // Without this guard, every M# would block for DRIVER_TIMEOUT seconds
    // when the XBee link is absent, causing visible lag in the timer loop.
    if (m_bShutterPresent)
    {
        int shutterStatus = 0;
        if (queryShutterState(shutterStatus))
        {
            // Full 11-state mapping from the RTI firmware enum:
            //   0=OPEN, 1=CLOSED, 2=OPENING, 3=CLOSING,
            //   4=BOTTOM_OPEN, 5=BOTTOM_CLOSED, 6=BOTTOM_OPENING, 7=BOTTOM_CLOSING,
            //   8=SHUTTER_ERROR, 9=FINISHING_OPEN, 10=FINISHING_CLOSE
            const char *shutterText = nullptr;
            ShutterState newShutterState = getShutterState();

            switch (shutterStatus)
            {
                case 0:
                    shutterText     = "Open";
                    newShutterState = SHUTTER_OPENED;
                    break;
                case 1:
                    shutterText     = "Closed";
                    newShutterState = SHUTTER_CLOSED;
                    break;
                case 2:
                case 6:
                case 9:
                    shutterText     = "Opening";
                    newShutterState = SHUTTER_MOVING;
                    break;
                case 3:
                case 7:
                case 10:
                    shutterText     = "Closing";
                    newShutterState = SHUTTER_MOVING;
                    break;
                case 4:
                    shutterText     = "Bottom Open";
                    newShutterState = SHUTTER_OPENED;
                    break;
                case 5:
                    shutterText     = "Bottom Closed";
                    newShutterState = SHUTTER_CLOSED;
                    break;
                case 8:
                    shutterText     = "Error";
                    newShutterState = SHUTTER_UNKNOWN;
                    break;
                default:
                    shutterText     = "Unknown";
                    newShutterState = SHUTTER_UNKNOWN;
                    break;
            }

            if (strcmp(StatusTP[STATUS_SHUTTER].getText(), shutterText) != 0)
            {
                StatusTP[STATUS_SHUTTER].setText(shutterText);
                StatusTP.setState(IPS_OK);
                StatusTP.apply();
            }

            if (newShutterState != getShutterState())
                setShutterState(newShutterState);
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState RTIDome::MoveAbs(double az)
{
    if (gotoAzimuth(az))
        return IPS_BUSY;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState RTIDome::MoveRel(double azDiff)
{
    m_TargetAz = DomeAbsPosNP[0].getValue() + azDiff;

    if (m_TargetAz < DomeAbsPosNP[0].getMin())
        m_TargetAz += DomeAbsPosNP[0].getMax();
    if (m_TargetAz > DomeAbsPosNP[0].getMax())
        m_TargetAz -= DomeAbsPosNP[0].getMax();

    return MoveAbs(m_TargetAz);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::Sync(double az)
{
    return syncAzimuth(az);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState RTIDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_STOP)
    {
        if (stopDome())
            return IPS_OK;
    }
    else
    {
        // For manual jog: move a small fixed amount in the requested direction
        double jogDeg = (dir == DOME_CW) ? 5.0 : -5.0;
        return MoveRel(jogDeg);
    }
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState RTIDome::Park()
{
    m_TargetAz = GetAxis1Park();
    return MoveAbs(m_TargetAz);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState RTIDome::UnPark()
{
    SetParked(false);
    return IPS_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState RTIDome::ControlShutter(ShutterOperation operation)
{
    // Refuse immediately if the shutter XBee link is not established
    if (!m_bShutterPresent)
    {
        LOG_ERROR("Cannot control shutter: shutter controller not detected.");
        return IPS_ALERT;
    }

    if (operation == SHUTTER_OPEN)
    {
        if (openShutter())
            return IPS_BUSY;
    }
    else if (operation == SHUTTER_CLOSE)
    {
        if (closeShutter())
            return IPS_BUSY;
    }
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::Abort()
{
    if (!stopDome())
        return false;

    if (getShutterState() == SHUTTER_MOVING)
        setShutterState(SHUTTER_UNKNOWN);

    if (ParkSP.getState() == IPS_BUSY)
        SetParked(false);

    if (HomeSP.getState() == IPS_BUSY)
    {
        HomeSP.reset();
        HomeSP.setState(IPS_IDLE);
        HomeSP.apply();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool RTIDome::SetDefaultPark()
{
    SetAxis1Park(0);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get rotation firmware version
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getFirmwareVersion()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("v", res))
        return false;

    // Response: v2.645#  — strip the leading 'v'
    if (res[0] == 'v' || res[0] == 'V')
    {
        FirmwareTP[FIRMWARE_ROTATION].setText(res + 1);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Get shutter firmware version
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getShutterFirmwareVersion()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("V", res))
        return false;

    // Response: V2.645#  — strip the leading 'V'
    if (res[0] == 'v' || res[0] == 'V')
    {
        FirmwareTP[FIRMWARE_SHUTTER].setText(res + 1);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Get dome azimuth
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getDomeAzimuth(double &az)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("g", res))
        return false;

    // Response: g321.50#  — strip the leading 'g'
    if (res[0] != 'g')
        return false;

    az = std::atof(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get slewing status: -1=CCW, 0=idle, 1=CW
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getSlewingStatus(int &status)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("m", res))
        return false;

    // Response: m0#, m1#, m-1#
    if (res[0] != 'm')
        return false;

    status = std::atoi(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get homed status: 0=not homed, 1=homed, 2=at home
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getHomedStatus(int &status)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("z", res))
        return false;

    // Response: z0#
    if (res[0] != 'z')
        return false;

    status = std::atoi(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get shutter state
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::queryShutterState(int &status)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("M", res))
        return false;

    // Response: M0#
    if (res[0] != 'M')
        return false;

    status = std::atoi(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get steps per rotation
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getStepsPerRotation(uint32_t &steps)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("t", res))
        return false;

    // Response: t440640#
    if (res[0] != 't')
        return false;

    steps = static_cast<uint32_t>(std::stoul(res + 1));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get step rate
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getStepRate(uint32_t &rate)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("r", res))
        return false;

    // Response: r8000#
    if (res[0] != 'r')
        return false;

    rate = static_cast<uint32_t>(std::stoul(res + 1));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get acceleration
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getAcceleration(uint32_t &accel)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("e", res))
        return false;

    // Response: e7000#
    if (res[0] != 'e')
        return false;

    accel = static_cast<uint32_t>(std::stoul(res + 1));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get home position in degrees
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getHomePosition(double &az)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("i", res))
        return false;

    // Response: i180.00#
    if (res[0] != 'i')
        return false;

    az = std::atof(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get park position in degrees
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getParkPosition(double &az)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("l", res))
        return false;

    // Response: l321.50#
    if (res[0] != 'l')
        return false;

    az = std::atof(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get motor reversed status
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getMotorReversed(bool &reversed)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("y", res))
        return false;

    // Response: y0# or y1#
    if (res[0] != 'y')
        return false;

    reversed = (res[1] == '1');
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get rain action
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getRainAction(int &action)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("n", res))
        return false;

    // Response: n2#
    if (res[0] != 'n')
        return false;

    action = std::atoi(res + 1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get rain sensor status
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getRainStatus(bool &raining)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("F", res))
        return false;

    // Response: F0# or F1#
    if (res[0] != 'F')
        return false;

    raining = (res[1] == '1');
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get shutter battery voltage and cutoff
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getShutterVoltage(double &voltage, double &cutoff)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("K", res))
        return false;

    // Response: K1319,1150#  — VVVV,vvvv; divide by 100 for float
    if (res[0] != 'K')
        return false;

    int raw_v = 0, raw_c = 0;
    if (sscanf(res + 1, "%d,%d", &raw_v, &raw_c) != 2)
        return false;

    voltage = raw_v / 100.0;
    cutoff  = raw_c / 100.0;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Go to azimuth
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::gotoAzimuth(double az)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "g%.2f", az);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'g');
}

/////////////////////////////////////////////////////////////////////////////
/// Sync to azimuth
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::syncAzimuth(double az)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "s%.2f", az);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 's');
}

/////////////////////////////////////////////////////////////////////////////
/// Stop dome
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::stopDome()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("a", res))
        return false;
    return (res[0] == 'a');
}

/////////////////////////////////////////////////////////////////////////////
/// Find home
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::findHome()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("h", res))
        return false;
    return (res[0] == 'h');
}

/////////////////////////////////////////////////////////////////////////////
/// Set home position
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setHomePosition(double az)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "i%.2f", az);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'i');
}

/////////////////////////////////////////////////////////////////////////////
/// Set park position
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setParkPosition(double az)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "l%.2f", az);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'l');
}

/////////////////////////////////////////////////////////////////////////////
/// Set steps per rotation
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setStepsPerRotation(uint32_t steps)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "t%u", steps);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 't');
}

/////////////////////////////////////////////////////////////////////////////
/// Set step rate
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setStepRate(uint32_t rate)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "r%u", rate);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'r');
}

/////////////////////////////////////////////////////////////////////////////
/// Set acceleration
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setAcceleration(uint32_t accel)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "e%u", accel);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'e');
}

/////////////////////////////////////////////////////////////////////////////
/// Set motor direction reversed
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setMotorReversed(bool reversed)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "y%d", reversed ? 1 : 0);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'y');
}

/////////////////////////////////////////////////////////////////////////////
/// Set rain action (0=nothing, 1=home, 2=park)
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::setRainAction(int action)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "n%d", action);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    return (res[0] == 'n');
}

/////////////////////////////////////////////////////////////////////////////
/// Open shutter
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::openShutter()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("O", res))
        return false;
    // Response: O# normally; OL# if low voltage, OR# if raining
    if (res[0] != 'O')
        return false;
    if (strlen(res) > 1 && res[1] == 'L')
    {
        LOG_WARN("Shutter refused to open: low voltage.");
        return false;
    }
    if (strlen(res) > 1 && res[1] == 'R')
    {
        LOG_WARN("Shutter refused to open: rain detected.");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Close shutter
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::closeShutter()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("C", res))
        return false;
    return (res[0] == 'C');
}

/////////////////////////////////////////////////////////////////////////////
/// Send shutter hello (H#)
/// Broadcasts a ping over the rotation controller's XBee radio to wake up
/// the shutter controller. The rotation controller echoes H# back to the
/// client (response: H#). We MUST read this response; leaving it unread in
/// the RX buffer causes the very next command (o# / getShutterPresent) to
/// consume the stale 'H' byte instead of its own response, producing a
/// cascade of mis-matched read results and eventually serial timeout errors.
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::sendShutterHello()
{
    char res[DRIVER_LEN] = {0};
    return sendCommand("H", res);
}

/////////////////////////////////////////////////////////////////////////////
/// Check whether the shutter controller is reachable (o#)
/// The rotation controller tracks whether the shutter controller has
/// responded to XBee pings. Response: o0# = not present, o1# = present.
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::getShutterPresent(bool &present)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("o", res))
    {
        present = false;
        return false;
    }

    // Response: o0# or o1#
    if (res[0] != 'o')
    {
        present = false;
        return false;
    }

    present = (res[1] == '1');
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool RTIDome::sendCommand(const char *cmd, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    // Build full command string: cmd + '#'
    char formatted_command[DRIVER_LEN] = {0};
    snprintf(formatted_command, DRIVER_LEN, "%s#", cmd);

    LOGF_DEBUG("CMD <%s>", formatted_command);

    rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    // Read until '#' stop character
    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Strip trailing '#'
    if (nbytes_read > 0 && res[nbytes_read - 1] == DRIVER_STOP_CHAR)
        res[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void RTIDome::hexDump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
