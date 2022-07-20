/*
    Primaluca Labs Essato-Arco-Sesto Command Set
    For USB Control Specification Document Revision 3.3 published 2020.07.08

    Copyright (C) 2022 Jasem Mutlaq

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

    JM 2022.07.16: Major refactor to using json.h and update to Essato Arco
    Document protocol revision 3.3 (8th July 2022).
*/

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "primalucacommandset.h"
#include "indicom.h"
#include "indilogger.h"

namespace PrimalucaLabs
{

/******************************************************************************************************
 * Communication
*******************************************************************************************************/
bool Communication::sendCommand(const std::string &command, json *response)
{
    int tty_rc = TTY_OK;
    int nbytes_written = 0, nbytes_read = 0;
    tcflush(m_PortFD, TCIOFLUSH);
    LOGF_DEBUG("<REQ> %s", command.c_str());
    if ( (tty_rc = tty_write(m_PortFD, command.c_str(), command.length(), &nbytes_written)) == TTY_OK)
    {
        char errorMessage[MAXRBUF] = {0};
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    // Should we ignore response?
    if (response == nullptr)
        return true;

    char read_buf[DRIVER_LEN] = {0};
    if ( (tty_rc = tty_read_section(m_PortFD, read_buf, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read)) == TTY_OK)
    {
        char errorMessage[MAXRBUF] = {0};
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    LOGF_DEBUG("<RES> %s", read_buf);

    try
    {
        *response = json::parse(read_buf)["res"];
    }
    catch (json::exception &e)
    {
        // output exception information
        LOGF_ERROR("Error parsing device response %s id: %d", e.what(), e.id);
        return false;
    }

    return true;
}

template <typename T> bool Communication::motorGet(MotorType type, const std::string &parameter, T &value)
{
    auto motor = type == MOT_1 ? "MOT1" : "MOT2";
    json jsonRequest = {"req", {"get", {motor, ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"][motor][parameter].get_to(value);
        return true;
    }
    return false;

}

template <typename T> bool Communication::genericCommand(const std::string &motor, const std::string &type,
        const json &command, T *response)
{
    json jsonRequest = {"req", {type, {motor, command}}};
    if (response == nullptr)
        return sendCommand(jsonRequest);
    else
    {
        json jsonResponse;
        if (sendCommand(jsonRequest, &jsonResponse))
        {
            auto key = command.items().begin().key();
            return jsonResponse[type][motor][key].get_to(*response);
        }
    }

    return false;
}

template <typename T> bool Communication::motorSet(MotorType type, const json &command)
{
    json response;
    auto motor = type == MOT_1 ? "MOT1" : "MOT2";
    if (genericCommand(motor, "set", command, &response))
        return response == "done";
    return false;
}

template <typename T> bool Communication::motorCommand(MotorType type, const json &command)
{
    json response;
    auto motor = type == MOT_1 ? "MOT1" : "MOT2";
    if (genericCommand(motor, "cmd", command, &response))
        return response == "done";
    return false;
}

/******************************************************************************************************
 * Esatto
*******************************************************************************************************/
Esatto::Esatto(const std::string &name, int port) : m_DeviceName(name), m_PortFD(port)
{
    m_Communication.reset(new Communication(name, port));
}

bool Esatto::go(uint32_t position)
{
    return m_Communication->motorCommand(MOT_1, {"MOV_ABS", {"STEPS", position}});
}

bool Esatto::stop()
{
    return m_Communication->motorCommand(MOT_1, {"MOT_STOP", ""});
}

bool Esatto::fastMoveOut()
{
    return m_Communication->motorCommand(MOT_1, {"F_OUTW", ""});
}

bool Esatto::fastMoveIn()
{
    return m_Communication->motorCommand(MOT_1, {"F_INW", ""});
}

bool Esatto::getMaxPosition(uint32_t &position)
{
    return m_Communication->motorGet(MOT_1, "CAL_MAXPOS", position);
}

bool Esatto::isHallSensorDetected(bool &isDetected)
{
    int detected = 0;
    if (m_Communication->motorGet(MOT_1, "CAL_MAXPOS", detected))
    {
        isDetected = detected == 1;
        return true;
    }
    return false;
}

bool Esatto::storeAsMaxPosition()
{
    return m_Communication->motorCommand(MOT_1, {"CAL_FOCUSER", "StoreAsMaxPos"});
}

bool Esatto::storeAsMinPosition()
{
    return m_Communication->motorCommand(MOT_1, {"CAL_FOCUSER", "StoreAsMinPos"});
}

bool Esatto::goOutToFindMaxPos()
{
    return m_Communication->motorCommand(MOT_1, {"CAL_FOCUSER", "GoOutToFindMaxPos"});
}

bool Esatto::initCalibration()
{
    return m_Communication->motorCommand(MOT_1, {"CAL_FOCUSER", "Init"});
}

bool Esatto::getAbsolutePosition(uint32_t &position)
{
    return m_Communication->motorGet(MOT_1, "ABS_POS", position);
}

bool Esatto::getCurrentSpeed(uint32_t &speed)
{
    return m_Communication->motorGet(MOT_1, "SPEED", speed);
}

bool Esatto::getMotorTemp(double &value)
{
    return m_Communication->motorGet(MOT_1, "NTC_T", value);
}

bool Esatto::getExternalTemp(double &value)
{
    return m_Communication->motorGet(MOT_1, "EXT_T", value);
}

bool Esatto::getVoltageIn(double &value)
{
    return m_Communication->motorGet(MOT_1, "VIN_12V", value);
}

bool Esatto::applyMotorPreset(const std::string &name)
{
    return m_Communication->motorCommand(MOT_1, {"RUNPRESET", name});
}

bool Esatto::setMotorUserPreset(uint32_t index, const MotorRates &rates, const MotorCurrents &currents)
{
    auto name = std::string("RUNPRESET_") + std::to_string(index);
    auto user = std::string("user_") + std::to_string(index);

    json preset = {{"RP_NAME", user},
        {"M1ACC", rates.accRate},
        {"M1DEC", rates.decRate},
        {"M1SPD", rates.runSpeed},
        {"M1CACC", currents.accCurrent},
        {"M1CDEC", currents.decCurrent},
        {"M1CSPD", currents.runCurrent},
        {"M1CHOLD", currents.holdCurrent}
    };

    json jsonRequest = {"req", {"set", {name, preset}}};
    return m_Communication->sendCommand(jsonRequest);
}

bool Esatto::getMotorSettings(MotorRates &rates, MotorCurrents &currents, bool &motorHoldActive)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;

    if (m_Communication->sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["FnRUN_ACC"].get_to(rates.accRate);
        jsonResponse["get"]["MOT1"]["FnRUN_DEC"].get_to(rates.decRate);
        jsonResponse["get"]["MOT1"]["FnRUN_SPD"].get_to(rates.runSpeed);

        jsonResponse["get"]["MOT1"]["FnRUN_CURR_ACC"].get_to(currents.accCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_DEC"].get_to(currents.decCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_SPD"].get_to(currents.runCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_HOLD"].get_to(currents.holdCurrent);
        jsonResponse["get"]["MOT1"]["HOLDCURR_STATUS"].get_to(motorHoldActive);
        return true;
    }
    return false;
}

bool Esatto::setMotorRates(const MotorRates &rates)
{
    json jsonRates =
    {
        {"FnRUN_ACC", rates.accRate},
        {"FnRUN_ACC", rates.accRate},
        {"FnRUN_ACC", rates.accRate},
    };

    json jsonRequest = {"req", {"set", {"MOT1", jsonRates}}};
    return m_Communication->sendCommand(jsonRequest);
}

bool Esatto::setMotorCurrents(const MotorCurrents &currents)
{
    json jsonRates =
    {
        {"FnRUN_CURR_ACC", currents.accCurrent},
        {"FnRUN_CURR_DEC", currents.decCurrent},
        {"FnRUN_CURR_SPD", currents.runCurrent},
        {"FnRUN_CURR_HOLD", currents.holdCurrent},
    };

    json jsonRequest = {"req", {"set", {"MOT1", jsonRates}}};
    return m_Communication->sendCommand(jsonRequest);
}

bool Esatto::setMotorHold(bool hold)
{
    json jsonHold = {"HOLDCURR_STATUS", hold ? 1 : 0};
    json jsonRequest = {"req", {"set", {"MOT1", jsonHold}}};
    return m_Communication->sendCommand(jsonRequest);
}

/******************************************************************************************************
 * Arco
*******************************************************************************************************/
Arco::Arco(const std::string &name, int port) : m_DeviceName(name), m_PortFD(port)
{
    m_Communication.reset(new Communication(name, port));
}

bool Arco::isEnabled()
{
    int enabled = 0;
    if (m_Communication->motorGet(MOT_2, "ARCO", enabled))
        return enabled == 1;
    return false;
}


bool Arco::getAbsolutePosition(Units unit, double &value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command["POSITION"] = "DEG";
            break;
        case UNIT_ARCSECS:
            command["POSITION"] = "ARCSEC";
            break;
        case UNIT_STEPS:
            command["POSITION"] = "STEPS";
            break;
    }

    json jsonRequest = {"req", {"get", {"MOT2", command}}};
    json jsonResponse;
    if (m_Communication->sendCommand(jsonRequest, &jsonResponse))
    {

        std::string position = jsonResponse["get"]["MOT2"]["POSITION"];
        sscanf(position.c_str(), "%lf", &value);
        return true;
    }

    return false;
}

bool Arco::moveAbsolutePoition(Units unit, double value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command["MOVE"] = {"DEG", value};
            break;
        case UNIT_ARCSECS:
            command["MOVE"] = {"ARCSEC", value};
            break;
        case UNIT_STEPS:
            command["MOVE"] = {"STEPS", value};
            break;
    }

    json jsonRequest = {"req", {"cmd", {"MOT2", command}}};
    return m_Communication->sendCommand(jsonRequest);
}

bool Arco::sync(Units unit, double value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command["SYNC_POS"] = {"DEG", value};
            break;
        case UNIT_ARCSECS:
            command["SYNC_POS"] = {"ARCSEC", value};
            break;
        case UNIT_STEPS:
            command["SYNC_POS"] = {"STEPS", value};
            break;
    }

    json jsonRequest = {"req", {"cmd", {"MOT2", command}}};
    return m_Communication->sendCommand(jsonRequest);
}

bool Arco::isBusy()
{
    json status;
    if (m_Communication->motorGet(MOT_2, "STATUS", status))
    {
        return (status["BUSY"].get<int>() == 1);
    }
    return false;
}

bool Arco::stop()
{
    return m_Communication->motorCommand(MOT_2, {"MOT_STOP", ""});
}


bool Arco::calibrate()
{
    return m_Communication->motorSet(MOT_2, {"CAL_STATUS", "exec"});
}

bool Arco::isCalibrating()
{
    std::string value;
    if (m_Communication->motorGet(MOT_2, "CAL_STATUS", value))
    {
        return value == "exec";
    }
    return false;
}

bool Arco::reverse(bool enabled)
{
    return m_Communication->motorCommand(MOT_2, {"REVERSE", enabled ? 1 : 0});
}

bool Arco::isReversed()
{
    int value;
    if (m_Communication->motorGet(MOT_2, "REVERSE", value))
    {
        return value == 1;
    }
    return false;
}
}
