/*
    Primaluca Labs Essato-Arco Focuser+Rotator Driver

    Copyright (C) 2020 Piotr Zyziuk
    Copyright (C) 2020-2022 Jasem Mutlaq

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

#include "esattoarco.h"

#include "indicom.h"
#include "json.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>
#include <sys/ioctl.h>

static std::unique_ptr<EssatoArco> essatoarco(new EssatoArco());

static const char *MOTOR_TAB  = "Motor";
static const char *ENVIRONMENT_TAB  = "Environment";
static const char *ROTATOR_TAB = "Rotator";

// Settings names for the default motor settings presets
const char *MOTOR_PRESET_NAMES[] = { "light", "medium", "slow" };

namespace CommandSet
{

bool sendCommand(const std::string &command, json *response)
{
    int tty_rc = TTY_OK;
    int nbytes_written = 0, nbytes_read = 0;
    tcflush(PortFD, TCIOFLUSH);
    LOGF_DEBUG("<REQ> %s", command.c_str());
    if ( (tty_rc = tty_write(PortFD, command.c_str(), command.length(), &nbytes_written)) == TTY_OK)
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
    if ( (tty_rc = tty_read_section(PortFD, read_buf, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read)) == TTY_OK)
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

namespace Essato
{
bool stop()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"MOT_STOP", ""}}}};
    return sendCommand(jsonRequest);
}

bool fastMoveOut()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"F_OUTW", ""}}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        return jsonResponse["get"]["MOT1"]["F_OUTW"] == "done";
    }
    return false;
}

bool fastMoveIn()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"F_INW", ""}}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        return jsonResponse["get"]["MOT1"]["F_INW"] == "done";
    }
    return false;
}

bool getMaxPosition(uint32_t &position)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["CAL_MAXPOS"].get_to(position);
        return true;
    }
    return false;
}

bool isHallSensorDetected(bool &isDetected)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        int detected = jsonResponse["get"]["MOT1"]["HSENDET"].get<int>();
        isDetected = detected == 1;
        return true;
    }
    return false;
}

bool storeAsMaxPosition()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"CAL_FOCUSER", "StoreAsMaxPos"}}}};
    return sendCommand(jsonRequest);
}

bool storeAsMinPosition()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"CAL_FOCUSER", "StoreAsMinPos"}}}};
    return sendCommand(jsonRequest);
}

bool goOutToFindMaxPos()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"CAL_FOCUSER", "GoOutToFindMaxPos"}}}};
    return sendCommand(jsonRequest);
}

bool initCalibration()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"CAL_FOCUSER", "Init"}}}};
    return sendCommand(jsonRequest);
}

bool getAbsolutePosition(uint32_t &position)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["ABS_POS"].get_to(position);
        return true;
    }
    return false;
}

bool getCurrentSpeed(uint32_t &speed)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["SPEED"].get_to(speed);
        return true;
    }
    return false;
}

bool getMotorTemp(double &value)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["NTC_T"].get_to(value);
        return true;
    }
    return false;
}

bool getExternalTemp(double &value)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["EXT_T"].get_to(value);
        return true;
    }
    return false;
}

bool getVoltageIn(double &value)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        jsonResponse["get"]["MOT1"]["VIN_12V"].get_to(value);
        return true;
    }
    return false;
}

bool applyMotorPreset(const std::string &name)
{
    json jsonRequest = {"req", {"cmd", {"RUNPRESET", name}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, &jsonResponse))
    {
        return jsonResponse["get"]["RUNPRESET"] == "done";
    }
    return false;
}

bool saveMotorUserPreset(uint32_t index, const MotorRates &rates, const MotorCurrents &currents)
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

    json jsonRequest = {"req", {"cmd", {name, preset}}};
    return sendCommand(jsonRequest);
}

bool getMotorSettings(MotorRates &rates, MotorCurrents &currents, bool &motorHoldActive)
{
    json jsonRequest = {"req", {"get", {"MOT1", ""}}};
    json jsonResponse;

    if (sendCommand(jsonRequest, &jsonResponse))
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

bool setMotorRates(const MotorRates &rates)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), MOTOR_RATES_CMD, mr.accRate, mr.runSpeed, mr.decRate);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}

//constexpr char MOTOR_CURRENTS_CMD[] =
//    "{\"req\":{\"set\":{\"MOT1\":{"
//    "\"FnRUN_CURR_ACC\":%u,"
//    "\"FnRUN_CURR_SPD\":%u,"
//    "\"FnRUN_CURR_DEC\":%u,"
//    "\"FnRUN_CURR_HOLD\":%u"
//    "}}}}";

bool setMotorCurrents(const MotorCurrents &current)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), MOTOR_CURRENTS_CMD, current.accCurrent, current.runCurrent, current.decCurrent,
             current.holdCurrent);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}

bool setMotorHold(bool hold)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"set\":{\"MOT1\":{\"HOLDCURR_STATUS\":%u}}}}", hold ? 1 : 0);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}

}

namespace Arco
{
bool getARCO(char *res)
{
    return sendCommand("{\"req\":{\"get\":{\"ARCO\":\"\"}}}", "ARCO", res);
}

//bool getArcoAbsPos(char *res)
//{

//return sendCmd("{\"req\":{\"get\":{\"MOT2\":{\"ABS_POS\":\"DEG\"}}}}","ABS_POS", res);

//}

double getArcoAbsPos()
{

    char res[SESTO_LEN] = {0};
    std::string buf;
    bool rc = sendCommand("{\"req\":{\"get\":{\"MOT2\":{\"ABS_POS\":\"DEG\"}}}}", "ABS_POS", res);
    if(rc == true)
    {
        std::string my_string(res);
        buf = my_string.substr(0, my_string.length() - 5);

    }
    return std::stof(buf);
}


double getArcoPosition()
{

    char res[SESTO_LEN] = {0};
    std::string buf;
    bool rc = sendCommand("{\"req\":{\"get\":{\"MOT2\":{\"POSITION\":\"DEG\"}}}}", "POSITION", res);
    if(rc == true)
    {
        std::string my_string(res);
        buf = my_string.substr(0, my_string.length() - 5);

    }
    return std::stof(buf);
}



bool setArcoAbsPos(double targetAngle, char *res)
{
    char cmd[SESTO_LEN] = {0};
    if(targetAngle > getArcoAbsPos() + 180.)
    {
        targetAngle -= 360.0;
    }

    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"MOT2\" :{\"GOTO\":{\"DEG\":%f}}}}}", targetAngle);

    LOGF_INFO("Rotator moving to: %f", targetAngle);
    return sendCommand(cmd, "DEG", res);
}

bool isArcoBusy()
{
    char res[SESTO_LEN] = {0};

    if(!sendCommand("{\"req\":{\"get\":{\"MOT2\":\"\"}}}", "BUSY", res))
    {
        LOG_INFO("Could not check if Arco is busy.");
        return true;
    }

    int busy = std::stoi(res);
    if(!(busy == 0)) return true;

    return false;
}

bool stopArco()
{
    char res[SESTO_LEN] = {0};
    return sendCommand("{\"req\":{\"cmd\":{\"MOT2\":{\"MOT_STOP\":\"\"}}}}", "MOT_STOP", res);
}


bool syncArco(double angle)
{
    char res[SESTO_LEN] = {0};
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"MOT2\" :{\"SYNC_POS\":{\"DEG\":%f}}}}}", angle);
    return sendCommand(cmd, "DEG", res);
}

bool calArco()
{
    char res[SESTO_LEN] = {0};
    return sendCommand("{\"req\":{\"set\":{\"MOT2\":{\"CAL_STATUS\":\"exec\"}}}}", "CAL_STATUS", res);
}

bool isArcoCalibrating()
{
    char res[SESTO_LEN] = {0};
    if(!sendCommand("{\"req\":{\"get\":{\"MOT2\":{\"CAL_STATUS\":\"\"}}}}", "CAL_STATUS", res))
    {
        LOG_INFO("Could not check if Arco is Calibrating.");
        return true;
    }
    std::string result(res);
    if(result == "exec") return true;
    return false;

}


}
}

EssatoArco::EssatoArco() : RotatorInterface(this)
{
    setVersion(0, 8);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_HAS_BACKLASH | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);

    m_MotionProgressTimer.callOnTimeout(std::bind(&EssatoArco::checkMotionProgressCallback, this));
    m_MotionProgressTimer.setSingleShot(true);

    m_HallSensorTimer.callOnTimeout(std::bind(&EssatoArco::checkHallSensorCallback, this));
    m_HallSensorTimer.setSingleShot(true);
    m_HallSensorTimer.setInterval(1000);
}

bool EssatoArco::initProperties()
{
    INDI::Focuser::initProperties();

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 10000;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 0;

    setConnectionParams();

    // Firmware information
    IUFillText(&FirmwareT[FIRMWARE_SN], "SERIALNUMBER", "Serial Number", "");
    IUFillText(&FirmwareT[FIRMWARE_VERSION], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 2, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 0,
                     IPS_IDLE);

    // Voltage Information
    IUFillNumber(&VoltageInN[0], "VOLTAGEIN", "Volts", "%.2f", 0, 100, 0., 0.);
    IUFillNumberVector(&VoltageInNP, VoltageInN, 1, getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[TEMPERATURE_MOTOR], "TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_EXTERNAL], "TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 2, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Current Speed
    IUFillNumber(&SpeedN[0], "SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser calibration
    IUFillText(&CalibrationMessageT[0], "CALIBRATION", "Calibration stage", "Press START to begin the Calibration.");
    IUFillTextVector(&CalibrationMessageTP, CalibrationMessageT, 1, getDeviceName(), "CALIBRATION_MESSAGE", "Calibration",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Calibration
    IUFillSwitch(&CalibrationS[CALIBRATION_START], "CALIBRATION_START", "Start", ISS_OFF);
    IUFillSwitch(&CalibrationS[CALIBRATION_NEXT], "CALIBRATION_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&CalibrationSP, CalibrationS, 2, getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillText(&BacklashMessageT[0], "BACKLASH", "Backlash stage", "Press START to measure backlash.");
    IUFillTextVector(&BacklashMessageTP, BacklashMessageT, 1, getDeviceName(), "BACKLASH_MESSAGE", "Backlash",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Backlash
    IUFillSwitch(&BacklashS[BACKLASH_START], "BACKLASH_START", "Start", ISS_OFF);
    IUFillSwitch(&BacklashS[BACKLASH_NEXT], "BACKLASH_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&BacklashSP, BacklashS, 2, getDeviceName(), "FOCUS_BACKLASH", "Backlash", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    IUFillSwitch(&FastMoveS[FASTMOVE_IN], "FASTMOVE_IN", "Move In", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_OUT], "FASTMOVE_OUT", "Move out", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_STOP], "FASTMOVE_STOP", "Stop", ISS_OFF);
    IUFillSwitchVector(&FastMoveSP, FastMoveS, 3, getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Hold state
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_ON], "HOLD_ON", "Hold On", ISS_OFF);
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_OFF], "HOLD_OFF", "Hold Off", ISS_OFF);
    IUFillSwitchVector(&MotorHoldSP, MotorHoldS, 2, getDeviceName(), "MOTOR_HOLD", "Motor Hold", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    // Home Position
    IUFillSwitch(&HomeS[0], "FOCUS_HOME_GO", "Go", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "FOCUS_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // Motor rate
    IUFillNumber(&MotorRateN[MOTOR_RATE_ACC], "ACC", "Acceleration", "%.f", 1, 10, 1, 1);
    IUFillNumber(&MotorRateN[MOTOR_RATE_RUN], "RUN", "Run Speed", "%.f", 1, 10, 1, 2);
    IUFillNumber(&MotorRateN[MOTOR_RATE_DEC], "DEC", "Deceleration", "%.f", 1, 10, 1, 1);
    IUFillNumberVector(&MotorRateNP, MotorRateN, 3, getDeviceName(), "MOTOR_RATE", "Motor Rate", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Motor current
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_ACC], "CURR_ACC", "Acceleration", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_RUN], "CURR_RUN", "Run", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_DEC], "CURR_DEC", "Deceleration", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_HOLD], "CURR_HOLD", "Hold", "%.f", 0, 5, 1, 3);
    IUFillNumberVector(&MotorCurrentNP, MotorCurrentN, 4, getDeviceName(), "MOTOR_CURRENT", "Current", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Load motor preset
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_LIGHT], "MOTOR_APPLY_LIGHT", "Light", ISS_OFF);
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_MEDIUM], "MOTOR_APPLY_MEDIUM", "Medium", ISS_OFF);
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_HEAVY], "MOTOR_APPLY_HEAVY", "Heavy", ISS_OFF);
    IUFillSwitchVector(&MotorApplyPresetSP, MotorApplyPresetS, 3, getDeviceName(), "MOTOR_APPLY_PRESET", "Apply Preset",
                       MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Load user preset
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER1], "MOTOR_APPLY_USER1", "User 1", ISS_OFF);
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER2], "MOTOR_APPLY_USER2", "User 2", ISS_OFF);
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER3], "MOTOR_APPLY_USER3", "User 3", ISS_OFF);
    IUFillSwitchVector(&MotorApplyUserPresetSP, MotorApplyUserPresetS, 3, getDeviceName(), "MOTOR_APPLY_USER_PRESET",
                       "Apply Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Save user preset
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER1], "MOTOR_SAVE_USER1", "User 1", ISS_OFF);
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER2], "MOTOR_SAVE_USER2", "User 2", ISS_OFF);
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER3], "MOTOR_SAVE_USER3", "User 3", ISS_OFF);
    IUFillSwitchVector(&MotorSaveUserPresetSP, MotorSaveUserPresetS, 3, getDeviceName(), "MOTOR_SAVE_USER_PRESET",
                       "Save Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    // Rotator Position
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Angle", "%f", 0., 360., .0001, 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                       0, IPS_IDLE );





    // Rotator calibration
    //    IUFillText(&RotCalibrationMessageT[0], "Cal Arco", "Arco Cal Stage", "Press START to calibrate Arco.");
    //    IUFillTextVector(&RotCalibrationMessageTP, RotCalibrationMessageT, 1, getDeviceName(), "ROT_CALIBRATION_MESSAGE", "Cal Arco",
    //                    ROTATOR_TAB, IP_RO, 0, IPS_IDLE);


    // Rotator Calibration
    IUFillSwitch(&RotCalibrationS[ARCO_CALIBRATION_START], "ARCO_CALIBRATION_START", "Start", ISS_OFF);
    //   IUFillSwitch(&RotCalibrationS[ARCO_CALIBRATION_NEXT], "CALIBRATION_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&RotCalibrationSP, RotCalibrationS, 1, getDeviceName(), "ARCO_CALIBRATION", "Cal Arco", ROTATOR_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);




    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].value = 2097152;
    PresetN[0].max = FocusMaxPosN[0].value;
    PresetN[1].max = FocusMaxPosN[0].value;
    PresetN[2].max = FocusMaxPosN[0].value;

    RotatorAbsPosN[0].min = 0.000;
    RotatorAbsPosN[0].max = 360.;
    RotatorAbsPosN[0].value = 0.000;
    RotatorAbsPosN[0].step = 0.0001;

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

bool EssatoArco::updateProperties()
{
    if (isConnected() && updateMaxLimit() == false)
        LOGF_WARN("Check you have the latest %s firmware. Focuser requires calibration.", getDeviceName());

    // Rotator
    INDI::RotatorInterface::updateProperties();
    defineProperty(&RotatorAbsPosNP);
    defineProperty(&RotCalibrationSP);
    defineProperty(&RotCalibrationMessageTP);

    //Focuser
    INDI::Focuser::updateProperties();


    if (isConnected())
    {
        defineProperty(&SpeedNP);
        defineProperty(&CalibrationMessageTP);
        defineProperty(&CalibrationSP);
        defineProperty(&BacklashMessageTP);
        defineProperty(&BacklashSP);
        defineProperty(&HomeSP);
        defineProperty(&MotorRateNP);
        defineProperty(&MotorCurrentNP);
        defineProperty(&MotorHoldSP);
        defineProperty(&MotorApplyPresetSP);
        defineProperty(&MotorApplyUserPresetSP);
        defineProperty(&MotorSaveUserPresetSP);

        defineProperty(&FirmwareTP);

        if (updateTemperature())
            defineProperty(&TemperatureNP);

        if (updateVoltageIn())
            defineProperty(&VoltageInNP);

        if (getStartupValues())
            LOG_INFO("Parameters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");
    }
    else
    {
        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);
        deleteProperty(FirmwareTP.name);
        deleteProperty(VoltageInNP.name);
        deleteProperty(CalibrationMessageTP.name);
        deleteProperty(CalibrationSP.name);
        deleteProperty(BacklashMessageTP.name);
        deleteProperty(BacklashSP.name);
        deleteProperty(SpeedNP.name);
        deleteProperty(HomeSP.name);
        deleteProperty(MotorRateNP.name);
        deleteProperty(MotorCurrentNP.name);
        deleteProperty(MotorHoldSP.name);
        deleteProperty(MotorApplyPresetSP.name);
        deleteProperty(MotorApplyUserPresetSP.name);
        deleteProperty(MotorSaveUserPresetSP.name);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        deleteProperty(RotatorAbsPosNP.name);
        deleteProperty(RotCalibrationSP.name);
        deleteProperty(RotCalibrationMessageTP.name);

    }

    return true;
}





bool EssatoArco::Handshake()
{
    //char res[SESTO_LEN] = {0};
    double ArcoPos;


    if (Ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", getDeviceName());
        ArcoPos = Arco::getArcoAbsPos();
        LOGF_INFO("ARCO POSITION %f", ArcoPos);
        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure focuser is powered and the port is correct.");
    return false;
}

bool EssatoArco::Disconnect()
{
    //    if (isSimulation() == false)
    //        command->goHome();

    return INDI::Focuser::Disconnect();
}

bool EssatoArco::SetFocuserBacklash(int32_t steps)
{
    backlashTicks = static_cast<uint32_t>(abs(steps));
    backlashDirection = steps < 0 ? FOCUS_INWARD : FOCUS_OUTWARD;
    oldbacklashDirection = backlashDirection;
    return true;
}

const char *EssatoArco::getDefaultName()
{
    return "Sesto Senso 2";
}

bool EssatoArco::updateTemperature()
{
    char res[SESTO_LEN] = {0};
    double temperature = 0;

    if (isSimulation())
        strncpy(res, "23.45", SESTO_LEN);
    else if (command->getMotorTemp(res) == false)
        return false;

    try
    {
        temperature = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process temperature response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (temperature > 90)
        return false;

    TemperatureN[TEMPERATURE_MOTOR].value = temperature;
    TemperatureNP.s = IPS_OK;

    // External temperature - Optional
    if (command->getExternalTemp(res))
    {
        TemperatureN[TEMPERATURE_EXTERNAL].value = -273.15;
        try
        {
            temperature = std::stod(res);
        }
        catch(...)
        {
            LOGF_DEBUG("Failed to process external temperature response: %s (%d bytes)", res, strlen(res));
        }

        if (temperature < 90)
            TemperatureN[TEMPERATURE_EXTERNAL].value = temperature;
    }

    return true;
}


bool EssatoArco::updateMaxLimit()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        return true;

    if (command->getMaxPosition(res) == false)
        return false;

    int maxLimit = 0;

    sscanf(res, "%d", &maxLimit);

    if (maxLimit > 0)
    {
        FocusMaxPosN[0].max = maxLimit;
        if (FocusMaxPosN[0].value > maxLimit)
            FocusMaxPosN[0].value = maxLimit;

        FocusAbsPosN[0].min   = 0;
        FocusAbsPosN[0].max   = maxLimit;
        FocusAbsPosN[0].value = 0;
        FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

        FocusRelPosN[0].min   = 0.;
        FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
        FocusRelPosN[0].value = 0;
        FocusRelPosN[0].step  = FocusAbsPosN[0].step;

        PresetN[0].max = maxLimit;
        PresetN[0].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
        PresetN[1].max = maxLimit;
        PresetN[1].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
        PresetN[2].max = maxLimit;
        PresetN[2].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;


        FocusMaxPosNP.s = IPS_OK;
        return true;
    }


    FocusMaxPosNP.s = IPS_ALERT;
    return false;
}

bool EssatoArco::updatePosition()
{
    char res[SESTO_LEN] = {0};
    if (isSimulation())
        snprintf(res, SESTO_LEN, "%u", static_cast<uint32_t>(FocusAbsPosN[0].value));
    else if (command->getAbsolutePosition(res) == false)
        return false;

    try
    {
        int32_t currentPos = std::stoi(res);
        if(backlashDirection == FOCUS_INWARD)
        {
            currentPos += backlashTicks;
        }
        else
        {
            currentPos -= backlashTicks;
        }
        FocusAbsPosN[0].value = currentPos;
        FocusAbsPosNP.s = IPS_OK;


        //Update Rotator Position
        RotatorAbsPosN[0].value = command->getArcoAbsPos();

        if(command->getArcoPosition() >= 0 )
        {
            GotoRotatorN[0].value = command->getArcoPosition();
        }
        else
        {
            GotoRotatorN[0].value = 360.0 + command->getArcoPosition();
        }

        if(!command -> isArcoBusy())
        {
            RotatorAbsPosNP.s = IPS_OK;
            GotoRotatorNP.s = IPS_OK;
        }
        if(command -> isArcoCalibrating())
        {
            RotatorAbsPosNP.s = IPS_BUSY;
            GotoRotatorNP.s = IPS_BUSY;
            RotCalibrationSP.s = IPS_BUSY;
            IDSetSwitch(&RotCalibrationSP, nullptr);
        }
        else
        {
            if(RotCalibrationSP.s == IPS_BUSY)
            {
                RotCalibrationSP.s = IPS_IDLE;
                IDSetSwitch(&RotCalibrationSP, nullptr);
                LOG_INFO("Arco calibration complete.");
                if(command -> syncArco(0))
                {
                    LOG_INFO("Arco synced to zero");
                }
            }

        }

        if(GotoRotatorNP.s == IPS_BUSY) RotatorAbsPosNP.s = IPS_BUSY;
        //LOGF_INFO("RotatorAbsPos: %f",RotatorAbsPosN[0].value);
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
        //End Update Rotator Position

        return true;
    }
    catch(...)
    {
        LOGF_WARN("Failed to process position response: %s (%d bytes)", res, strlen(res));
        FocusAbsPosNP.s = IPS_ALERT;
        return false;
    }
}

bool EssatoArco::updateVoltageIn()
{
    char res[SESTO_LEN] = {0};
    double voltageIn = 0;

    if (isSimulation())
        strncpy(res, "12.00", SESTO_LEN);
    else if (command->getVoltageIn(res) == false)
        return false;

    try
    {
        voltageIn = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process voltage response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (voltageIn > 24)
        return false;

    VoltageInN[0].value = voltageIn;
    VoltageInNP.s = (voltageIn >= 11.0) ? IPS_OK : IPS_ALERT;

    return true;
}

bool EssatoArco::fetchMotorSettings()
{
    // Fetch driver state and reflect in INDI
    MotorRates ms;
    MotorCurrents mc;
    bool motorHoldActive = false;

    if (isSimulation())
    {
        ms.accRate = 1;
        ms.runSpeed = 2;
        ms.decRate = 1;
        mc.accCurrent = 3;
        mc.runCurrent = 4;
        mc.decCurrent = 3;
        mc.holdCurrent = 2;
    }
    else
    {
        if (!command->getMotorSettings(ms, mc, motorHoldActive))
        {
            MotorRateNP.s = IPS_IDLE;
            MotorCurrentNP.s = IPS_IDLE;
            MotorHoldSP.s = IPS_IDLE;
            return false;
        }
    }

    MotorRateN[MOTOR_RATE_ACC].value = ms.accRate;
    MotorRateN[MOTOR_RATE_RUN].value = ms.runSpeed;
    MotorRateN[MOTOR_RATE_DEC].value = ms.decRate;
    MotorRateNP.s = IPS_OK;
    IDSetNumber(&MotorRateNP, nullptr);

    MotorCurrentN[MOTOR_CURR_ACC].value = mc.accCurrent;
    MotorCurrentN[MOTOR_CURR_RUN].value = mc.runCurrent;
    MotorCurrentN[MOTOR_CURR_DEC].value = mc.decCurrent;
    MotorCurrentN[MOTOR_CURR_HOLD].value = mc.holdCurrent;
    MotorCurrentNP.s = IPS_OK;
    IDSetNumber(&MotorCurrentNP, nullptr);

    // Also update motor hold switch
    const char *activeSwitchID = motorHoldActive ? "HOLD_ON" : "HOLD_OFF";
    ISwitch *sp = IUFindSwitch(&MotorHoldSP, activeSwitchID);
    assert(sp != nullptr && "Motor hold switch not found");
    if (sp)
    {
        IUResetSwitch(&MotorHoldSP);
        sp->s = ISS_ON;
        MotorHoldSP.s = motorHoldActive ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&MotorHoldSP, nullptr);
    }

    if (motorHoldActive && mc.holdCurrent == 0)
    {
        LOG_WARN("Motor hold current set to 0, motor hold setting will have no effect");
    }

    return true;
}

bool EssatoArco::AbortRotator()
{
    bool rc = command->stopArco();
    if (rc && RotatorAbsPosNP.s != IPS_OK)
    {
        RotatorAbsPosNP.s = IPS_OK;
        GotoRotatorNP.s = IPS_OK;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
    }

    return rc;
}

bool  EssatoArco::ReverseRotator(bool enabled)
{

}

bool EssatoArco::SyncRotator(double angle)
{
    GotoRotatorN[0].value = angle;
    return command -> syncArco(angle);
}


bool EssatoArco::applyMotorRates()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    MotorRates mr;
    mr.accRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_ACC].value);
    mr.runSpeed = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_RUN].value);
    mr.decRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_DEC].value);

    if (!command->setMotorRates(mr))
    {
        LOG_ERROR("Failed to apply motor rates");
        // TODO: Error state?
        return false;
    }

    LOGF_INFO("Motor rates applied: Acc: %u Run: %u Dec: %u", mr.accRate, mr.runSpeed, mr.decRate);
    return true;
}

bool EssatoArco::applyMotorCurrents()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    MotorCurrents mc;
    mc.accCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_ACC].value);
    mc.runCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_RUN].value);
    mc.decCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_DEC].value);
    mc.holdCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_HOLD].value);

    if (!command->setMotorCurrents(mc))
    {
        LOG_ERROR("Failed to apply motor currents");
        // TODO: Error state?
        return false;
    }

    LOGF_INFO("Motor currents applied: Acc: %u Run: %u Dec: %u Hold: %u", mc.accCurrent, mc.runCurrent, mc.decCurrent,
              mc.holdCurrent);
    return true;
}

bool EssatoArco::isMotionComplete()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
    {
        int32_t nextPos = FocusAbsPosN[0].value;
        int32_t targPos = static_cast<int32_t>(targetPos);

        if (targPos > nextPos)
            nextPos += 250;
        else if (targPos < nextPos)
            nextPos -= 250;

        if (abs(nextPos - targPos) < 250)
            nextPos = targetPos;
        else if (nextPos < 0)
            nextPos = 0;
        else if (nextPos > FocusAbsPosN[0].max)
            nextPos = FocusAbsPosN[0].max;

        snprintf(res, SESTO_LEN, "%d", nextPos);
    }
    else
    {
        if(command->getCurrentSpeed(res))
        {
            try
            {
                uint32_t newSpeed = std::stoi(res);
                SpeedN[0].value = newSpeed;
                SpeedNP.s = IPS_OK;
            }
            catch (...)
            {
                LOGF_WARN("Failed to get motor speed response: %s (%d bytes)", res, strlen(res));
            }

            if(!strcmp(res, "0"))
                return true;

            *res = {0};
            if(command->getAbsolutePosition(res))
            {
                try
                {
                    uint32_t newPos = std::stoi(res);
                    if(backlashDirection == FOCUS_INWARD)
                    {
                        newPos += backlashTicks;
                    }
                    else
                    {
                        newPos -= backlashTicks;
                    }
                    FocusAbsPosN[0].value = newPos;
                }
                catch (...)
                {
                    LOGF_WARN("Failed to process motion response: %s (%d bytes)", res, strlen(res));
                }
            }
        }

    }

    return false;
}

bool EssatoArco::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Calibrate focuser
        if (!strcmp(name, CalibrationSP.name))
        {
            int current_switch = 0;

            CalibrationSP.s = IPS_BUSY;
            //IDSetSwitch(&CalibrationSP, nullptr);
            IUUpdateSwitch(&CalibrationSP, states, names, n);

            current_switch = IUFindOnSwitchIndex(&CalibrationSP);
            CalibrationS[current_switch].s = ISS_ON;
            IDSetSwitch(&CalibrationSP, nullptr);

            if (current_switch == CALIBRATION_START)
            {
                if (cStage == Idle || cStage == Complete )
                {
                    // Start the calibration process
                    LOG_INFO("Start Calibration");
                    CalibrationSP.s = IPS_BUSY;
                    IDSetSwitch(&CalibrationSP, nullptr);

                    //
                    // Init
                    //
                    if (m_IsEssatoArco && command->initCalibration() == false)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Set focus in MIN position and then press NEXT.");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    // Motor hold disabled during calibration init, so fetch new hold state
                    fetchMotorSettings();

                    // Set next step
                    cStage = GoToMiddle;
                }
                else
                {
                    LOG_INFO("Already started calibration. Proceed to next step.");
                    IUSaveText(&CalibrationMessageT[0], "Already started. Proceed to NEXT.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }
            }
            else if (current_switch == CALIBRATION_NEXT)
            {
                if (cStage == GoToMiddle)
                {
                    defineProperty(&FastMoveSP);
                    if (m_IsEssatoArco)
                    {
                        if (command->storeAsMinPosition() == false)
                            return false;

                        IUSaveText(&CalibrationMessageT[0], "Press MOVE OUT to move focuser out (CAUTION!)");
                        IDSetText(&CalibrationMessageTP, nullptr);
                        cStage = GoMinimum;
                    }
                    // For Esatto, start moving out immediately
                    else
                    {
                        cStage = GoMaximum;

                        ISState fs[1] = { ISS_ON };
                        const char *fn[1] =  { FastMoveS[FASTMOVE_OUT].name };
                        ISNewSwitch(getDeviceName(), FastMoveSP.name, fs, const_cast<char **>(fn), 1);
                    }
                }
                else if (cStage == GoMinimum)
                {
                    char res[SESTO_LEN] = {0};
                    if (m_IsEssatoArco && command->storeAsMaxPosition(res) == false)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Press NEXT to finish.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    cStage = GoMaximum;
                }
                else if (cStage == GoMaximum)
                {
                    char res[SESTO_LEN] = {0};

                    if (command->getMaxPosition(res) == false)
                        return false;

                    int maxLimit = 0;
                    sscanf(res, "%d", &maxLimit);
                    LOGF_INFO("MAX setting is %d", maxLimit);

                    FocusMaxPosN[0].max = maxLimit;
                    FocusMaxPosN[0].value = maxLimit;

                    FocusAbsPosN[0].min   = 0;
                    FocusAbsPosN[0].max   = maxLimit;
                    FocusAbsPosN[0].value = maxLimit;
                    FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

                    FocusRelPosN[0].min   = 0.;
                    FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
                    FocusRelPosN[0].value = 0;
                    FocusRelPosN[0].step  = FocusAbsPosN[0].step;

                    PresetN[0].max = maxLimit;
                    PresetN[0].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
                    PresetN[1].max = maxLimit;
                    PresetN[1].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
                    PresetN[2].max = maxLimit;
                    PresetN[2].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

                    FocusMaxPosNP.s = IPS_OK;
                    IUUpdateMinMax(&FocusAbsPosNP);
                    IUUpdateMinMax(&FocusRelPosNP);
                    IUUpdateMinMax(&PresetNP);
                    IUUpdateMinMax(&FocusMaxPosNP);

                    IUSaveText(&CalibrationMessageT[0], "Calibration Completed.");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    deleteProperty(FastMoveSP.name);
                    cStage = Complete;

                    LOG_INFO("Calibration completed");
                    CalibrationSP.s = IPS_OK;
                    IDSetSwitch(&CalibrationSP, nullptr);
                    CalibrationS[current_switch].s = ISS_OFF;
                    IDSetSwitch(&CalibrationSP, nullptr);

                    // Double check motor hold state after calibration
                    fetchMotorSettings();
                }
                else
                {
                    IUSaveText(&CalibrationMessageT[0], "Calibration not in progress.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }

            }
            return true;
        }
        // Set backlash
        else if (!strcmp(name, BacklashSP.name))
        {
            int current_switch = 0;

            BacklashSP.s = IPS_BUSY;
            //IDSetSwitch(&BacklashSP, nullptr);
            IUUpdateSwitch(&BacklashSP, states, names, n);

            current_switch = IUFindOnSwitchIndex(&BacklashSP);
            BacklashS[current_switch].s = ISS_ON;
            IDSetSwitch(&BacklashSP, nullptr);

            if (current_switch == BACKLASH_START)
            {
                if (bStage == BacklashIdle || bStage == BacklashComplete )
                {
                    // Start the backlash measurement process
                    LOG_INFO("Start Backlash Measure");
                    BacklashSP.s = IPS_BUSY;
                    IDSetSwitch(&BacklashSP, nullptr);

                    //
                    // Init
                    //

                    IUSaveText(&BacklashMessageT[0], "Drive the focuser in any direction until focus changes.");
                    IDSetText(&BacklashMessageTP, nullptr);

                    // Motor hold disabled during calibration init, so fetch new hold state
                    fetchMotorSettings();

                    // Set next step
                    bStage = BacklashMinimum;
                }
                else
                {
                    LOG_INFO("Already started backlash measure. Proceed to next step.");
                    IUSaveText(&BacklashMessageT[0], "Already started. Proceed to NEXT.");
                    IDSetText(&BacklashMessageTP, nullptr);
                }
            }
            else if (current_switch == BACKLASH_NEXT)
            {
                if (bStage == BacklashMinimum)
                {
                    FocusBacklashN[0].value = static_cast<int32_t>(FocusAbsPosN[0].value);

                    IUSaveText(&BacklashMessageT[0], "Drive the focuser in the opposite direction, then press NEXT to finish.");
                    IDSetText(&BacklashMessageTP, nullptr);
                    bStage = BacklashMaximum;
                }
                else if (bStage == BacklashMaximum)
                {
                    FocusBacklashN[0].value -= FocusAbsPosN[0].value;
                    IDSetNumber(&FocusBacklashNP, nullptr);
                    SetFocuserBacklashEnabled(true);

                    IUSaveText(&BacklashMessageT[0], "Backlash Measure Completed.");
                    IDSetText(&BacklashMessageTP, nullptr);

                    bStage = BacklashComplete;

                    LOG_INFO("Backlash measurement completed");
                    BacklashSP.s = IPS_OK;
                    IDSetSwitch(&BacklashSP, nullptr);
                    BacklashS[current_switch].s = ISS_OFF;
                    IDSetSwitch(&BacklashSP, nullptr);
                }
                else
                {
                    IUSaveText(&BacklashMessageT[0], "Backlash not in progress.");
                    IDSetText(&BacklashMessageTP, nullptr);
                }

            }
            return true;
        }
        // Fast motion
        else if (!strcmp(name, FastMoveSP.name))
        {
            IUUpdateSwitch(&FastMoveSP, states, names, n);
            int current_switch = IUFindOnSwitchIndex(&FastMoveSP);
            char res[SESTO_LEN] = {0};

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    if (command->fastMoveIn(res) == false)
                    {
                        return false;
                    }
                    break;
                case FASTMOVE_OUT:
                    if (m_IsEssatoArco)
                    {
                        if (command->goOutToFindMaxPos() == false)
                        {
                            return false;
                        }
                        IUSaveText(&CalibrationMessageT[0], "Press STOP focuser almost at MAX position.");

                        // GoOutToFindMaxPos should cause motor hold to be reactivated
                        fetchMotorSettings();
                    }
                    else
                    {
                        if (command->fastMoveOut(res))
                        {
                            IUSaveText(&CalibrationMessageT[0], "Focusing out to detect hall sensor.");

                            //                            if (m_MotionProgressTimerID > 0)
                            //                                IERmTimer(m_MotionProgressTimerID);
                            //                            m_MotionProgressTimerID = IEAddTimer(500, &EssatoArco::checkMotionProgressHelper, this);
                            m_MotionProgressTimer.start(500);
                        }
                    }
                    IDSetText(&CalibrationMessageTP, nullptr);
                    break;
                case FASTMOVE_STOP:
                    if (command->stop() == false)
                    {
                        return false;
                    }
                    IUSaveText(&CalibrationMessageT[0], "Press NEXT to store max limit.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    break;
                default:
                    break;
            }

            FastMoveSP.s = IPS_BUSY;
            IDSetSwitch(&FastMoveSP, nullptr);
            return true;
        }
        // Homing
        else if (!strcmp(HomeSP.name, name))
        {
            char res[SESTO_LEN] = {0};
            if (command->goHome(res))
            {
                HomeS[0].s = ISS_ON;
                HomeSP.s = IPS_BUSY;

                //                if (m_MotionProgressTimerID > 0)
                //                    IERmTimer(m_MotionProgressTimerID);
                //                m_MotionProgressTimerID = IEAddTimer(100, &EssatoArco::checkMotionProgressHelper, this);
                m_MotionProgressTimer.start(100);
            }
            else
            {
                HomeS[0].s = ISS_OFF;
                HomeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }
        else if (!strcmp(name, MotorHoldSP.name))
        {
            IUUpdateSwitch(&MotorHoldSP, states, names, n);
            ISwitch *sp = IUFindOnSwitch(&MotorHoldSP);
            assert(sp != nullptr);

            // NOTE: Default to HOLD_ON as a safety feature
            if (!strcmp(sp->name, "HOLD_OFF"))
            {
                command->setMotorHold(false);
                MotorHoldSP.s = IPS_ALERT;
                LOG_INFO("Motor hold OFF. You may now manually adjust the focuser. Remember to enable motor hold once done.");
            }
            else
            {
                command->setMotorHold(true);
                MotorHoldSP.s = IPS_OK;
                LOG_INFO("Motor hold ON. Do NOT attempt to manually adjust the focuser!");
                if (MotorCurrentN[MOTOR_CURR_HOLD].value < 2.0)
                {
                    LOGF_WARN("Motor hold current set to %.1f: This may be insufficent to hold focus", MotorCurrentN[MOTOR_CURR_HOLD].value);
                }
            }

            IDSetSwitch(&MotorHoldSP, nullptr);
            return true;
        }
        else if (!strcmp(name, MotorApplyPresetSP.name))
        {
            IUUpdateSwitch(&MotorApplyPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorApplyPresetSP);
            assert(index >= 0 && index < 3);

            const char* presetName = MOTOR_PRESET_NAMES[index];

            if (command->applyMotorPreset(presetName))
            {
                LOGF_INFO("Loaded motor preset: %s", presetName);
                MotorApplyPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to load motor preset: %s", presetName);
                MotorApplyPresetSP.s = IPS_ALERT;
            }

            MotorApplyPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorApplyPresetSP, nullptr);

            fetchMotorSettings();
            return true;
        }
        else if (!strcmp(name, MotorApplyUserPresetSP.name))
        {
            IUUpdateSwitch(&MotorApplyUserPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorApplyUserPresetSP);
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            if (command->applyMotorUserPreset(userIndex))
            {
                LOGF_INFO("Loaded motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to load motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.s = IPS_ALERT;
            }

            MotorApplyUserPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorApplyUserPresetSP, nullptr);

            fetchMotorSettings();
            return true;
        }
        else if (!strcmp(name, MotorSaveUserPresetSP.name))
        {
            IUUpdateSwitch(&MotorSaveUserPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorSaveUserPresetSP);
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            MotorRates mr;
            mr.accRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_ACC].value);
            mr.runSpeed = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_RUN].value);
            mr.decRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_DEC].value);

            MotorCurrents mc;
            mc.accCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_ACC].value);
            mc.runCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_RUN].value);
            mc.decCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_DEC].value);
            mc.holdCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_HOLD].value);

            if (command->saveMotorUserPreset(userIndex, mr, mc))
            {
                LOGF_INFO("Saved motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to save motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.s = IPS_ALERT;
            }

            MotorSaveUserPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorSaveUserPresetSP, nullptr);
            return true;
        }
        else if (!strcmp(name, RotCalibrationSP.name))
        {
            if(command -> calArco())
            {
                LOG_INFO("Calibrating Arco. Please wait.");
                RotCalibrationSP.s = IPS_BUSY;
                IDSetSwitch(&RotCalibrationSP, nullptr);
            }
            return true;
        }

        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
                return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool EssatoArco::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if (!strcmp(name, MotorRateNP.name))
    {
        IUUpdateNumber(&MotorRateNP, values, names, n);
        MotorRateNP.s = IPS_OK;
        applyMotorRates();
        IDSetNumber(&MotorRateNP, nullptr);
        return true;
    }
    else if (!strcmp(name, MotorCurrentNP.name))
    {
        IUUpdateNumber(&MotorCurrentNP, values, names, n);
        MotorCurrentNP.s = IPS_OK;
        applyMotorCurrents();
        IDSetNumber(&MotorCurrentNP, nullptr);
        return true;
    }

    else if (strcmp(name, RotatorAbsPosNP.name) == 0)
    {
        char res[SESTO_LEN] = {0};
        bool rc = command->setArcoAbsPos(values[0], res);
        if(rc == true)
        {
            RotatorAbsPosNP.s = IPS_BUSY;
        }
        else
        {
            RotatorAbsPosNP.s = IPS_ALERT;
        }
        GotoRotatorNP.s = RotatorAbsPosNP.s;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
        //IUUpdateNumber(&RotatorAbsPosNP,values,names,n);
        if (RotatorAbsPosNP.s == IPS_BUSY)
            LOGF_INFO("Rotator moving to %f degrees...", values[0]);
        return true;
    }
    else if (strstr(name, "ROTATOR"))
    {
        if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState EssatoArco::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (isSimulation() == false)
    {
        backlashDirection = targetTicks < lastPos ? FOCUS_INWARD : FOCUS_OUTWARD;
        if(backlashDirection == FOCUS_INWARD)
        {
            targetPos -=  backlashTicks;
        }
        else
        {
            targetPos +=  backlashTicks;
        }
        char res[SESTO_LEN] = {0};
        if (command->go(static_cast<uint32_t>(targetPos), res) == false)
            return IPS_ALERT;
    }

    //    if (m_MotionProgressTimerID > 0)
    //        IERmTimer(m_MotionProgressTimerID);
    //    m_MotionProgressTimerID = IEAddTimer(10, &EssatoArco::checkMotionProgressHelper, this);
    m_MotionProgressTimer.start(10);
    return IPS_BUSY;
}

//Move Rotator to Gotposition corrected for Sync-Angle
IPState EssatoArco::MoveRotator(double angle)
{
    char res[SESTO_LEN] = {0};
    //Calc sync angle (could also be received from Arco (CORRECTION_POS)
    double PosCor = RotatorAbsPosN[0].value - GotoRotatorN[0].value;
    if(PosCor - 360.0 < 0.0001)
        PosCor = 0.0;
    angle += PosCor;
    if(angle > 360.0)
        angle -= 360.0;
    if(command->setArcoAbsPos(angle, res) == false)
    {
        LOGF_INFO("ARCO won't move. %s", res);
        return IPS_ALERT;
    }
    LOGF_DEBUG("Reply: %s", res);
    return IPS_BUSY;
}

IPState EssatoArco::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

bool EssatoArco::AbortFocuser()
{
    //    if (m_MotionProgressTimerID > 0)
    //    {
    //        IERmTimer(m_MotionProgressTimerID);
    //        m_MotionProgressTimerID = -1;
    //    }

    m_MotionProgressTimer.stop();

    if (isSimulation())
        return true;

    bool rc = command->abort();

    if (rc && HomeSP.s == IPS_BUSY)
    {
        HomeS[0].s = ISS_OFF;
        HomeSP.s = IPS_IDLE;
        IDSetSwitch(&HomeSP, nullptr);
    }

    return rc;
}

//void EssatoArco::checkMotionProgressHelper(void *context)
//{
//    static_cast<EssatoArco*>(context)->checkMotionProgressCallback();
//}

//void EssatoArco::checkHallSensorHelper(void *context)
//{
//    static_cast<EssatoArco*>(context)->checkHallSensorCallback();
//}

//
// This timer function is initiated when a GT command has been issued
// A timer will call this function on a regular interval during the motion
// Modified the code to exit when motion is complete
//
void EssatoArco::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        SpeedNP.s = IPS_OK;
        SpeedN[0].value = 0;
        IDSetNumber(&SpeedNP, nullptr);

        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);


        lastPos = FocusAbsPosN[0].value;

        if (HomeSP.s == IPS_BUSY)
        {
            LOG_INFO("Focuser at home position.");
            HomeS[0].s = ISS_OFF;
            HomeSP.s = IPS_OK;
            IDSetSwitch(&HomeSP, nullptr);
        }
        else if (CalibrationSP.s == IPS_BUSY)
        {
            ISState states[2] = { ISS_OFF, ISS_ON };
            const char * names[2] = { CalibrationS[CALIBRATION_START].name, CalibrationS[CALIBRATION_NEXT].name };
            ISNewSwitch(getDeviceName(), CalibrationSP.name, states, const_cast<char **>(names), CalibrationSP.nsp);
        }
        else
            LOG_INFO("Focuser reached requested position.");
        return;
    }
    else
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SpeedNP.s = IPS_BUSY;
    IDSetNumber(&SpeedNP, nullptr);

    lastPos = FocusAbsPosN[0].value;

    //    IERmTimer(m_MotionProgressTimerID);
    //    m_MotionProgressTimerID = IEAddTimer(500, &EssatoArco::checkMotionProgressHelper, this);
    m_MotionProgressTimer.start(500);
}

void EssatoArco::checkHallSensorCallback()
{
    // FIXME
    // Function not getting call from anywhere?
    char res[SESTO_LEN] = {0};
    if (command->getHallSensor(res))
    {
        int detected = 0;
        if (sscanf(res, "%d", &detected) == 1)
        {
            if (detected == 1)
            {
                ISState states[2] = { ISS_OFF, ISS_ON };
                const char * names[2] = { CalibrationS[CALIBRATION_START].name, CalibrationS[CALIBRATION_NEXT].name };
                ISNewSwitch(getDeviceName(), CalibrationSP.name, states, const_cast<char **>(names), CalibrationSP.nsp);
                return;
            }
        }
    }

    //m_HallSensorTimerID = IEAddTimer(1000, &EssatoArco::checkHallSensorHelper, this);
    m_HallSensorTimer.start();
}

void EssatoArco::TimerHit()
{
    if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY || (m_IsEssatoArco
            && CalibrationSP.s == IPS_BUSY))
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 0)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
            }
        }

        // Also use temparature poll rate for tracking input voltage
        rc = updateVoltageIn();
        if (rc)
        {
            if (fabs(lastVoltageIn - VoltageInN[0].value) >= 0.1)
            {
                IDSetNumber(&VoltageInNP, nullptr);
                lastVoltageIn = VoltageInN[0].value;

                if (VoltageInN[0].value < 11.0)
                {
                    LOG_WARN("Please check 12v DC power supply is connected.");
                }
            }
        }

        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(getCurrentPollingPeriod());
}

bool EssatoArco::getStartupValues()
{
    bool rc = updatePosition();
    if (rc)
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    rc &= fetchMotorSettings();

    return (rc);
}


bool EssatoArco::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return false;
}


bool EssatoArco::Ack()
{
    std::string response;

    if (isSimulation())
        response = "1.0 Simulation";
    else
    {
        if(initCommandSet() == false)
        {
            LOG_ERROR("Failed setting attributes on serial port and init command sets");
            return false;
        }
        if(command->getSerialNumber(response))
        {
            LOGF_INFO("Serial number: %s", res);
        }
        else
        {
            return false;
        }
    }

    m_IsEssatoArco = !strstr(res, "ESATTO");
    IUSaveText(&FirmwareT[FIRMWARE_SN], res);

    if (command->getFirmwareVersion(res))
    {
        LOGF_INFO("Firmware version: %s", res);
        IUSaveText(&FirmwareT[FIRMWARE_VERSION], res);
    }
    else
    {
        return false;
    }

    return true;
}


void EssatoArco::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}


bool EssatoArco::initCommandSet()
{
    command = new CommandSet(PortFD, getDeviceName());

    struct termios tty_setting;
    if (tcgetattr(PortFD, &tty_setting) == -1)
    {
        LOG_ERROR("setTTYFlags: failed getting tty attributes.");
        return false;
    }
    tty_setting.c_lflag |= ICANON;
    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOG_ERROR("setTTYFlags: failed setting attributes on serial port.");
        return false;
    }
    return true;
}

bool EssatoArco::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);
    RI::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MotorRateNP);
    IUSaveConfigNumber(fp, &MotorCurrentNP);
    return true;
}


inline void remove_chars_inplace(std::string &str, char ch)
{
    str.erase(std::remove(str.begin(), str.end(), ch), str.end());
}

bool getSerialNumber(std::string &response)
{
    json jsonRequest = {"req", {"get", {"SN", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, jsonResponse))
    {
        jsonResponse["get"]["SN"].get_to(response);
        return true;
    }

    return false;
}

bool getFirmwareVersion(std::string &response)
{
    json jsonRequest = {"req", {"get", {"SWVERS", ""}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, jsonResponse))
    {
        jsonResponse["get"]["SWVERS"]["SWAPP"].get_to(response);
        return true;
    }

    return false;
}

bool abort()
{
    json jsonRequest = {"req", {"cmd", {"MOT1", {"MOT_ABORT", ""}}}};
    json jsonResponse;
    if (sendCommand(jsonRequest, jsonResponse))
    {
        return jsonResponse["get"]["cmd"]["MOT1"]["MOT_ABORT"] == "done";
    }

    return false;
}

bool go(uint32_t targetTicks, char *res)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"MOT1\" :{\"GOTO\":%u}}}}", targetTicks);
    return sendCommand(cmd, "GOTO", res);
}


