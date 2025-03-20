/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 Starlight Instruments EFS Focuser

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

#include "si_efs.h"

#include <cmath>
#include <cstring>
#include <memory>

#define FOCUS_SETTINGS_TAB "Settings"

static std::unique_ptr<SIEFS> siefs(new SIEFS());

const std::map<SIEFS::SI_COMMANDS, std::string> SIEFS::CommandsMap =
{
    {SIEFS::SI_NOOP, "No Operation"},
    {SIEFS::SI_IN, "Moving Inwards"},
    {SIEFS::SI_OUT, "Moving Outwards"},
    {SIEFS::SI_GOTO, "Goto"},
    {SIEFS::SI_SET_POS, "Set Position"},
    {SIEFS::SI_MAX_POS, "Set Max Position"},
    {SIEFS::SI_FAST_IN, "Fast In"},
    {SIEFS::SI_FAST_OUT, "Fast Out"},
    {SIEFS::SI_HALT, "Halt"},
    {SIEFS::SI_MOTOR_POLARITY, "Motor Polarity"},
};

const std::map<SIEFS::SI_MOTOR, std::string> SIEFS::MotorMap =
{
    {SIEFS::SI_NOT_MOVING, "Idle"},
    {SIEFS::SI_MOVING_IN, "Moving Inwards"},
    {SIEFS::SI_MOVING_OUT, "Moving Outwards"},
    {SIEFS::SI_LOCKED, "Locked"},
};

SIEFS::SIEFS()
{
    setVersion(0, 2);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_CAN_REVERSE);
    setSupportedConnections(CONNECTION_NONE);
}

bool SIEFS::Connect()
{
    if (isSimulation())
    {
        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    handle = hid_open(0x04D8, 0xF056, nullptr);

    if (handle == nullptr)
    {
        LOG_ERROR("No SIEFS focuser found.");
        return false;
    }
    else
    {
        uint32_t maximumPosition = 0;
        bool rc = getMaxPosition(&maximumPosition);
        if (rc)
        {
            FocusMaxPosNP[0].setValue(maximumPosition);

            FocusAbsPosNP[0].setMin(0);
            FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getValue());
            FocusAbsPosNP[0].setStep(FocusMaxPosNP[0].getValue() / 50.0);

            FocusSyncNP[0].setMin(0);
            FocusSyncNP[0].setMax(FocusMaxPosNP[0].getValue());
            FocusSyncNP[0].setStep(FocusMaxPosNP[0].getValue() / 50.0);

            FocusRelPosNP[0].setMax(FocusMaxPosNP[0].getValue() / 2);
            FocusRelPosNP[0].setStep(FocusMaxPosNP[0].getValue() / 100.0);
            FocusRelPosNP[0].setMin(0);
        }

        bool reversed = isReversed();
        FocusReverseSP[INDI_ENABLED].setState(reversed ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState(reversed ? ISS_OFF : ISS_ON);
        FocusReverseSP.setState(IPS_OK);
        SetTimer(getCurrentPollingPeriod());
    }

    return (handle != nullptr);
}

bool SIEFS::Disconnect()
{
    if (isSimulation() == false)
    {
        hid_close(handle);
        hid_exit();
    }

    return true;
}

const char *SIEFS::getDefaultName()
{
    return "SI EFS";
}

bool SIEFS::initProperties()
{
    INDI::Focuser::initProperties();

    addSimulationControl();

    return true;
}

void SIEFS::TimerHit()
{
    if (!isConnected())
        return;

    uint32_t currentTicks = 0;

    bool rc = getAbsPosition(&currentTicks);

    if (rc)
        FocusAbsPosNP[0].setValue(currentTicks);

    getStatus();

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (isSimulation())
        {
            if (FocusAbsPosNP[0].getValue() < targetPosition)
                simPosition += 500;
            else
                simPosition -= 500;

            if (std::abs(simPosition - static_cast<int32_t>(targetPosition)) < 500)
            {
                FocusAbsPosNP[0].setValue(targetPosition);
                simPosition = FocusAbsPosNP[0].getValue();
                m_Motor = SI_NOT_MOVING;
            }

            FocusAbsPosNP[0].setValue(simPosition);
        }

        if (m_Motor == SI_NOT_MOVING && targetPosition == FocusAbsPosNP[0].getValue())
        {
            if (FocusRelPosNP.getState() == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();
            }

            FocusAbsPosNP.setState(IPS_OK);
            LOG_DEBUG("Focuser reached target position.");
        }
    }

    FocusAbsPosNP.apply();

    SetTimer(getCurrentPollingPeriod());
}

