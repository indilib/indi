/*
  NStep Focuser

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "nstep.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <memory>
#include <thread>
#include <chrono>

static std::unique_ptr<NStep> nstep(new NStep());

void ISGetProperties(const char *dev)
{
    nstep->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    nstep->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    nstep->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    nstep->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    nstep->ISSnoopDevice(root);
}

NStep::NStep()
{
    setVersion(1, 2);
    FI::SetCapability(FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_VARIABLE_SPEED);
}

bool NStep::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -100, 100, 0, 0);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Compensation Modes
    CompensationModeSP[COMPENSATION_MODE_OFF].fill("COMPENSATION_MODE_OFF", "Off", ISS_ON);
    CompensationModeSP[COMPENSATION_MODE_ONE_SHOT].fill("COMPENSATION_MODE_ONE_SHOT", "One shot", ISS_OFF);
    CompensationModeSP[COMPENSATION_MODE_AUTO].fill("COMPENSATION_MODE_AUTO", "Auto", ISS_OFF);
    CompensationModeSP.fill(getDeviceName(), "COMPENSATION_MODE", "Mode",
                       COMPENSATION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    // Prime for Manual
    PrimeManualSP[0].fill("MANUAL_MODE_PRIME", "Prime Manual Mode", ISS_OFF);
    PrimeManualSP.fill(getDeviceName(), "COMPENSATION_PRIME", "Prime",
                       COMPENSATION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    // Compensation Settings
    CompensationSettingsNP[COMPENSATION_SETTING_CHANGE].fill("COMPENSATION_SETTING_CHANGE", "Delta T. (C)", "%.1f", -99, 99, 0.1, 0);
    CompensationSettingsNP[COMPENSATION_SETTING_STEP].fill("COMPENSATION_SETTING_STEP", "Steps per Delta", "%.0f", 0, 999, 1, 0);
    CompensationSettingsNP[COMPENSATION_SETTING_BACKLASH].fill("COMPENSATION_SETTING_BACKLASH", "Backlash steps", "%.0f", 0, 999, 1, 0);
    CompensationSettingsNP[COMPENSATION_SETTING_TIMER].fill("COMPENSATION_SETTING_TIMER", "Averaged Time (s)", "%.0f", 0, 75, 1, 0);
    CompensationSettingsNP.fill(getDeviceName(), "COMPENSATION_SETTING", "Settings",
                       COMPENSATION_TAB, IP_RW, 0, IPS_OK);

    // Stepping Modes
    SteppingModeSP[STEPPING_WAVE].fill("STEPPING_WAVE", "Wave", ISS_OFF);
    SteppingModeSP[STEPPING_HALF].fill("STEPPING_HALF", "Half", ISS_OFF);
    SteppingModeSP[STEPPING_FULL].fill("STEPPING_FULL", "Full", ISS_ON);
    SteppingModeSP.fill(getDeviceName(), "STEPPING_MODE", "Mode",
                       STEPPING_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    // Stepping Phase
    SteppingPhaseNP[0].fill("PHASES", "Wiring", "%.f", 0, 2, 1, 0);
    SteppingPhaseNP.fill(getDeviceName(), "STEPPING_PHASE", "Phase",
                       STEPPING_TAB, IP_RW, 0, IPS_OK);

    // Max Speed
    MaxSpeedNP[0].fill("RATE", "Rate", "%.f", 1, 254, 10, 0);
    MaxSpeedNP.fill(getDeviceName(), "MAX_SPEED", "Max Speed",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_OK);

    // Coil Energized Status
    CoilStatusSP[COIL_ENERGIZED_OFF].fill("COIL_ENERGIZED_OFF", "De-energized", ISS_OFF);
    CoilStatusSP[COIL_ENERGIZED_ON].fill("COIL_ENERGIZED_OFF", "Energized", ISS_OFF);
    CoilStatusSP.fill(getDeviceName(), "COIL_MODE", "Coil After Move",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    addDebugControl();

    // Set limits as per documentation
    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(999999);
    FocusAbsPosNP[0].setStep(1000);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(999);
    FocusRelPosNP[0].setStep(100);

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(254);
    FocusSpeedNP[0].setStep(10);

    return true;
}

const char *NStep::getDefaultName()
{
    return "Rigel NStep";
}

bool NStep::updateProperties()
{
    if (isConnected())
    {
        // Read these values before defining focuser interface properties
        readPosition();
        readSpeedInfo();
    }

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        if (readTemperature())
            defineProperty(TemperatureNP);

        bool rc = getStartupValues();

        // Settings
        defineProperty(MaxSpeedNP);
        defineProperty(CompensationModeSP);
        defineProperty(PrimeManualSP);
        defineProperty(CompensationSettingsNP);
        defineProperty(SteppingModeSP);
        defineProperty(SteppingPhaseNP);
        defineProperty(CoilStatusSP);

        if (rc)
            LOG_INFO("NStep is ready.");
        else
            LOG_WARN("Failed to query startup values.");
    }
    else
    {
        if (TemperatureNP.getState() == IPS_OK)
            deleteProperty(TemperatureNP.getName());

        deleteProperty(MaxSpeedNP.getName());
        deleteProperty(CompensationModeSP.getName());
        deleteProperty(PrimeManualSP.getName());
        deleteProperty(CompensationSettingsNP.getName());
        deleteProperty(SteppingModeSP.getName());
        deleteProperty(SteppingPhaseNP.getName());
        deleteProperty(CoilStatusSP.getName());
    }

    return true;
}

bool NStep::Handshake()
{
    char cmd[NSTEP_LEN] = {0}, res[NSTEP_LEN] = {0};

    // Ack
    cmd[0] = 0x6;

    bool rc = sendCommand(cmd, res, 1, 1);
    if (rc == false)
        return false;

    return res[0] == 'S';
}

bool NStep::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[NSTEP_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
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
        rc = tty_read(PortFD, res, res_len, NSTEP_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, NSTEP_LEN, NSTEP_STOP_CHAR, NSTEP_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[NSTEP_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void NStep::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool NStep::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Compensation Settings
        if (CompensationSettingsNP.isNameMatch(name))
        {
            // Extract values
            int change = 0, step = 0, backlash = 0, timer = 0;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], CompensationSettingsNP[COMPENSATION_SETTING_CHANGE].getName()))
                {
                    change = values[i];
                }
                else if (!strcmp(names[i], CompensationSettingsNP[COMPENSATION_SETTING_STEP].getName()))
                {
                    step = values[i];
                }
                else if (!strcmp(names[i], CompensationSettingsNP[COMPENSATION_SETTING_BACKLASH].getName()))
                {
                    backlash = values[i];
                }
                else if (!strcmp(names[i], CompensationSettingsNP[COMPENSATION_SETTING_TIMER].getName()))
                {
                    timer = values[i];
                }
            }

            // Try to update settings
            if (setCompensationSettings(change, step, backlash, timer))
            {
                CompensationSettingsNP.update(values, names, n);
                CompensationSettingsNP.setState(IPS_OK);
            }
            else
            {
                CompensationSettingsNP.setState(IPS_ALERT);
            }

            CompensationSettingsNP.apply();
            return true;
        }


        // Stepping Phase
        if (SteppingPhaseNP.isNameMatch(name))
        {
            if (setSteppingPhase(static_cast<uint8_t>(values[0])))
            {
                SteppingPhaseNP.update(values, names, n);
                SteppingPhaseNP.setState(IPS_OK);
            }
            else
                SteppingPhaseNP.setState(IPS_ALERT);

            SteppingPhaseNP.apply();
            return true;
        }

        // Max Speed
        if (MaxSpeedNP.isNameMatch(name))
        {
            if (setMaxSpeed(static_cast<uint8_t>(values[0])))
            {
                MaxSpeedNP.update(values, names, n);
                MaxSpeedNP.setState(IPS_OK);

                // We must update the Min/Max of focus speed
                FocusSpeedNP[0].setMax(values[0]);
                FocusSpeedNP.updateMinMax();
            }
            else
            {
                MaxSpeedNP.setState(IPS_ALERT);
            }

            MaxSpeedNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool NStep::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Compensation Mode
        if (CompensationModeSP.isNameMatch(name))
        {
            int prevIndex = CompensationModeSP.findOnSwitchIndex();
            CompensationModeSP.update(states, names, n);
            int mode = CompensationModeSP.findOnSwitchIndex();
            if (setCompensationMode(mode))
            {
                CompensationModeSP.setState(IPS_OK);
                // If it was set to one shot, we put it back to off?
                switch (mode)
                {
                    case COMPENSATION_MODE_OFF:
                        LOG_INFO("Temperature compensation is disabled.");
                        break;

                    case COMPENSATION_MODE_ONE_SHOT:
                        CompensationModeSP.reset();
                        CompensationModeSP[COMPENSATION_MODE_OFF].setState(ISS_ON);
                        LOG_INFO("One shot compensation applied.");
                        break;

                    case COMPENSATION_MODE_AUTO:
                        LOG_INFO("Automatic temperature compensation is enabled.");
                        break;
                }
            }
            else
            {
                CompensationModeSP.reset();
                CompensationModeSP[prevIndex].setState(ISS_ON);
                CompensationModeSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to change temperature compensation mode.");
            }

            CompensationModeSP.apply();
            return true;
        }

        // Manual Prime
        if (PrimeManualSP.isNameMatch(name))
        {
            sendCommand(":TI");
            PrimeManualSP.setState(IPS_OK);
            PrimeManualSP.apply();
            LOG_INFO("Prime for manual complete. Click One Shot to apply manual compensation once.");
            return true;
        }

        // Stepping Mode
        if (SteppingModeSP.isNameMatch(name))
        {
            SteppingModeSP.update(states, names, n);
            SteppingModeSP.setState(IPS_OK);
            SteppingModeSP.apply();
            return true;
        }

        // Coil Status after Move is done
        if (CoilStatusSP.isNameMatch(name))
        {
            int prevIndex = CoilStatusSP.findOnSwitchIndex();
            CoilStatusSP.update(states, names, n);
            int state = CoilStatusSP.findOnSwitchIndex();
            if (setCoilStatus(state))
            {
                CoilStatusSP.setState(IPS_OK);
                if (state == COIL_ENERGIZED_ON)
                    LOG_WARN("Coil shall be kept energized after motion is complete. Watch for motor heating!");
                else
                    LOG_INFO("Coil shall be de-energized after motion is complete.");
            }
            else
            {
                CoilStatusSP.reset();
                CoilStatusSP[prevIndex].setState(ISS_ON);
                CoilStatusSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to update coil energization status.");
            }

            CoilStatusSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool NStep::getStartupValues()
{
    bool rc1 = readCoilStatus();
    bool rc2 = readSteppingInfo();
    bool rc3 = readCompensationInfo();

    return (rc1 && rc2 && rc3);
}

IPState NStep::MoveAbsFocuser(uint32_t targetTicks)
{
    m_TargetDiff = targetTicks - FocusAbsPosNP[0].getValue();
    return IPS_BUSY;
}

IPState NStep::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    m_TargetDiff = ticks * ((dir == FOCUS_INWARD) ? -1 : 1);
    return MoveAbsFocuser(FocusAbsPosNP[0].value + m_TargetDiff);
}

bool NStep::AbortFocuser()
{
    return sendCommand("F00000#");
}

void NStep::TimerHit()
{
    if (isConnected() == false)
        return;

    double currentPosition = FocusAbsPosNP[0].getValue();

    readPosition();

    // Check if we have a pending motion
    // and if we STOPPED, then let's take the next action
    if ( (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY) && isMoving() == false)
    {
        // Are we done moving?
        if (m_TargetDiff == 0)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
        }
        else
        {
            // 999 is the max we can go in one command
            // so we need to go 999 or LESS
            // therefore for larger movements, we break it down.
            int nextMotion = (std::abs(m_TargetDiff) > 999) ? 999 : std::abs(m_TargetDiff);
            int direction = m_TargetDiff > 0 ? FOCUS_OUTWARD : FOCUS_INWARD;
            int mode = SteppingModeSP.findOnSwitchIndex();
            char cmd[NSTEP_LEN] = {0};
            snprintf(cmd, NSTEP_LEN, ":F%d%d%03d#", direction, mode, nextMotion);
            if (sendCommand(cmd) == false)
            {
                LOG_ERROR("Failed to issue motion command.");
                if (FocusRelPosNP.getState() == IPS_BUSY)
                {
                    FocusRelPosNP.setState(IPS_ALERT);
                    FocusRelPosNP.apply();
                }
                if (FocusAbsPosNP.getState() == IPS_BUSY)
                {
                    FocusAbsPosNP.setState(IPS_ALERT);
                    FocusAbsPosNP.apply();
                }
            }
            else
                // Reduce target diff depending on the motion direction
                // Negative targetDiff increases eventually to zero
                // Positive targetDiff decreases eventually to zero
                m_TargetDiff = m_TargetDiff + (nextMotion * ((direction == FOCUS_INWARD) ? 1 : -1));
        }
        // Check if can update the absolute position in case it changed.
    }
    else if (currentPosition != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP.apply();
    }

    // Read temperature
    if (TemperatureNP.getState() == IPS_OK && m_TemperatureCounter++ == NSTEP_TEMPERATURE_FREQ)
    {
        m_TemperatureCounter = 0;
        if (readTemperature())
            TemperatureNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

bool NStep::isMoving()
{
    char res[NSTEP_LEN] = {0};

    bool rc = sendCommand("S", res, 1, 1);

    if (rc && res[0] == '1')
        return true;

    return false;
}

bool NStep::readTemperature()
{
    char res[NSTEP_LEN] = {0};

    if (sendCommand(":RT", res, 3, 4) == false)
        return false;

    float temperature = -1000;
    sscanf(res, "%f", &temperature);

    // Divide by 10 to get actual value
    temperature /= 10.0;

    if (temperature < -80)
        return false;

    TemperatureNP[0].setValue(temperature);
    TemperatureNP.setState(IPS_OK);

    return true;
}

bool NStep::readPosition()
{
    char res[NSTEP_LEN] = {0};

    if (sendCommand(":RP", res, 3, 7) == false)
        return false;

    int32_t pos = 1e6;
    sscanf(res, "%d", &pos);

    if (pos == 1e6)
        return false;

    FocusAbsPosNP[0].setValue(pos);

    return true;
}

bool NStep::readCompensationInfo()
{
    char res[NSTEP_LEN] = {0};
    int32_t change = 1e6, step = 1e6, state = 1e6, backlash = 1e6, timer = 1e6;

    // State (Off, One shot, or Auto)
    if (sendCommand(":RG", res, 3, 1) == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (sendCommand(":RG", res, 3, 1) == false)
            return false;
    }
    sscanf(res, "%d", &state);
    if (state == 1e6)
        return false;
    CompensationModeSP.reset();
    CompensationModeSP[state].setState(ISS_ON);
    CompensationModeSP.setState(IPS_OK);

    // Change
    memset(res, 0, NSTEP_LEN);
    if (sendCommand(":RA", res, 3, 4) == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (sendCommand(":RA", res, 3, 4) == false)
            return false;
    }
    sscanf(res, "%d", &change);
    if (change == 1e6)
        return false;
    CompensationSettingsNP[COMPENSATION_SETTING_CHANGE].setValue(change);

    // Step
    memset(res, 0, NSTEP_LEN);
    if (sendCommand(":RB", res, 3, 3) == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (sendCommand(":RB", res, 3, 3) == false)
            return false;
    }
    sscanf(res, "%d", &step);
    if (step == 1e6)
        return false;
    CompensationSettingsNP[COMPENSATION_SETTING_STEP].setValue(step);

    // Backlash
    memset(res, 0, NSTEP_LEN);
    if (sendCommand(":RE", res, 3, 3) == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (sendCommand(":RE", res, 3, 3) == false)
            return false;
    }
    sscanf(res, "%d", &backlash);
    if (backlash == 1e6)
        return false;
    CompensationSettingsNP[COMPENSATION_SETTING_BACKLASH].setValue(backlash);

    // Timer
    memset(res, 0, NSTEP_LEN);
    if (sendCommand(":RH", res, 3, 2) == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (sendCommand(":RH", res, 3, 2) == false)
            return false;
    }
    sscanf(res, "%d", &timer);
    if (timer == 1e6)
        return false;
    CompensationSettingsNP[COMPENSATION_SETTING_TIMER].setValue(timer);
    CompensationSettingsNP.setState(IPS_OK);

    return true;

}

bool NStep::readSpeedInfo()
{
    char res[NSTEP_LEN] = {0};
    int32_t max_step = 1e6, current_step = 1e6;

    // Max Step
    if (sendCommand(":RS", res, 3, 3) == false)
        return false;
    sscanf(res, "%d", &max_step);
    if (max_step == 1e6)
        return false;

    // Current Step
    if (sendCommand(":RO", res, 3, 3) == false)
        return false;
    sscanf(res, "%d", &current_step);
    if (current_step == 1e6)
        return false;

    MaxSpeedNP[0].setValue(254 - max_step + 1);
    MaxSpeedNP.setState(IPS_OK);

    // nStep defines speed step rates from 1 to 254
    // when 1 being the fastest, so for speed we flip the values
    FocusSpeedNP[0].setMax(254 - max_step + 1);
    FocusSpeedNP[0].setValue(254 - current_step + 1);
    FocusSpeedNP.setState(IPS_OK);

    return true;
}

bool NStep::readSteppingInfo()
{
    char res[NSTEP_LEN] = {0};

    if (sendCommand(":RW", res, 3, 1) == false)
        return false;

    int32_t phase = 1e6;
    sscanf(res, "%d", &phase);

    if (phase == 1e6)
        return false;

    SteppingPhaseNP[0].setValue(phase);
    SteppingPhaseNP.setState(IPS_OK);

    return true;
}

bool NStep::readCoilStatus()
{
    char res[NSTEP_LEN] = {0};

    if (sendCommand(":RC", res, 3, 1) == false)
        return false;

    CoilStatusSP.reset();

    CoilStatusSP[COIL_ENERGIZED_OFF].setState((res[0] == '0') ? ISS_ON : ISS_OFF);
    CoilStatusSP[COIL_ENERGIZED_ON].setState((res[0] == '0') ? ISS_OFF : ISS_ON);
    CoilStatusSP.setState(IPS_OK);

    return true;
}

bool NStep::SyncFocuser(uint32_t ticks)
{
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, "#:CP+%06d#", ticks);
    return sendCommand(cmd);
}

bool NStep::SetFocuserSpeed(int speed)
{
    // Speed and Current NStep steps are opposite.
    // Speed 1 is slowest, translated to 254 for nStep.
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, "#:CO%03d#", 254 - speed + 1);
    return sendCommand(cmd);
}

bool NStep::setMaxSpeed(uint8_t maxSpeed)
{
    // INDI Focus Speed and Current NStep steps are opposite.
    // INDI Speed 1 is slowest, translated to 254 for nStep.
    // and vice versa
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, ":CS%03d#", 254 - maxSpeed + 1);
    return sendCommand(cmd);
}

bool NStep::setCompensationMode(uint8_t mode)
{
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, "#:TA%01d#", mode);
    return sendCommand(cmd);
}

bool NStep::setCompensationSettings(double change, double move, double backlash, double timer)
{
    int temperature_change = change * 10;
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, ":TT%+03d#", temperature_change);
    bool rc1 = sendCommand(cmd);

    int temperature_steps = static_cast<int>(move);
    snprintf(cmd, NSTEP_LEN, ":TS%03d#", temperature_steps);
    bool rc2 = sendCommand(cmd);

    int temperature_backlash = static_cast<int>(backlash);
    snprintf(cmd, NSTEP_LEN, ":TB%03d#", temperature_backlash);
    bool rc3 = sendCommand(cmd);


    int temperature_timer = static_cast<int>(timer);
    bool rc4 = true;
    if (timer > 0)
    {
        snprintf(cmd, NSTEP_LEN, ":TC%02d#", temperature_timer);
        rc4 = sendCommand(cmd);
    }

    return (rc1 && rc2 && rc3 && rc4);
}

bool NStep::setSteppingPhase(uint8_t phase)
{
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, "#:CW%01d#", phase);
    return sendCommand(cmd);
}

bool NStep::setCoilStatus(uint8_t status)
{
    char cmd[NSTEP_LEN] = {0};
    snprintf(cmd, NSTEP_LEN, "#:CC%01d#", status == COIL_ENERGIZED_OFF ? 1 : 0);
    return sendCommand(cmd);
}

bool NStep::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    CompensationSettingsNP.save(fp);
    CompensationModeSP.save(fp);
    SteppingModeSP.save(fp);

    return true;
}
