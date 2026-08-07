/*
    INDI driver for IKunFocuser
    Copyright (C) 2026 IKunFocuser contributors
    SPDX-License-Identifier: LGPL-2.1-or-later

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.
*/

#include "ikunfocuser.h"

#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<IKunFocuser> ikunFocuser(new IKunFocuser());

IKunFocuser::IKunFocuser()
{
    setVersion(1, 0);
    setSupportedConnections(CONNECTION_SERIAL | CONNECTION_TCP);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE);
}

const char *IKunFocuser::getDefaultName()
{
    return "IKun Focuser";
}

bool IKunFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    // Speed is fixed at 300 steps/s for reliable torque with 28BYJ-48.
    m_FixedSpeed = 300;

    FocusMaxPosNP[0].setMin(100);
    FocusMaxPosNP[0].setMax(9999999);
    FocusMaxPosNP[0].setStep(1000);
    FocusMaxPosNP[0].setValue(816000);
    applyMaximumPosition(816000);

    AccelerationNP[0].fill("ACCELERATION_VALUE", "Steps/s²", "%.f", 1, 10000, 10, 1000);
    AccelerationNP.fill(getDeviceName(), "FOCUS_ACCELERATION", "Acceleration", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    HoldSP[HOLD_ON].fill("HOLD_ON", "Enabled", ISS_OFF);
    HoldSP[HOLD_OFF].fill("HOLD_OFF", "Disabled", ISS_ON);
    HoldSP.fill(getDeviceName(), "FOCUS_MOTOR_HOLD", "Motor Hold", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -55, 125, 0, 20);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Firmware", "Unknown");
    FirmwareTP[CONTROLLER_MODEL].fill("MODEL", "Controller", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(4030);

    setDefaultPollingPeriod(500);
    return true;
}

bool IKunFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(AccelerationNP);
        defineProperty(HoldSP);
        defineProperty(TemperatureNP);
        defineProperty(FirmwareTP);

        if (readFullState())
        {
            // Apply fixed speed on every connect.
            SetFocuserSpeed(m_FixedSpeed);
            FocusMaxPosNP.apply();
            FocusAbsPosNP.apply();
            FocusReverseSP.apply();
            AccelerationNP.apply();
            HoldSP.apply();
            TemperatureNP.apply();
            FirmwareTP.apply();
            LOG_INFO("IKunFocuser parameters updated; focuser is ready.");
        }
        else
        {
            LOG_WARN("Connected to IKunFocuser, but one or more startup values could not be read.");
        }
    }
    else
    {
        deleteProperty(AccelerationNP);
        deleteProperty(HoldSP);
        deleteProperty(TemperatureNP);
        deleteProperty(FirmwareTP);
    }

    return true;
}

bool IKunFocuser::Handshake()
{
    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        // ESP8266 USB bridges reset the controller when the serial port opens.
        usleep(2200000);
        tcflush(PortFD, TCIOFLUSH);
    }

    char response[RESPONSE_SIZE] = { 0 };
    if (!sendCommand("#", response, sizeof(response), true) ||
            !IKunFocuserProtocol::isSupportedIdentity(response))
    {
        LOG_ERROR("IKunFocuser identification failed. Check the selected port, power, and firmware.");
        return false;
    }

    char identity[128] = { 0 };
    std::snprintf(identity, sizeof(identity), "%s", response);

    if (!sendCommand("V#", response, sizeof(response), true) ||
            !IKunFocuserProtocol::parseVersion(response, m_FirmwareVersion))
    {
        LOG_ERROR("IKunFocuser firmware version query failed.");
        return false;
    }

    char versionText[16] = { 0 };
    std::snprintf(versionText, sizeof(versionText), "%d", m_FirmwareVersion);
    FirmwareTP[FIRMWARE_VERSION].setText(versionText);
    FirmwareTP[CONTROLLER_MODEL].setText(IKunFocuserProtocol::modelForVersion(m_FirmwareVersion));

    if (!IKunFocuserProtocol::isSupportedVersion(m_FirmwareVersion))
    {
        LOGF_ERROR("Firmware %d is outside the supported release range. Upgrade the IKunFocuser firmware.",
                   m_FirmwareVersion);
        return false;
    }

    LOGF_INFO("Connected to %s, firmware %d.", identity, m_FirmwareVersion);
    return true;
}

bool IKunFocuser::sendCommand(const char *command, char *response, std::size_t responseSize, bool silent)
{
    if (response == nullptr || responseSize < 2)
        return false;

    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
        tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("CMD <%s>", command);

    int bytesWritten = 0;
    int rc = tty_write_string(PortFD, command, &bytesWritten);
    if (rc != TTY_OK)
    {
        if (!silent)
        {
            char errorMessage[MAXRBUF] = { 0 };
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("IKunFocuser write failed: %s.", errorMessage);
        }
        return false;
    }

    int bytesRead = 0;
    rc = tty_nread_section(PortFD, response, static_cast<int>(responseSize - 1), '#',
                           IO_TIMEOUT_SECONDS, &bytesRead);
    if (rc != TTY_OK)
    {
        if (!silent)
        {
            char errorMessage[MAXRBUF] = { 0 };
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("IKunFocuser read failed: %s.", errorMessage);
        }
        return false;
    }

    response[bytesRead] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    if (IKunFocuserProtocol::isErrorResponse(response))
    {
        if (!silent)
            LOGF_ERROR("IKunFocuser rejected command <%s> with response <%s>.", command, response);
        return false;
    }

    return true;
}

