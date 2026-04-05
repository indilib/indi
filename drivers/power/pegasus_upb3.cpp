/*******************************************************************************
  Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box V3 Driver.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "pegasus_upb3.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>
#include <math.h>
#include <iomanip>

static constexpr const int PEGASUS_LEN = 128;

// We declare an auto pointer to PegasusUPB3.
static std::unique_ptr<PegasusUPB3> upb3(new PegasusUPB3());

PegasusUPB3::PegasusUPB3() : INDI::DefaultDevice(),
    INDI::FocuserInterface(this),
    INDI::WeatherInterface(this),
    INDI::PowerInterface(this),
    INDI::OutputInterface(this)
{
    setVersion(1, 0);

    lastSensorData.reserve(25);
    lastPowerData.reserve(5);
    lastStepperData.reserve(6);
    lastAutoDewData.reserve(4);
}

const char * PegasusUPB3::getDefaultName()
{
    return "Pegasus UPB3";
}

//////////////////////////////////////////////////////////////////////
/// Handshake
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::Handshake()
{
    char response[PEGASUS_LEN] = {0};
    int nbytes_read = 0;
    PortFD = serialConnection->getPortFD();

    LOG_DEBUG("CMD <P#>");

    if (isSimulation())
    {
        snprintf(response, PEGASUS_LEN, "UPBv3:1.0");
        nbytes_read = 10;
    }
    else
    {
        int tty_rc = 0, nbytes_written = 0;
        char command[PEGASUS_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        strncpy(command, "P#\n", PEGASUS_LEN);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errorMessage);
            return false;
        }

        // Try first with stopChar as the stop character
        if ( (tty_rc = tty_nread_section(PortFD, response, PEGASUS_LEN, stopChar, 1, &nbytes_read)) != TTY_OK)
        {
            // Try 0xA as the stop character
            if (tty_rc == TTY_OVERFLOW || tty_rc == TTY_TIME_OUT)
            {
                tcflush(PortFD, TCIOFLUSH);
                tty_write_string(PortFD, command, &nbytes_written);
                stopChar = 0xA;
                tty_rc = tty_nread_section(PortFD, response, PEGASUS_LEN, stopChar, 1, &nbytes_read);
            }

            if (tty_rc != TTY_OK)
            {
                char errorMessage[MAXRBUF];
                tty_error_msg(tty_rc, errorMessage, MAXRBUF);
                LOGF_ERROR("Serial read error: %s", errorMessage);
                return false;
            }
        }

        cleanupResponse(response);
        tcflush(PortFD, TCIOFLUSH);
    }

    LOGF_DEBUG("RES <%s>", response);

    setupComplete = false;

    // Check for UPBv3 identification
    if (strstr(response, "UPBv3") == nullptr)
    {
        LOG_ERROR("Device not recognized as UPBv3. Please check connection.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Control Methods
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::setPowerEnabled(uint8_t port, uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "P%d:%d", port, value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setPowerLEDEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setBuckVoltage(uint8_t voltage)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PJ:%d", voltage);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setBoostVoltage(uint8_t voltage)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PB:%d", voltage);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setBuckEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PJ:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setBoostEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PB:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setAutoDewEnabled(uint8_t port, bool enabled)
{
    // Per-port autodew: ADW command takes 3 boolean values
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};

    // Get current states
    bool dew1 = (PI::AutoDewSP.size() > 0 && PI::AutoDewSP[0].getState() == ISS_ON);
    bool dew2 = (PI::AutoDewSP.size() > 1 && PI::AutoDewSP[1].getState() == ISS_ON);
    bool dew3 = (PI::AutoDewSP.size() > 2 && PI::AutoDewSP[2].getState() == ISS_ON);

    // Update the specific port
    if (port == 1) dew1 = enabled;
    else if (port == 2) dew2 = enabled;
    else if (port == 3) dew3 = enabled;

    snprintf(cmd, PEGASUS_LEN, "ADW:%d:%d:%d", dew1 ? 1 : 0, dew2 ? 1 : 0, dew3 ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setAutoDewAgg(uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "DA:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setDewPWM(uint8_t id, uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "D%d:%d", id, value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setUSBPortEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "U%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setRelay(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "RL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setFocuserMaxSpeed(uint16_t maxSpeed)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SS:%d", maxSpeed);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setFocuserMicrostepping(uint8_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "MSTEP:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setFocuserCurrentLimit(uint16_t value)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "MCUR:%d", value);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setFocuserHoldTorque(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "MHLD:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::setPowerOnBoot()
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PS");  // Store current power port states
    if (sendCommand(cmd, res))
    {
        return true;
    }
    return false;
}

bool PegasusUPB3::getPowerOnBoot()
{
    // UPBv3 stores power states, we'll read them during getSensorData
    return true;
}

//////////////////////////////////////////////////////////////////////
/// Setup & Data Methods
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::setupParams()
{
    sendFirmware();
    return true;
}

bool PegasusUPB3::sendFirmware()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PV", res))
    {
        LOGF_INFO("Detected firmware %s", res);
        FirmwareTP[FIRMWARE_VERSION].setText(res);
        FirmwareTP.apply();
        return true;
    }
    return false;
}

bool PegasusUPB3::getSensorData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PA", res))
    {
        std::vector<std::string> result = split(res, ":");
        // PA response format: PA:P1:P2:P3:P4:P5:P6:D1:D2:D3:Buck:Boost:Relay
        // Total: 13 fields (PA + 12 values)
        if (result.size() < 13)
        {
            LOGF_WARN("Received wrong number (%i) of sensor data (%s). Expected 13.", result.size(), res);
            return false;
        }

        if (result == lastSensorData)
            return true;

        // Parse PA response (Power Aggregate Report)
        // Fields: [0]=PA, [1-6]=Power ports PWM, [7-9]=Dew PWM, [10]=Buck, [11]=Boost, [12]=Relay

        // Update Power ports (P1-P6) - UPBv3 has fixed 6 power ports
        PI::PowerChannelsSP[0].setState((std::stoi(result[1]) > 0) ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP[1].setState((std::stoi(result[2]) > 0) ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP[2].setState((std::stoi(result[3]) > 0) ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP[3].setState((std::stoi(result[4]) > 0) ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP[4].setState((std::stoi(result[5]) > 0) ? ISS_ON : ISS_OFF);
        PI::PowerChannelsSP[5].setState((std::stoi(result[6]) > 0) ? ISS_ON : ISS_OFF);
        if (sensorUpdated(result, 1, 6))
            PI::PowerChannelsSP.apply();

        // Update Dew heaters (D1-D3) - UPBv3 has fixed 3 dew heaters, PWM values 0-100
        PI::DewChannelDutyCycleNP[0].setValue(std::stod(result[7]));
        PI::DewChannelDutyCycleNP[1].setValue(std::stod(result[8]));
        PI::DewChannelDutyCycleNP[2].setValue(std::stod(result[9]));
        if (sensorUpdated(result, 7, 9))
            PI::DewChannelDutyCycleNP.apply();

        // Update Buck/Boost status (indices 10, 11)
        // Update Relay status (index 12)
        m_RelayState = (std::stoi(result[12]) == 1);

        lastSensorData = result;
        return true;
    }
    return false;
}

bool PegasusUPB3::getPowerData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PC", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < 3)
        {
            LOGF_WARN("Received wrong number (%i) of power data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastPowerData)
            return true;

        PowerConsumptionNP[CONSUMPTION_AVG_AMPS].setValue(std::stod(result[1]));
        PowerConsumptionNP[CONSUMPTION_AMP_HOURS].setValue(std::stod(result[2]));
        PowerConsumptionNP[CONSUMPTION_WATT_HOURS].setValue(std::stod(result[3]));
        PowerConsumptionNP.setState(IPS_OK);
        PowerConsumptionNP.apply();

        if (result.size() >= 5)
        {
            FirmwareTP[FIRMWARE_UPTIME].setText(result[4].c_str());
            FirmwareTP.apply();
        }

        lastPowerData = result;
        return true;
    }
    return false;
}

bool PegasusUPB3::getStepperData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < 5)
        {
            LOGF_WARN("Received wrong number (%i) of stepper data (%s). Retrying...", result.size(), res);
            return false;
        }

        if (result == lastStepperData)
            return true;

        FocusAbsPosNP[0].setValue(std::stoi(result[1]));
        focusMotorRunning = (std::stoi(result[2]) == 1);

        if (FocusAbsPosNP.getState() == IPS_BUSY && focusMotorRunning == false)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
        }
        else if (stepperUpdated(result, 1))
            FocusAbsPosNP.apply();

        FocusReverseSP[INDI_ENABLED].setState((std::stoi(result[3]) == 1) ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState((std::stoi(result[3]) == 1) ? ISS_OFF : ISS_ON);

        if (stepperUpdated(result, 3))
            FocusReverseSP.apply();

        lastStepperData = result;
        return true;
    }
    return false;
}

bool PegasusUPB3::getAutoDewData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("PD", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() < 2)
        {
            return true;  // AutoDew data may not always be available
        }

        if (result == lastAutoDewData)
            return true;

        lastAutoDewData = result;
        return true;
    }
    return false;
}

bool PegasusUPB3::getUSBStatus()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("UA", res))
    {
        std::vector<std::string> result = split(res, ":");
        // UA response format: UA:USB1:USB2:USB3:USB4:USB5:USB6:USB7:USB8
        // Total: 9 fields (UA + 8 USB port statuses)
        if (result.size() < 9)
        {
            return true;  // USB data may not always be available
        }

        // Update USB port statuses (0 = off, 1 = on)
        bool changed = false;
        for (size_t i = 0; i < 8 && i < PI::USBPortSP.size(); i++)
        {
            auto newState = (std::stoi(result[i + 1]) == 1) ? ISS_ON : ISS_OFF;
            if (PI::USBPortSP[i].getState() != newState)
            {
                PI::USBPortSP[i].setState(newState);
                changed = true;
            }
        }

        if (changed)
            PI::USBPortSP.apply();

        return true;
    }
    return false;
}

bool PegasusUPB3::getInputVoltageCurrentData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("VR", res))
    {
        std::vector<std::string> result = split(res, ":");
        // VR response format: VR:<voltage>:<current>
        if (result.size() < 3)
        {
            return true;  // VR data may not always be available
        }

        // Check if data changed
        if (result == lastVRData)
            return true;

        // Update PowerSensorsNP from PowerInterface
        double voltage = std::stod(result[1]);
        double current = std::stod(result[2]);
        double power = voltage * current;

        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(voltage);
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(current);
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(power);
        PI::PowerSensorsNP.setState(IPS_OK);
        PI::PowerSensorsNP.apply();

        lastVRData = result;
        return true;
    }
    return false;
}

bool PegasusUPB3::getEnvironmentalData()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("ES", res))
    {
        std::vector<std::string> result = split(res, ":");
        // ES response format: ES:<temp>:<humidity>:<dewpoint>:<flag>
        if (result.size() < 4)
        {
            return true;  // ES data may not always be available
        }

        // Check if data changed
        if (result == lastESData)
            return true;

        // Update Weather parameters
        setParameterValue("WEATHER_TEMPERATURE", std::stod(result[1]));
        setParameterValue("WEATHER_HUMIDITY", std::stod(result[2]));
        setParameterValue("WEATHER_DEWPOINT", std::stod(result[3]));

        if (WI::syncCriticalParameters())
            WI::critialParametersLP.apply();

        WI::ParametersNP.setState(IPS_OK);
        WI::ParametersNP.apply();

        lastESData = result;
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
/// PowerInterface Implementation
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::SetPowerPort(size_t port, bool enabled)
{
    // Port numbers are 0-based in interface, device expects 1-based (P1-P6)
    // Set to 100% PWM if enabled, 0% if disabled
    return setPowerEnabled(port + 1, enabled ? 100 : 0);
}

bool PegasusUPB3::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // Dew ports are D1-D3, convert duty cycle (0-100%) to 0-255 PWM
    auto pwmValue = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setDewPWM(port + 1, enabled ? pwmValue : 0);
}

bool PegasusUPB3::SetVariablePort(size_t port, bool enabled, double voltage)
{
    // Port 0 = Buck (PJ), Port 1 = Boost (PB)
    if (port == 0)
    {
        if (enabled)
            return setBuckVoltage(static_cast<uint8_t>(voltage));
        else
            return setBuckEnabled(false);
    }
    else if (port == 1)
    {
        if (enabled)
            return setBoostVoltage(static_cast<uint8_t>(voltage));
        else
            return setBoostEnabled(false);
    }
    return false;
}

bool PegasusUPB3::SetLEDEnabled(bool enabled)
{
    return setPowerLEDEnabled(enabled);
}

bool PegasusUPB3::SetAutoDewEnabled(size_t port, bool enabled)
{
    return setAutoDewEnabled(port + 1, enabled);
}

bool PegasusUPB3::CyclePower()
{
    // Turn all power ports off then on
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};

    // Turn all off
    for (int i = 1; i <= 6; i++)
    {
        snprintf(cmd, PEGASUS_LEN, "P%d:0", i);
        sendCommand(cmd, res);
    }

    // Short delay
    usleep(100000); // 100ms

    // Turn all on
    for (int i = 1; i <= 6; i++)
    {
        snprintf(cmd, PEGASUS_LEN, "P%d:100", i);
        sendCommand(cmd, res);
    }

    return true;
}

bool PegasusUPB3::SetUSBPort(size_t port, bool enabled)
{
    return setUSBPortEnabled(port + 1, enabled);
}

//////////////////////////////////////////////////////////////////////
/// OutputInterface Implementation (Relay)
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::UpdateDigitalOutputs()
{
    // Update relay status from device state
    for (size_t i = 0; i < DigitalOutputsSP.size(); i++)
    {
        auto oldState = DigitalOutputsSP[i].findOnSwitchIndex();
        auto newState = m_RelayState ? 1 : 0;

        if (oldState != newState)
        {
            DigitalOutputsSP[i].reset();
            DigitalOutputsSP[i][newState].setState(ISS_ON);
            DigitalOutputsSP[i].setState(IPS_OK);
            DigitalOutputsSP[i].apply();
        }
    }

    return true;
}

bool PegasusUPB3::CommandOutput(uint32_t index, OutputState command)
{
    if (index >= 1)  // We only have 1 relay (index 0)
        return false;

    bool enabled = (command == OutputState::On);

    if (setRelay(enabled))
    {
        m_RelayState = enabled;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
/// FocuserInterface Implementation
//////////////////////////////////////////////////////////////////////
IPState PegasusUPB3::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SM:%u", targetTicks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }
    return IPS_ALERT;
}

IPState PegasusUPB3::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SG:%c%u", dir == FOCUS_INWARD ? '-' : '+', ticks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }
    return IPS_ALERT;
}

bool PegasusUPB3::AbortFocuser()
{
    char res[PEGASUS_LEN] = {0};
    if (sendCommand("SH", res))
    {
        return strstr(res, "SH:1") != nullptr;
    }
    return false;
}

bool PegasusUPB3::ReverseFocuser(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SR:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::SyncFocuser(uint32_t ticks)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "SC:%u", ticks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }
    return false;
}

bool PegasusUPB3::SetFocuserBacklash(int32_t steps)
{
    // UPBv3 doesn't have explicit backlash command in documentation
    // This would need to be handled in software
    INDI_UNUSED(steps);
    return true;
}

bool PegasusUPB3::SetFocuserBacklashEnabled(bool enabled)
{
    // UPBv3 doesn't have explicit backlash command in documentation
    INDI_UNUSED(enabled);
    return true;
}

//////////////////////////////////////////////////////////////////////
/// ISNewSwitch
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Reboot
        if (RebootSP.isNameMatch(name))
        {
            RebootSP.setState(reboot() ? IPS_OK : IPS_ALERT);
            RebootSP.apply();
            LOG_INFO("Rebooting device...");
            return true;
        }

        // Power on boot
        if (PowerOnBootSP.isNameMatch(name))
        {
            PowerOnBootSP.update(states, names, n);
            PowerOnBootSP.setState(setPowerOnBoot() ? IPS_OK : IPS_ALERT);
            PowerOnBootSP.apply();
            saveConfig(PowerOnBootSP);
            return true;
        }

        // Focuser Interface
        if (FI::processSwitch(dev, name, states, names, n))
            return true;

        // Power Interface
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        // Output Interface (Relay)
        if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
            return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
/// ISNewNumber
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Auto Dew Aggressiveness (Global)
        if (AutoDewAggNP.isNameMatch(name))
        {
            if (setAutoDewAgg(static_cast<uint8_t>(values[0])))
            {
                AutoDewAggNP[0].setValue(values[0]);
                AutoDewAggNP.setState(IPS_OK);
            }
            else
            {
                AutoDewAggNP.setState(IPS_ALERT);
            }
            AutoDewAggNP.apply();
            return true;
        }

        // Auto Dew Aggressiveness per Port
        if (AutoDewAggPerPortNP.isNameMatch(name))
        {
            AutoDewAggPerPortNP.update(values, names, n);
            AutoDewAggPerPortNP.setState(IPS_OK);
            AutoDewAggPerPortNP.apply();
            saveConfig(AutoDewAggPerPortNP);
            return true;
        }

        // Focuser Settings
        if (FocuserSettingsNP.isNameMatch(name))
        {
            FocuserSettingsNP.update(values, names, n);

            setFocuserMaxSpeed(static_cast<uint16_t>(FocuserSettingsNP[SETTING_MAX_SPEED].getValue()));
            setFocuserMicrostepping(static_cast<uint8_t>(FocuserSettingsNP[SETTING_MICROSTEPPING].getValue()));
            setFocuserCurrentLimit(static_cast<uint16_t>(FocuserSettingsNP[SETTING_CURRENT_LIMIT].getValue()));
            setFocuserHoldTorque(static_cast<bool>(FocuserSettingsNP[SETTING_HOLD_TORQUE].getValue()));

            FocuserSettingsNP.setState(IPS_OK);
            FocuserSettingsNP.apply();
            saveConfig(FocuserSettingsNP);
            return true;
        }

        // Focuser
        if (FI::processNumber(dev, name, values, names, n))
            return true;

        // Weather
        if (WI::processNumber(dev, name, values, names, n))
            return true;

        // Power Interface
        if (PI::processNumber(dev, name, values, names, n))
            return true;

        // Output Interface (Relay)
        if (INDI::OutputInterface::processNumber(dev, name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
/// ISNewText
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Interface (USB/Power labels)
        if (PI::processText(dev, name, texts, names, n))
            return true;

        // Output Interface (Relay labels)
        if (INDI::OutputInterface::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

//////////////////////////////////////////////////////////////////////
/// Save Config Items
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);

    AutoDewAggNP.save(fp);
    AutoDewAggPerPortNP.save(fp);
    FocuserSettingsNP.save(fp);
    PowerOnBootSP.save(fp);

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Timer Hit
//////////////////////////////////////////////////////////////////////
void PegasusUPB3::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (getSensorData())
    {
        getPowerData();
        getStepperData();
        getAutoDewData();
        getUSBStatus();
        getInputVoltageCurrentData();
        getEnvironmentalData();
        UpdateDigitalOutputs();
    }

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
/// Send Command
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        // Simulation responses will be added incrementally
        if (!strcmp(cmd, "PV"))
        {
            strncpy(res, "PV:1.0", PEGASUS_LEN);
        }
        else if (res)
        {
            strncpy(res, cmd, PEGASUS_LEN);
        }
        return true;
    }

    for (int i = 0; i < 2; i++)
    {
        char command[PEGASUS_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, PEGASUS_LEN, "%s\n", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, PEGASUS_LEN, stopChar, PEGASUS_TIMEOUT, &nbytes_read)) != TTY_OK
                || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);

        cleanupResponse(res);
        LOGF_DEBUG("RES <%s>", res);
        return true;
    }

    if (tty_rc != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial error: %s", errorMessage);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
/// Cleanup Response
//////////////////////////////////////////////////////////////////////
void PegasusUPB3::cleanupResponse(char *response)
{
    std::string s(response);
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char x)
    {
        return std::isspace(x);
    }), s.end());
    strncpy(response, s.c_str(), PEGASUS_LEN);
}

//////////////////////////////////////////////////////////////////////
/// Split String
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusUPB3::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

//////////////////////////////////////////////////////////////////////
/// Sensor Data Updated Check
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::sensorUpdated(const std::vector<std::string> &result, uint8_t start, uint8_t end)
{
    if (lastSensorData.empty())
        return true;

    for (uint8_t index = start; index <= end; index++)
    {
        if (index >= lastSensorData.size() || result[index] != lastSensorData[index])
            return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
/// Stepper Data Updated Check
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::stepperUpdated(const std::vector<std::string> &result, uint8_t index)
{
    if (lastStepperData.empty())
        return true;

    return index >= lastStepperData.size() || result[index] != lastStepperData[index];
}

//////////////////////////////////////////////////////////////////////
/// Reboot
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::reboot()
{
    return sendCommand("PF", nullptr);
}

//////////////////////////////////////////////////////////////////////
/// Init Properties
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE | POWER_INTERFACE | OUTPUT_INTERFACE);

    // Focuser Interface
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT);

    FI::initProperties(FOCUS_TAB);

    // Weather Interface
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");

    // Power Interface
    PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_DEW_OUT | POWER_HAS_VARIABLE_OUT |
                      POWER_HAS_VOLTAGE_SENSOR | POWER_HAS_OVERALL_CURRENT | POWER_HAS_PER_PORT_CURRENT |
                      POWER_HAS_LED_TOGGLE | POWER_HAS_AUTO_DEW | POWER_HAS_POWER_CYCLE | POWER_HAS_USB_TOGGLE);
    // 6 DC ports, 3 DEW ports, 2 Variable ports (Buck+Boost), 3 Auto Dew ports (per-port), 8 USB ports
    PI::initProperties(POWER_TAB, 6, 3, 2, 3, 8);

    // Set Variable port labels to Buck and Boost
    PI::VariableChannelLabelsTP[0].setLabel("Buck");
    PI::VariableChannelLabelsTP[1].setLabel("Boost");

    // Set Variable port voltage ranges
    // Port 0 = Buck (3-12V), Port 1 = Boost (12-24V)
    PI::VariableChannelVoltsNP[0].setMinMax(3, 12);
    PI::VariableChannelVoltsNP[0].setValue(12);
    PI::VariableChannelVoltsNP[1].setMinMax(12, 24);
    PI::VariableChannelVoltsNP[1].setStep(3);
    PI::VariableChannelVoltsNP[1].setValue(12);

    // Populate PI::USBPortLabelsTP with custom labels
    if (PI::USBPortLabelsTP.size() >= 8)
    {
        PI::USBPortLabelsTP[0].setLabel("USB Port 1");
        PI::USBPortLabelsTP[1].setLabel("USB Port 2");
        PI::USBPortLabelsTP[2].setLabel("USB Port 3");
        PI::USBPortLabelsTP[3].setLabel("USB Port 4");
        PI::USBPortLabelsTP[4].setLabel("USB Port 5");
        PI::USBPortLabelsTP[5].setLabel("USB Port 6");
        PI::USBPortLabelsTP[6].setLabel("USB Port 7");
        PI::USBPortLabelsTP[7].setLabel("USB Port 8");
    }

    // Output Interface for Relay
    INDI::OutputInterface::initProperties(RELAY_TAB, 1, "Relay");

    addAuxControls();

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Power Consumption
    PowerConsumptionNP[CONSUMPTION_AVG_AMPS].fill("CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_AMP_HOURS].fill("CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[CONSUMPTION_WATT_HOURS].fill("CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP.fill(getDeviceName(), "POWER_CONSUMPTION", "Consumption", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Power on Boot
    PowerOnBootSP[POWER_PORT_1].fill("POWER_PORT_1", "Power Port 1", ISS_ON);
    PowerOnBootSP[POWER_PORT_2].fill("POWER_PORT_2", "Power Port 2", ISS_ON);
    PowerOnBootSP[POWER_PORT_3].fill("POWER_PORT_3", "Power Port 3", ISS_ON);
    PowerOnBootSP[POWER_PORT_4].fill("POWER_PORT_4", "Power Port 4", ISS_ON);
    PowerOnBootSP[POWER_PORT_5].fill("POWER_PORT_5", "Power Port 5", ISS_ON);
    PowerOnBootSP[POWER_PORT_6].fill("POWER_PORT_6", "Power Port 6", ISS_ON);
    PowerOnBootSP.fill(getDeviceName(), "POWER_ON_BOOT", "Power On Boot", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Overcurrent Protection
    OverCurrentLP[OC_POWER_1].fill("OC_POWER_1", "Power Port 1", IPS_OK);
    OverCurrentLP[OC_POWER_2].fill("OC_POWER_2", "Power Port 2", IPS_OK);
    OverCurrentLP[OC_POWER_3].fill("OC_POWER_3", "Power Port 3", IPS_OK);
    OverCurrentLP[OC_POWER_4].fill("OC_POWER_4", "Power Port 4", IPS_OK);
    OverCurrentLP[OC_POWER_5].fill("OC_POWER_5", "Power Port 5", IPS_OK);
    OverCurrentLP[OC_POWER_6].fill("OC_POWER_6", "Power Port 6", IPS_OK);
    OverCurrentLP[OC_DEW_1].fill("OC_DEW_1", "Dew A", IPS_OK);
    OverCurrentLP[OC_DEW_2].fill("OC_DEW_2", "Dew B", IPS_OK);
    OverCurrentLP[OC_DEW_3].fill("OC_DEW_3", "Dew C", IPS_OK);
    OverCurrentLP.fill(getDeviceName(), "OVER_CURRENT", "Overcurrent", POWER_TAB, IPS_IDLE);

    // Auto Dew Aggressiveness (Global)
    AutoDewAggNP[AUTO_DEW_AGG].fill("AUTO_DEW_AGG_VALUE", "Global (0-10)", "%.f", 0, 10, 1, 5);
    AutoDewAggNP.fill(getDeviceName(), "AUTO_DEW_AGG", "Auto Dew Agg", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // Auto Dew Aggressiveness per Port
    AutoDewAggPerPortNP[AUTO_DEW_AGG_1].fill("AUTO_DEW_AGG_1", "Port 1 (1-10)", "%.f", 1, 10, 1, 5);
    AutoDewAggPerPortNP[AUTO_DEW_AGG_2].fill("AUTO_DEW_AGG_2", "Port 2 (1-10)", "%.f", 1, 10, 1, 5);
    AutoDewAggPerPortNP[AUTO_DEW_AGG_3].fill("AUTO_DEW_AGG_3", "Port 3 (1-10)", "%.f", 1, 10, 1, 5);
    AutoDewAggPerPortNP.fill(getDeviceName(), "AUTO_DEW_AGG_PER_PORT", "Per-Port Agg", DEW_TAB, IP_RW, 60, IPS_IDLE);

    // Focuser Settings
    FocuserSettingsNP[SETTING_MAX_SPEED].fill("SETTING_MAX_SPEED", "Max Speed", "%.f", 0, 1000, 100, 400);
    FocuserSettingsNP[SETTING_MICROSTEPPING].fill("SETTING_MICROSTEPPING", "Microstepping", "%.f", 1, 32, 1, 2);
    FocuserSettingsNP[SETTING_CURRENT_LIMIT].fill("SETTING_CURRENT_LIMIT", "Current Limit (mA)", "%.f", 0, 3000, 100, 1000);
    FocuserSettingsNP[SETTING_HOLD_TORQUE].fill("SETTING_HOLD_TORQUE", "Hold Torque (0/1)", "%.f", 0, 1, 1, 0);
    FocuserSettingsNP.fill(getDeviceName(), "FOCUSER_SETTINGS", "Settings", FOCUS_TAB, IP_RW, 60, IPS_IDLE);

    // Firmware
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "NA");
    FirmwareTP[FIRMWARE_UPTIME].fill("UPTIME", "Uptime (s)", "NA");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", FIRMWARE_TAB, IP_RO, 60, IPS_IDLE);

    // Serial Connection
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Update Properties
//////////////////////////////////////////////////////////////////////
bool PegasusUPB3::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Setup Parameters
        setupParams();

        // Main Control
        defineProperty(PowerConsumptionNP);
        defineProperty(RebootSP);

        // Power
        defineProperty(PowerOnBootSP);
        defineProperty(OverCurrentLP);

        // Dew
        defineProperty(AutoDewAggNP);
        defineProperty(AutoDewAggPerPortNP);

        // Focuser
        FI::updateProperties();
        defineProperty(FocuserSettingsNP);

        // Weather
        WI::updateProperties();

        // Power Interface
        PI::updateProperties();

        // Output Interface (Relay)
        INDI::OutputInterface::updateProperties();

        // Firmware
        defineProperty(FirmwareTP);

        setupComplete = true;
    }
    else
    {
        // Main Control
        deleteProperty(PowerConsumptionNP);
        deleteProperty(RebootSP);

        // Power
        deleteProperty(PowerOnBootSP);
        deleteProperty(OverCurrentLP);

        // Dew
        deleteProperty(AutoDewAggNP);
        deleteProperty(AutoDewAggPerPortNP);

        // Focuser
        FI::updateProperties();
        deleteProperty(FocuserSettingsNP);

        // Weather
        WI::updateProperties();

        // Power Interface
        PI::updateProperties();

        // Output Interface (Relay)
        INDI::OutputInterface::updateProperties();

        // Firmware
        deleteProperty(FirmwareTP);

        setupComplete = false;
    }

    return true;
}