IPState SIEFS::MoveAbsFocuser(uint32_t targetTicks)
{
    bool rc = setAbsPosition(targetTicks);

    if (!rc)
        return IPS_ALERT;

    targetPosition = targetTicks;

    rc = sendCommand(SI_GOTO);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState SIEFS::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int direction = (dir == FOCUS_INWARD) ? -1 : 1;
    int reversed = (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);

    int targetAbsPosition = FocusAbsPosNP[0].getValue() + (relative * direction * reversed);

    targetAbsPosition = std::min(static_cast<uint32_t>(FocusMaxPosNP[0].getValue())
                                 , static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosNP[0].getMin()), targetAbsPosition)));

    return MoveAbsFocuser(targetAbsPosition);
}

bool SIEFS::setPosition(uint32_t ticks, uint8_t cmdCode)
{
    int rc = 0;
    uint8_t command[3];
    uint8_t response[3];

    // 20 bit resolution position. 4 high bits + 16 lower bits

    // Send 4 high bits first
    command[0] = cmdCode + 8;
    command[1] = (ticks & 0x40000) >> 16;

    LOGF_DEBUG("Set %s Position (%ld)", cmdCode == 0x20 ? "Absolute" : "Maximum", ticks);
    LOGF_DEBUG("CMD <%02X %02X>", command[0], command[1]);

    if (isSimulation())
        rc = 2;
    else
        rc = hid_write(handle, command, 2);

    if (rc < 0)
    {
        LOGF_ERROR("setPosition: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 2;
        response[0] = cmdCode + 8;
        response[1] = command[1];
    }
    else
        rc = hid_read_timeout(handle, response, 2, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("setPosition: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X>", response[0], response[1]);

    // Send lower 16 bit
    command[0] = cmdCode;
    // Low Byte
    command[1] = ticks & 0xFF;
    // High Byte
    command[2] = (ticks & 0xFF00) >> 8;

    LOGF_DEBUG("CMD <%02X %02X %02X>", command[0], command[1], command[2]);

    if (isSimulation())
        rc = 3;
    else
        rc = hid_write(handle, command, 3);

    if (rc < 0)
    {
        LOGF_ERROR("setPosition: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 3;
        response[0] = command[0];
        response[1] = command[1];
        response[2] = command[2];
    }
    else
        rc = hid_read_timeout(handle, response, 3, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("setPosition: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X %02X>", response[0], response[1], response[2]);

    targetPosition = ticks;

    // TODO add checking later
    return true;
}

bool SIEFS::getPosition(uint32_t *ticks, uint8_t cmdCode)
{
    int rc       = 0;
    uint32_t pos = 0;
    uint8_t command[1] = {0};
    uint8_t response[3] = {0};

    // 20 bit resolution position. 4 high bits + 16 lower bits

    // Get 4 high bits first
    command[0] = cmdCode + 8;

    LOGF_DEBUG("Get %s Position (High 4 bits)", cmdCode == 0x21 ? "Absolute" : "Maximum");
    LOGF_DEBUG("CMD <%02X>", command[0]);

    if (isSimulation())
        rc = 2;
    else
        rc = hid_write(handle, command, 1);

    if (rc < 0)
    {
        LOGF_ERROR("getPosition: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 2;
        response[0] = command[0];
        response[1] = simPosition >> 16;
    }
    else
        rc = hid_read_timeout(handle, response, 2, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("getPosition: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X>", response[0], response[1]);

    // Store 4 high bits part of a 20 bit number
    pos = response[1] << 16;

    // Get 16 lower bits
    command[0] = cmdCode;

    LOGF_DEBUG("Get %s Position (Lower 16 bits)", cmdCode == 0x21 ? "Absolute" : "Maximum");
    LOGF_DEBUG("CMD <%02X>", command[0]);

    if (isSimulation())
        rc = 1;
    else
        rc = hid_write(handle, command, 1);

    if (rc < 0)
    {
        LOGF_ERROR("getPosition: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 3;
        response[0] = command[0];
        response[1] = simPosition & 0xFF;
        response[2] = (simPosition & 0xFF00) >> 8;
    }
    else
        rc = hid_read_timeout(handle, response, 3, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("getPosition: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X %02X>", response[0], response[1], response[2]);

    // Res[1] is lower byte and Res[2] is high byte. Combine them and add them to ticks.
    pos |= response[1] | response[2] << 8;

    *ticks = pos;

    LOGF_DEBUG("%s Position: %ld", cmdCode == 0x21 ? "Absolute" : "Maximum", pos);

    return true;
}

bool SIEFS::setAbsPosition(uint32_t ticks)
{
    return setPosition(ticks, 0x20);
}

bool SIEFS::getAbsPosition(uint32_t *ticks)
{
    return getPosition(ticks, 0x21);
}

bool SIEFS::setMaxPosition(uint32_t ticks)
{
    return setPosition(ticks, 0x22);
}

bool SIEFS::getMaxPosition(uint32_t *ticks)
{
    return getPosition(ticks, 0x23);
}

bool SIEFS::sendCommand(SI_COMMANDS targetCommand)
{
    int rc = 0;
    uint8_t command[2] = {0};
    uint8_t response[3] = {0};

    command[0] = 0x10;
    command[1] = targetCommand;

    LOGF_DEBUG("CMD <%02X %02X>", command[0], command[1]);

    if (isSimulation())
        rc = 2;
    else
        rc = hid_write(handle, command, 2);

    if (rc < 0)
    {
        LOGF_ERROR("setStatus: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 3;
        response[0] = command[0];
        response[1] = 0;
        response[2] = command[1];
        //        m_Motor = targetCommand;
    }
    else
        rc = hid_read_timeout(handle, response, 3, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("setStatus: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X %02X>", response[0], response[1], response[2]);

    if (response[1] == 0xFF)
    {
        LOG_ERROR("setStatus: Invalid state change.");
        return false;
    }

    return true;
}

bool SIEFS::getStatus()
{
    int rc = 0;
    uint8_t command[1] = {0};
    uint8_t response[2] = {0};

    command[0] = 0x11;

    LOGF_DEBUG("CMD <%02X>", command[0]);

    if (isSimulation())
        rc = 1;
    else
        rc = hid_write(handle, command, 1);

    if (rc < 0)
    {
        LOGF_ERROR("getStatus: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc          = 2;
        response[0] = command[0];
        response[1] = m_Motor;
        // Halt/SetPos is state = 0 "not moving".
        //        if (response[1] == SI_HALT || response[1] == SI_SET_POS)
        //            response[1] = 0;
    }
    else
        rc = hid_read_timeout(handle, response, 2, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("getStatus: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X>", response[0], response[1]);

    m_Motor = static_cast<SI_MOTOR>(response[1]);
    if (MotorMap.count(m_Motor) > 0)
        LOGF_DEBUG("State: %s", MotorMap.at(m_Motor).c_str());
    else
    {
        LOGF_WARN("Warning: Unknown status (%d)", response[1]);
        return false;
    }

    return true;
}

bool SIEFS::AbortFocuser()
{
    return sendCommand(SI_HALT);
}

bool SIEFS::SyncFocuser(uint32_t ticks)
{
    bool rc = setAbsPosition(ticks);

    if (!rc)
        return false;

    simPosition = ticks;

    rc = sendCommand(SI_SET_POS);

    return rc;
}

bool SIEFS::SetFocuserMaxPosition(uint32_t ticks)
{
    bool rc = setAbsPosition(ticks);

    if (!rc)
        return false;

    rc = sendCommand(SI_MAX_POS);

    return rc;
}

bool SIEFS::ReverseFocuser(bool enabled)
{
    return setReversed(enabled);
}

bool SIEFS::setReversed(bool enabled)
{
    int rc = 0;
    uint8_t command[2] = {0};
    uint8_t response[2] = {0};

    command[0] = SI_MOTOR_POLARITY;
    command[1] = enabled ? 1 : 0;

    LOGF_DEBUG("CMD <%02X> <%02X>", command[0], command[1]);

    if (isSimulation())
        rc = 1;
    else
        rc = hid_write(handle, command, 2);

    if (rc < 0)
    {
        LOGF_ERROR("setReversed: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc = 2;
        response[0] = command[0];
        // Normal
        response[1] = 0;
    }
    else
        rc = hid_read_timeout(handle, response, 2, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("setReversed: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X>", response[0], response[1]);

    return true;
}

bool SIEFS::isReversed()
{
    int rc = 0;
    uint8_t command[1] = {0};
    uint8_t response[2] = {0};

    command[0] = SI_MOTOR_POLARITY;

    LOGF_DEBUG("CMD <%02X>", command[0]);

    if (isSimulation())
        rc = 1;
    else
        rc = hid_write(handle, command, 1);

    if (rc < 0)
    {
        LOGF_ERROR("setReversed: Error writing to device (%s)", hid_error(handle));
        return false;
    }

    if (isSimulation())
    {
        rc = 2;
        response[0] = command[0];
        // Normal
        response[1] = 0;
    }
    else
        rc = hid_read_timeout(handle, response, 2, SI_TIMEOUT);

    if (rc < 0)
    {
        LOGF_ERROR("setReversed: Error reading from device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%02X %02X>", response[0], response[1]);

    return response[1] != 0;
}