bool IKunFocuser::readMotionStatus(IKunFocuserProtocol::MotionStatus &status, bool silent)
{
    char response[RESPONSE_SIZE] = { 0 };
    if (!sendCommand("G#", response, sizeof(response), silent))
        return false;

    if (!IKunFocuserProtocol::parseMotionStatus(response, status))
    {
        if (!silent)
            LOGF_ERROR("Invalid IKunFocuser status response <%s>.", response);
        return false;
    }

    return true;
}

void IKunFocuser::applyMaximumPosition(uint32_t maximum)
{
    FocusMaxPosNP[0].setValue(maximum);

    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(maximum);
    FocusAbsPosNP[0].setStep(std::max(1.0, maximum / 50.0));

    FocusSyncNP[0].setMin(0);
    FocusSyncNP[0].setMax(maximum);
    FocusSyncNP[0].setStep(std::max(1.0, maximum / 50.0));

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(maximum / 2.0);
    FocusRelPosNP[0].setStep(std::max(1.0, maximum / 100.0));

    FocusAbsPosNP.updateMinMax();
    FocusSyncNP.updateMinMax();
    FocusRelPosNP.updateMinMax();
    SyncPresets(maximum);
}

bool IKunFocuser::readFullState()
{
    char response[RESPONSE_SIZE] = { 0 };
    if (!sendCommand("I#", response, sizeof(response)))
        return false;

    const std::string json(response);
    bool foundAny = false;
    int64_t integerValue = 0;
    double numberValue = 0;
    bool booleanValue = false;

    if (IKunFocuserProtocol::parseJsonInteger(json, "maxSteps", integerValue) &&
            integerValue >= 100 && integerValue <= 9999999)
    {
        applyMaximumPosition(static_cast<uint32_t>(integerValue));
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonInteger(json, "positionSteps", integerValue) && integerValue >= 0)
    {
        FocusAbsPosNP[0].setValue(integerValue);
        m_LastPosition = static_cast<uint32_t>(integerValue);
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonInteger(json, "acceleration", integerValue) &&
            integerValue >= 1 && integerValue <= 10000)
    {
        AccelerationNP[0].setValue(integerValue);
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonBoolean(json, "reversed", booleanValue))
    {
        FocusReverseSP.reset();
        FocusReverseSP[INDI_ENABLED].setState(booleanValue ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState(booleanValue ? ISS_OFF : ISS_ON);
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonBoolean(json, "hold", booleanValue))
    {
        HoldSP.reset();
        HoldSP[HOLD_ON].setState(booleanValue ? ISS_ON : ISS_OFF);
        HoldSP[HOLD_OFF].setState(booleanValue ? ISS_OFF : ISS_ON);
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonNumber(json, "lastTemp", numberValue) &&
            numberValue >= -55 && numberValue <= 125)
    {
        TemperatureNP[0].setValue(numberValue);
        foundAny = true;
    }

    if (IKunFocuserProtocol::parseJsonBoolean(json, "tempSensorPresent", booleanValue))
    {
        TemperatureNP.setState(booleanValue ? IPS_OK : IPS_IDLE);
        foundAny = true;
    }

    return foundAny;
}

void IKunFocuser::applyMotionStatus(const IKunFocuserProtocol::MotionStatus &status)
{
    const auto position = static_cast<uint32_t>(status.position);
    if (position != m_LastPosition)
    {
        FocusAbsPosNP[0].setValue(position);
        FocusAbsPosNP.apply();
        m_LastPosition = position;
    }

    if (status.moving)
    {
        if (FocusAbsPosNP.getState() != IPS_BUSY && FocusRelPosNP.getState() != IPS_BUSY)
        {
            FocusAbsPosNP.setState(IPS_BUSY);
            FocusAbsPosNP.apply();
        }
    }
    else
    {
        if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            LOG_INFO("IKunFocuser reached the requested position.");
        }
    }
}

IPState IKunFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    char command[64] = { 0 };
    char response[RESPONSE_SIZE] = { 0 };
    std::snprintf(command, sizeof(command), "M %u#", targetTicks);

    if (!sendCommand(command, response, sizeof(response)))
        return IPS_ALERT;

    IKunFocuserProtocol::MotionStatus status;
    if (!IKunFocuserProtocol::parseMotionStatus(response, status))
        return IPS_ALERT;

    m_CommunicationFailures = 0;
    return status.moving ? IPS_BUSY : IPS_OK;
}

