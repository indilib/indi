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
#include <regex>

#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "primalucacommandset.h"
#include "indicom.h"
#include "indilogger.h"

namespace PrimalucaLabs
{

std::vector<std::string> split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

/******************************************************************************************************
 * Communication
*******************************************************************************************************/
bool Communication::sendRequest(const json &command, json *response)
{
    int tty_rc = TTY_OK;
    int nbytes_written = 0, nbytes_read = 0;
    tcflush(m_PortFD, TCIOFLUSH);

    std::string output = command.dump();
    LOGF_DEBUG("<REQ> %s", output.c_str());
    if ( (tty_rc = tty_write(m_PortFD, output.c_str(), output.length(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF] = {0};
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    // Should we ignore response?
    if (response == nullptr)
        return true;

    char read_buf[DRIVER_LEN];
    do
    {
        memset(read_buf, 0, sizeof(read_buf));
        if ( (tty_rc = tty_read_section(m_PortFD, read_buf, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errorMessage[MAXRBUF] = {0};
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errorMessage);
            return false;
        }
    }
    while (strncmp(read_buf, "ERR:", 4) == 0 && (LOGF_WARN("%s", read_buf), true));

    LOGF_DEBUG("<RES> %s", read_buf);

    try
    {
        if (strstr(read_buf, "Error:"))
        {
            LOGF_ERROR("Required %s failed: %s", command.dump().c_str(), read_buf);
            return false;
        }
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

bool Communication::getStringAsDouble(NodeType type, const std::string &parameter, double &value)
{
    std::string response;
    if (get(type, parameter, response))
    {
        sscanf(response.c_str(), "%lf", &value);
        return true;
    }
    return false;
}

template <typename T> bool Communication::get(NodeType type, const std::string &parameter, T &value)
{
    std::string node;
    switch (type)
    {
        case MOT_1:
            node = "MOT1";
            break;
        case MOT_2:
            node = "MOT2";
            break;
        default:
            break;
    }

    json jsonRequest = {{parameter, ""}};
    return genericRequest(node, "get", jsonRequest, &value);
}

bool Communication::set(NodeType type, const json &value)
{
    std::string node;
    switch (type)
    {
        case MOT_1:
            node = "MOT1";
            break;
        case MOT_2:
            node = "MOT2";
            break;
        default:
            break;
    }

    std::string isDone;
    if (genericRequest(node, "set", value, &isDone))
        return isDone == "done";
    return false;
}

template <typename T> bool Communication::genericRequest(const std::string &node, const std::string &type,
        const json &command, T *response)
{
    json jsonRequest;
    if (node.empty())
        jsonRequest = {{"req", {{type, command}}}};
    else
        jsonRequest = {{"req", {{type, {{node, command}}}}}};
    if (response == nullptr)
        return sendRequest(jsonRequest);
    else
    {
        json jsonResponse;
        if (sendRequest(jsonRequest, &jsonResponse))
        {
            // There is no command.items().last() so we have to iterate all
            std::string key;
            std::string flat = command.flatten().items().begin().key();
            std::vector<std::string> keys = split(flat, "/");
            key = keys.back();
            //            for (auto &oneItem : command.items())
            //                key = oneItem.key();
            try
            {
                if (node.empty())
                {
                    if (jsonResponse[type].contains(key))
                        jsonResponse[type][key].get_to(*response);
                    else if (jsonResponse[type].contains("ERROR"))
                    {
                        std::string error = jsonResponse[type]["ERROR"];
                        LOGF_ERROR("Error: %s", error.c_str());
                        return false;
                    }
                }
                else
                {
                    if (jsonResponse[type][node].contains(key))
                        jsonResponse[type][node][key].get_to(*response);
                    else if (jsonResponse[type][node].contains("ERROR"))
                    {
                        std::string error = jsonResponse[type][node]["ERROR"];
                        LOGF_ERROR("Error: %s", error.c_str());
                        return false;
                    }
                }


            }
            catch (json::exception &e)
            {
                // output exception information
                LOGF_ERROR("Failed Request: %s\nResponse: %s\nException: %s id: %d", jsonRequest.dump().c_str(),
                           jsonResponse.dump().c_str(),
                           e.what(), e.id);
                return false;
            }
            return true;
        }
    }

    return false;
}

template <typename T> bool Communication::command(NodeType type, const json &jsonCommand)
{
    std::string node;
    switch (type)
    {
        case MOT_1:
            node = "MOT1";
            break;
        case MOT_2:
            node = "MOT2";
            break;
        default:
            break;
    }
    std::string response;
    if (genericRequest(node, "cmd", jsonCommand, &response))
        return response == "done";
    return false;
}

/******************************************************************************************************
 * Common Focuser functions between SestoSenso2 & Esatto
*******************************************************************************************************/
Focuser::Focuser(const std::string &name, int port)
{
    m_Communication.reset(new Communication(name, port));
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::goAbsolutePosition(uint32_t position)
{
    return m_Communication->command(MOT_1, {{"MOVE_ABS", {{"STEP", position}}}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::stop()
{
    return m_Communication->command(MOT_1, {{"MOT_STOP", ""}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::fastMoveOut()
{
    return m_Communication->command(MOT_1, {{"F_OUTW", ""}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::fastMoveIn()
{
    return m_Communication->command(MOT_1, {{"F_INW", ""}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getMaxPosition(uint32_t &position)
{
    return m_Communication->get(MOT_1, "CAL_MAXPOS", position);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::isHallSensorDetected(bool &isDetected)
{
    int detected = 0;
    if (m_Communication->get(MOT_1, "HSENDET", detected))
    {
        isDetected = detected == 1;
        return true;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getAbsolutePosition(uint32_t &position)
{
    return m_Communication->get(MOT_1, "ABS_POS", position);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getCurrentSpeed(uint32_t &speed)
{
    return m_Communication->get(MOT_1, "SPEED", speed);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getStatus(json &status)
{
    return m_Communication->get(MOT_1, "STATUS", status);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::isBusy()
{
    json status;
    if (m_Communication->get(MOT_1, "STATUS", status))
    {
        return status["BUSY"] == 1;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getMotorTemp(double &value)
{
    return m_Communication->getStringAsDouble(MOT_1, "NTC_T", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getExternalTemp(double &value)
{
    return m_Communication->getStringAsDouble(GENERIC_NODE, "EXT_T", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getSerialNumber(std::string &response)
{
    return m_Communication->get(GENERIC_NODE, "SN", response);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getVoltage12v(double &value)
{
    return m_Communication->getStringAsDouble(GENERIC_NODE, "VIN_12V", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getFirmwareVersion(std::string &response)
{
    json versions;
    if (m_Communication->get(GENERIC_NODE, "SWVERS", versions))
    {
        versions["SWAPP"].get_to(response);
        return true;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::setBacklash(uint32_t steps)
{
    return m_Communication->set(MOT_1, {{"BKLASH", steps}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Focuser::getBacklash(uint32_t &steps)
{
    return m_Communication->get(MOT_1, "BKLASH", steps);
}


/******************************************************************************************************
 * SestoSenso2 functions
*******************************************************************************************************/
SestoSenso2::SestoSenso2(const std::string &name, int port) : Focuser(name, port)
{
}

bool SestoSenso2::getModel(std::string &model)
{
    return m_Communication->get(GENERIC_NODE, "MODNAME", model);
}

/******************************************************************************************************
 * Get SubModel for SestoSenso3 variants
*******************************************************************************************************/
bool SestoSenso2::getSubModel(std::string &submodel)
{
    json jsonRequest = {{"req", {{"srv", {{"GET_MODEL_SUBMODEL", ""}}}}}};
    json jsonResponse;
    
    if (m_Communication->sendRequest(jsonRequest, &jsonResponse))
    {
        try
        {
            std::string response;
            jsonResponse["srv"]["GET_MODEL_SUBMODEL"].get_to(response);
            
            // Parse: "Model = SESTOSENSO3, SubModel = SESTOSENSO3SC, ARCO = Not enabled"
            size_t subModelPos = response.find("SubModel = ");
            if (subModelPos != std::string::npos)
            {
                subModelPos += 11; // Length of "SubModel = "
                size_t commaPos = response.find(",", subModelPos);
                if (commaPos != std::string::npos)
                {
                    submodel = response.substr(subModelPos, commaPos - subModelPos);
                }
                else
                {
                    // No comma found, take until end of string
                    submodel = response.substr(subModelPos);
                }
                return true;
            }
        }
        catch (json::exception &e)
        {
            LOGF_ERROR("Error parsing submodel response: %s", e.what());
        }
    }
    return false;
}

bool SestoSenso2::storeAsMaxPosition()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "StoreAsMaxPos-Manual"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::storeAsMinPosition()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "StoreAsMinPos"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::goOutToFindMaxPos()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "GoOutToFindMaxPos"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::initCalibration()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "Init-Manual"}});
}

// SestoSenso3 Specific Calibration
bool SestoSenso2::initSemiAutoCalibration()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "Init"}});
}

bool SestoSenso2::goInToFindMinPos()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "GoInToFindMinPos"}});
}

bool SestoSenso2::stopMotor()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "StopMotor"}});
}

bool SestoSenso2::moveIn(uint32_t steps)
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "MoveIn-" + std::to_string(steps)}});
}

bool SestoSenso2::moveOut(uint32_t steps)
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "MoveOut-" + std::to_string(steps)}});
}

bool SestoSenso2::goOutToFindMaxPosSemiAuto()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "GoOutToFindMaxPos"}});
}

bool SestoSenso2::storeAsMaxPosSemiAuto()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "StoreAsMaxPos"}});
}

bool SestoSenso2::startAutoCalibration()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "start_auto_cal"}});
}

bool SestoSenso2::stopCalibration()
{
    return m_Communication->command(MOT_1, {{"CAL_FOCUSER", "stop_calib"}});
}

// SestoSenso3 Recovery Delay
bool SestoSenso2::setRecoveryDelay(int32_t delay)
{
    return m_Communication->set(GENERIC_NODE, {{"RECOVER_DELAY", delay}});
}

bool SestoSenso2::getRecoveryDelay(int32_t &delay)
{
    return m_Communication->get(GENERIC_NODE, "RECOVER_DELAY", delay);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::applyMotorPreset(const std::string &name)
{
    return m_Communication->command(GENERIC_NODE, {{"RUNPRESET", name}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::setMotorUserPreset(uint32_t index, const MotorRates &rates, const MotorCurrents &currents)
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

    return m_Communication->set(MOT_1, {{name, preset}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::getMotorSettings(MotorRates &rates, MotorCurrents &currents, bool &motorHoldActive)
{
    json jsonRequest = {{"req", {{"get", {{"MOT1", { {"FnRUN_ACC", ""},
                                {"FnRUN_DEC", ""}, {"FnRUN_SPD", ""}, {"FnRUN_CURR_ACC", ""},
                                {"FnRUN_CURR_DEC", ""}, {"FnRUN_CURR_SPD", ""}, {"FnRUN_CURR_HOLD", ""}, {"HOLDCURR_STATUS", ""}
                            }
                        }
                    }
                }
            }
        }
    };
    json jsonResponse;

    if (m_Communication->sendRequest(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["FnRUN_ACC"].get_to(rates.accRate);
        jsonResponse["get"]["MOT1"]["FnRUN_DEC"].get_to(rates.decRate);
        jsonResponse["get"]["MOT1"]["FnRUN_SPD"].get_to(rates.runSpeed);

        jsonResponse["get"]["MOT1"]["FnRUN_CURR_ACC"].get_to(currents.accCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_DEC"].get_to(currents.decCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_SPD"].get_to(currents.runCurrent);
        jsonResponse["get"]["MOT1"]["FnRUN_CURR_HOLD"].get_to(currents.holdCurrent);

        int status = 0;
        jsonResponse["get"]["MOT1"]["HOLDCURR_STATUS"].get_to(status);
        motorHoldActive = ( status == 1 );
        return true;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::setMotorRates(const MotorRates &rates)
{
    json jsonRates =
    {
        {"FnRUN_ACC", rates.accRate},
        {"FnRUN_ACC", rates.accRate},
        {"FnRUN_ACC", rates.accRate},
    };

    return m_Communication->set(MOT_1, jsonRates);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::setMotorCurrents(const MotorCurrents &currents)
{
    json jsonRates =
    {
        {"FnRUN_CURR_ACC", currents.accCurrent},
        {"FnRUN_CURR_DEC", currents.decCurrent},
        {"FnRUN_CURR_SPD", currents.runCurrent},
        {"FnRUN_CURR_HOLD", currents.holdCurrent},
    };

    return m_Communication->set(MOT_1, jsonRates);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool SestoSenso2::setMotorHold(bool hold)
{
    return m_Communication->set(MOT_1, {{"HOLDCURR_STATUS", hold ? 1 : 0}});
}

/******************************************************************************************************
 * Esatto functions
*******************************************************************************************************/
Esatto::Esatto(const std::string &name, int port) : Focuser(name, port) {}
bool Esatto::setBacklash(uint32_t steps)
{
    return m_Communication->set(MOT_1, {{"BKLASH", steps}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Esatto::getBacklash(uint32_t &steps)
{
    return m_Communication->get(MOT_1, "BKLASH", steps);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Esatto::getVoltageUSB(double &value)
{
    return m_Communication->getStringAsDouble(GENERIC_NODE, "VIN_USB", value);
}

bool Esatto::getModel(std::string &model)
{
    return m_Communication->get(GENERIC_NODE, "MODNAME", model);
}

/******************************************************************************************************
 * Arco
*******************************************************************************************************/
Arco::Arco(const std::string &name, int port)
{
    m_Communication.reset(new Communication(name, port));
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::setEnabled(bool enabled)
{
    return m_Communication->set(GENERIC_NODE, {{"ARCO", enabled ? 1 : 0}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::isEnabled()
{
    int enabled = 0;
    if (m_Communication->get(GENERIC_NODE, "ARCO", enabled))
        return enabled == 1;
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::getAbsolutePosition(Units unit, double &value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command = {{"POSITION_DEG", ""}};
            break;
        case UNIT_ARCSECS:
            command = {{"POSITION_ARCSEC", ""}};
            break;
        case UNIT_STEPS:
            command = {{"POSITION_STEP", ""}};
            break;
    }

    return m_Communication->genericRequest("MOT2", "get", command, &value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::moveAbsolutePoition(Units unit, double value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command = {{"MOVE_ABS", {{"DEG", value}}}};
            break;
        case UNIT_ARCSECS:
            command = {{"MOVE_ABS", {{"ARCSEC", value}}}};
            break;
        case UNIT_STEPS:
            command = {{"MOVE_ABS", {{"STEP", static_cast<int>(value)}}}};
            break;
    }

    return m_Communication->command(MOT_2, command);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::sync(Units unit, double value)
{
    json command;
    switch (unit)
    {
        case UNIT_DEGREES:
            command = {{"SYNC_POS", {{"DEG", value}}}};
            break;
        case UNIT_ARCSECS:
            command = {{"SYNC_POS", {{"ARCSEC", value}}}};
            break;
        case UNIT_STEPS:
            command = {{"SYNC_POS", {{"STEP", static_cast<int>(value)}}}};
            break;
    }

    return m_Communication->command(MOT_2, command);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::isBusy()
{
    json status;
    if (m_Communication->get(MOT_2, "STATUS", status))
    {
        return status["BUSY"] == 1;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::getStatus(json &status)
{
    return m_Communication->get(MOT_2, "STATUS", status);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::stop()
{
    return m_Communication->command(MOT_2, {{"MOT_STOP", ""}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::calibrate()
{
    return m_Communication->set(MOT_2, {{"CAL_STATUS", "exec"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::isCalibrating()
{
    std::string value;
    if (m_Communication->get(MOT_2, "CAL_STATUS", value))
    {
        return value == "exec";
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::reverse(bool enabled)
{
    return m_Communication->set(MOT_2, {{"REVERSE", enabled ? 1 : 0}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::isReversed()
{
    int value;
    if (m_Communication->get(MOT_2, "REVERSE", value))
    {
        return value == 1;
    }
    return false;
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::getSerialNumber(std::string &response)
{
    return m_Communication->get(GENERIC_NODE, "ARCO_SN", response);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::getFirmwareVersion(std::string &response)
{
    // JM 2022.07.22: Apparently not possible now in protocol.
    response = "NA";
    return true;
}

bool Arco::getModel(std::string &model)
{
    return m_Communication->get(GENERIC_NODE, "MODNAME", model);
}


/******************************************************************************************************
 *
*******************************************************************************************************/
bool Arco::getMotorInfo(json &info)
{
    json jsonRequest = {{"req", {{"get", {{"MOT2", ""}}}}}};
    return m_Communication->sendRequest(jsonRequest, &info);
}

/******************************************************************************************************
 * GIOTTO
*******************************************************************************************************/
GIOTTO::GIOTTO(const std::string &name, int port)
{
    m_Communication.reset(new Communication(name, port));
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::getModel(std::string &model)
{
    return m_Communication->get(GENERIC_NODE, "MODNAME", model);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::setLightEnabled(bool enabled)
{
    json jsonRequest = {{"req", {{"set", {{"LIGHT", enabled ? 1 : 0}}}}}};
    return m_Communication->sendRequest(jsonRequest);
}


/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::isLightEnabled()
{
    int value;
    if (m_Communication->get(GENERIC_NODE, "LIGHT", value))
    {
        return value == 1;
    }
    return false;
}
/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::getMaxBrightness(uint16_t &value)
{
    return m_Communication->get(GENERIC_NODE, "MAX_BRIGHTNESS", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::setBrightness(uint16_t value)
{
    return m_Communication->set(GENERIC_NODE, {{"BRIGHTNESS", value}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool GIOTTO::getBrightness(uint16_t &value)
{
    return m_Communication->get(GENERIC_NODE, "BRIGHTNESS", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
ALTO::ALTO(const std::string &name, int port)
{
    m_Communication.reset(new Communication(name, port));
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::getModel(std::string &model)
{
    return m_Communication->get(GENERIC_NODE, "MODNAME", model);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::getStatus(json &status)
{
    return m_Communication->get(MOT_1, "STATUS", status);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::Park()
{
    return setPosition(0);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::UnPark()
{
    return setPosition(100);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::setPosition(uint8_t value)
{
    return m_Communication->set(MOT_1, {{"POSITION", value}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::getPosition(uint8_t &value)
{
    return m_Communication->get(MOT_1, "POSITION", value);
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::stop()
{
    return m_Communication->command(MOT_1, {{"MOT_STOP", ""}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::initCalibration()
{
    return m_Communication->command(MOT_1, {{"CAL_ALTO", "Init"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::close(bool fast)
{
    return m_Communication->command(MOT_1, {{"CAL_ALTO", fast ? "Close_Fast" : "Close_Slow"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::open(bool fast)
{
    return m_Communication->command(MOT_1, {{"CAL_ALTO", fast ? "Open_Fast" : "Open_Slow"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::storeClosedPosition()
{
    return m_Communication->command(MOT_1, {{"CAL_ALTO", "StoreAsClosedPos"}});
}

/******************************************************************************************************
 *
*******************************************************************************************************/
bool ALTO::storeOpenPosition()
{
    return m_Communication->command(MOT_1, {{"CAL_ALTO", "StoreAsMaxOpenPos"}});
}

}