IPState IKunFocuser::MoveRelFocuser(FocusDirection direction, uint32_t ticks)
{
    const uint32_t current = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());
    const uint32_t maximum = static_cast<uint32_t>(FocusMaxPosNP[0].getValue());
    uint32_t target = current;

    if (direction == FOCUS_INWARD)
        target = ticks > current ? 0 : current - ticks;
    else
        target = ticks > maximum - current ? maximum : current + ticks;

    return MoveAbsFocuser(target);
}

bool IKunFocuser::AbortFocuser()
{
    char response[RESPONSE_SIZE] = { 0 };
    return sendCommand("S#", response, sizeof(response));
}

bool IKunFocuser::SyncFocuser(uint32_t ticks)
{
    char command[64] = { 0 };
    char response[RESPONSE_SIZE] = { 0 };
    std::snprintf(command, sizeof(command), "P %u#", ticks);
    return sendCommand(command, response, sizeof(response));
}

bool IKunFocuser::ReverseFocuser(bool enabled)
{
    char response[RESPONSE_SIZE] = { 0 };
    return sendCommand(enabled ? "R 1#" : "R 0#", response, sizeof(response));
}

bool IKunFocuser::SetFocuserMaxPosition(uint32_t ticks)
{
    char command[64] = { 0 };
    char response[RESPONSE_SIZE] = { 0 };
    std::snprintf(command, sizeof(command), "D %u#", ticks);
    if (!sendCommand(command, response, sizeof(response)))
        return false;

    applyMaximumPosition(ticks);
    return true;
}

bool IKunFocuser::SetFocuserSpeed(int speed)
{
    char command[64] = { 0 };
    char response[RESPONSE_SIZE] = { 0 };
    std::snprintf(command, sizeof(command), "X %d#", speed);
    return sendCommand(command, response, sizeof(response));
}

bool IKunFocuser::setAcceleration(uint32_t acceleration)
{
    char command[64] = { 0 };
    char response[RESPONSE_SIZE] = { 0 };
    std::snprintf(command, sizeof(command), "A %u#", acceleration);
    return sendCommand(command, response, sizeof(response));
}

bool IKunFocuser::setHold(bool enabled)
{
    char response[RESPONSE_SIZE] = { 0 };
    return sendCommand(enabled ? "C 1#" : "C 0#", response, sizeof(response));
}

bool IKunFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) == 0 && AccelerationNP.isNameMatch(name))
    {
        const double previous = AccelerationNP[0].getValue();
        AccelerationNP.update(values, names, n);
        const auto requested = static_cast<uint32_t>(AccelerationNP[0].getValue());

        if (setAcceleration(requested))
            AccelerationNP.setState(IPS_OK);
        else
        {
            AccelerationNP[0].setValue(previous);
            AccelerationNP.setState(IPS_ALERT);
        }

        AccelerationNP.apply();
        return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool IKunFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) == 0 && HoldSP.isNameMatch(name))
    {
        const int previous = HoldSP.findOnSwitchIndex();
        HoldSP.update(states, names, n);
        const bool requested = HoldSP.findOnSwitchIndex() == HOLD_ON;

        if (setHold(requested))
            HoldSP.setState(IPS_OK);
        else
        {
            HoldSP.reset();
            HoldSP[previous >= 0 ? previous : HOLD_OFF].setState(ISS_ON);
            HoldSP.setState(IPS_ALERT);
        }

        HoldSP.apply();
        return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

void IKunFocuser::markMotionAlert()
{
    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusAbsPosNP.setState(IPS_ALERT);
        FocusRelPosNP.setState(IPS_ALERT);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
    }
}

void IKunFocuser::TimerHit()
{
    if (!isConnected())
        return;

    IKunFocuserProtocol::MotionStatus status;
    if (readMotionStatus(status, true))
    {
        m_CommunicationFailures = 0;
        applyMotionStatus(status);
    }
    else if (++m_CommunicationFailures >= MAX_COMMUNICATION_FAILURES)
    {
        markMotionAlert();
        m_CommunicationFailures = 0;
        LOG_ERROR("Lost communication with IKunFocuser while polling status.");
    }

    if (++m_TemperaturePollCounter >= 10)
    {
        m_TemperaturePollCounter = 0;
        const double previousMaximum = FocusMaxPosNP[0].getValue();
        const double previousAcceleration = AccelerationNP[0].getValue();
        const double previousTemperature = TemperatureNP[0].getValue();
        const IPState previousTemperatureState = TemperatureNP.getState();
        const int previousReverse = FocusReverseSP.findOnSwitchIndex();
        const int previousHold = HoldSP.findOnSwitchIndex();

        if (readFullState())
        {
            if (FocusMaxPosNP[0].getValue() != previousMaximum)
                FocusMaxPosNP.apply();
            if (AccelerationNP[0].getValue() != previousAcceleration)
                AccelerationNP.apply();
            if (TemperatureNP[0].getValue() != previousTemperature ||
                    TemperatureNP.getState() != previousTemperatureState)
                TemperatureNP.apply();
            if (FocusReverseSP.findOnSwitchIndex() != previousReverse)
                FocusReverseSP.apply();
            if (HoldSP.findOnSwitchIndex() != previousHold)
                HoldSP.apply();
        }
    }

    SetTimer(getCurrentPollingPeriod());
}
