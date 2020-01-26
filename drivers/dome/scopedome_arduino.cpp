/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

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

#include "scopedome_arduino.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <termios.h>

#include <cstring>

#define SCOPEDOME_TIMEOUT 2
#define SCOPEDOME_MAX_READS 10

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    static_cast<std::string *>(userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

ScopeDomeArduino::ScopeDomeArduino(ScopeDome* driver, Connection::Interface* iface)
    : parent(driver), interface(iface)
{
    if (interface->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        ethernet = false;
        PortFD = static_cast<Connection::Serial*>(interface)->getPortFD();
    }
    else
    {
        ethernet = true;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            const INDI::PropertyText &credentials = parent->getCredentials();
            curl_easy_setopt(curl, CURLOPT_USERNAME, credentials[0].getText());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, credentials[1].getText());

            // (Ab)use TCP connection to get host and port (though port is not actually used)
            auto tcp = static_cast<Connection::TCP*>(interface);
            hostName = tcp->host();
            port = tcp->port();
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        }
        else
        {
            LOG_ERROR("Error initializing curl library");
        }
    }
}

ScopeDomeArduino::~ScopeDomeArduino()
{
    if (ethernet)
    {
        curl_global_cleanup();
        if (curl)
        {
            curl_easy_cleanup(curl);
            curl = nullptr;
        }
    }
}

bool ScopeDomeArduino::detect()
{
    std::string res;
    if(performCommand("getMode", res))
    {
        return (res == "MASTER"); // only master & slave mode is currently supported (not clamshell or rolloff root)
    }
    return false;
}

bool ScopeDomeArduino::performCommand(const std::string &command, std::string &response)
{
    if (ethernet)
    {
        if (curl)
        {
            CURLcode res;
            std::string readBuffer;
            char requestURL[MAXRBUF + 1];

            // Write buffer
            LOGF_DEBUG("write cmd: %s", command.c_str());

            snprintf(requestURL, MAXRBUF, "http://%s/?%s", hostName.c_str(), command.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, requestURL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);

            if(readBuffer.size() < 3)
            {
                LOGF_ERROR("Error reading: %s. Cmd: %s", readBuffer.c_str(), command.c_str());
                return false;
            }
            // Response is in form command|status|response, strip \r\n
            std::string resp(readBuffer.begin(), readBuffer.end() - 2);

            auto split = splitString(resp, '|');

            if (split.size() != 3)
            {
                LOGF_ERROR("Invalid response: %s. Cmd: %s", resp.c_str(), command.c_str());
                return false;
            }

            if (split[1] != "OK")
            {
                LOGF_ERROR("Error from device: %s. Cmd: %s", resp.c_str(), command.c_str());
                return false;
            }
            response = split[2];
            LOGF_DEBUG("read response: %s", response.c_str());
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        int nbytes_written = 0, rc = -1;
        char errstr[MAXRBUF];

        if (command.length() == 0)
            return false;

        tcflush(PortFD, TCIOFLUSH);

        // Write buffer
        LOGF_DEBUG("write cmd: %s", command.c_str());

        char lineBuf[MAXRBUF + 1];
        snprintf(lineBuf, MAXRBUF, "%s\r\n", command.c_str());

        if ((rc = tty_write_string(PortFD, lineBuf, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error writing command: %s. Cmd: %s", errstr, command.c_str());
            return false;
        }

        int nbytes_read = 0;

        // Read response
        if ((rc = tty_nread_section(PortFD, lineBuf, MAXRBUF, '\n', SCOPEDOME_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error reading: %s. Cmd: %s", errstr, command.c_str());
            return false;
        }
        lineBuf[nbytes_read] = 0;
        int len = strlen(lineBuf);
        if(len < 3)
        {
            LOGF_ERROR("Error reading: %s. Cmd: %s", errstr, command.c_str());
            return false;
        }
        // Response is in form command|status|response, strip \n
        std::string resp(lineBuf, strlen(lineBuf) - 1);

        auto split = splitString(resp, '|');

        if (split.size() != 3)
        {
            LOGF_ERROR("Invalid response: %s. Cmd: %s", lineBuf, command.c_str());
            return false;
        }

        if (split[1] != "OK")
        {
            LOGF_ERROR("Error from device: %s. Cmd: %s", lineBuf, command.c_str());
            return false;
        }
        response = split[2];
        LOGF_DEBUG("read response: %s", response.c_str());
        return true;
    }
}

void ScopeDomeArduino::getFirmwareVersions(double &main, double &rotary)
{
    std::string res;
    if(performCommand("getFirmwareVersion", res))
    {
        main = std::stod(res);
    }

    if(performCommand("slave=getFirmwareVersion", res))
    {
        rotary = std::stod(res);
    }
}

uint32_t ScopeDomeArduino::getStepsPerRevolution()
{
    std::string res;
    stepsPerRevolution = 0;
    if(performCommand("getCalibratedRotation", res))
    {
        stepsPerRevolution = std::stoi(res);
    }
    if(stepsPerRevolution == 0)
    {
        LOG_INFO("Step count read as zero, run calibration");
        stepsPerRevolution = 3240;
    }
    return stepsPerRevolution;
}


int ScopeDomeArduino::updateState()
{
    std::string status;
    if(!performCommand("getStatus", status))
    {
        return -1;
    }

    // Parse the string
    // getStatus|OK|0:1:0:1:1:1:1:1;0.0000:1.0000:0.4399;32000;31.29:38.00:999.90:999.90:999.90:999.90:999.90:999.90:999.90;1009.21:999.90;0;5.09;1:1:1:1:1:1;0:0:0:0:0.0000:0.0000:0:0:0;0;126;1#0:1:1:1:1:1:1:1;0.0000:1.0000:0.4291;32000;32.31:39.00:999.90:999.90:999.90:999.90:999.90:999.90:999.90;1009.30:999.90;0;5.09;1:1:1:1:1:1;0:0:0:0:0.0000:0.0000:0:0:0;0;106;1#1:0:0:0
    /*
        get master+slave status in format:
        <master digital inputs>;<master analog
        inputs>;<master encoders>;<master
        temperatures>;<master+slave
        clouds>;<master Vcc>;<master
        buttons>;<master relays>;<master loop
        time>;<master fresh flag>#<slave digital
        inputs>;<slave analog inputs>;<slave
        encoders>;<slave temperatures>;<slave
        clouds>;<slave Vcc>;<slave
        buttons>;<slave relays>;<slave loop
        time>;<slave fresh flag>#<flags>
    */
    auto parts = splitString(status, '#');
    if(parts.size() == 3)
    {
        auto master = splitString(parts[0], ';');

        if (master.size() == 12)
        {
            int i = 0;
            for(auto &s : master)
            {
                LOGF_DEBUG("master %d: %s", i++, s.c_str());
            }
            i = 0;
            auto digitalInputs = splitString(master[0], ':');

            for(const auto &input : digitalInputs)
            {
                inputs[i++] = (input == "0"); // inverted for some reason
            }

            i = 0;
            auto analogInputs = splitString(master[1], ':');
            for(const auto &input : analogInputs)
            {
                sensors[i++] = std::stod(input);
            }
            rotaryEncoder = std::stoi(master[2]);
            auto temperatures = splitString(master[3], ':');
            for(const auto &temp : temperatures)
            {
                sensors[i++] = std::stod(temp);
            }
            auto pressure_humidity = splitString(master[4], ':');
            for(const auto &value : pressure_humidity)
            {
                sensors[i++] = std::stod(value);
            }
            std::string clouds = master[5];
            std::string vcc = master[6];
            std::string buttons = master[7];
            auto relayPWM = splitString(master[8], ':');
            i = 0;
            for(int r = 6; r < 9; r++)
            {
                relays[i++] = (relayPWM[r] == "1");
            }
            moving = (relayPWM[0] == "1" || relayPWM[1] == "1");
            if(!moving && rotaryEncoder != previousEncoder) // Check if the dome is still moving from inertia
                moving = true;
            previousEncoder = rotaryEncoder;

            std::string emergency = master[9];
            std::string loopTime = master[10];
            std::string freshFlag = master[11];

            // Voltage 064V needs some adjustment
            sensors[0] *= 10.0 * (5.0 / 0.1955);
        }
        /*
        MASTER_INPD_DETECT_230LOSS:
        MASTER_INPD_ENCODERA:
        MASTER_INPD_HOMESENSOR:
        MASTER_INPD_FREE1:
        MASTER_INPD_FREE2:
        MASTER_INPD_RAINSENSOR:
        MASTER_INPD_CLOUDSENSOR:
        MASTER_INPD_TELESCOPE_A_H;
        MASTER_INPA_VOLTAGE064:
        MASTER_INPA_T_PT100:
        MASTER_INPA_T_PCB;
        MASTER_ENCODER_A;
        MASTER_THERMOMETER_PCB:
        MASTER_BAROMETER_TEMPERATURE:
        MASTER_THERMOMETER_ONEWIRE_MOTOR:
        MASTER_THERMOMETER_ONEWIRE_OUTSIDE:
        MASTER_THERMOMETER_ONEWIRE_MIRROR_1:
        MASTER_THERMOMETER_ONEWIRE_MIRROR_2:
        MASTER_HIGROMETER_TEMPERATURE:
        MASTER_PIROMETER_AMBIENT:
        MASTER_PIROMETER_SENSOR;
        MASTER_BAROMETER_PRESSURE:
        MASTER_HIGROMETER_HUMIDITY;
        MASTER_CLOUDS;
        MASTER_Vcc;
        MASTER_BUTTON_CW:
        MASTER_BUTTON_CCW:
        MASTER_BUTTON_FREE1:
        MASTER_BUTTON_FREE2:
        MASTER_BUTTON_FREE3:
        MASTER_BUTTON_PAIR;
        MASTER_REL_CW:
        MASTER_REL_CCW:
        MASTER_REL_INBOX:
        MASTER_REL_MOTOR:
        MASTER_PWM_1:
        MASTER_PWM_2:
        MASTER_REL_FREE1:
        MASTER_REL_FREE2:
        MASTER_REL_FREE3;
        MASTER_EMERGENCY;
        MASTER_LOOP_TIME;
        MASTER_FRESH_FLAG
        */
        auto slave = splitString(parts[1], ';');

        if (slave.size() == 12)
        {
            int i = 0;
            for(auto &s : slave)
            {
                LOGF_DEBUG("slave %d: %s", i++, s.c_str());
            }

            auto digitalInputs = splitString(slave[0], ':');
            i = 8;
            for(const auto &input : digitalInputs)
            {
                inputs[i++] = (input == "0"); // inverted for some reason
            }
            i = 14;
            auto analogInputs = splitString(slave[1], ':');
            for(const auto &input : analogInputs)
            {
                sensors[i++] = std::stod(input);
            }
            shutterEncoder = std::stoi(slave[2]);
            auto temperatures = splitString(slave[3], ':');
            for(const auto &temp : temperatures)
            {
                sensors[i++] = std::stod(temp);
            }
            auto pressure_humidity = splitString(slave[4], ':');
            for(const auto &value : pressure_humidity)
            {
                sensors[i++] = std::stod(value);
            }
            std::string clouds = slave[5];
            std::string vcc = slave[6];
            std::string buttons = slave[7];
            auto relayPWM = splitString(slave[8], ':');
            i = 3;
            for(int r = 6; r < 9; r++)
            {
                relays[i++] = (relayPWM[r] == "1");
            }
            std::string emergency = slave[9];
            std::string loopTime = slave[10];
            std::string freshFlag = slave[11];

            // Voltage 064V needs some adjustment
            sensors[14] *= 10.0 * (5.0 / 0.1955);
        }

        /*
        SLAVE_INPD_DETECT_230LOSS:
        SLAVE_INPD_ENCODERA:
        SLAVE_INPD_HOMESENSOR:
        SLAVE_INPD_OPEN1:
        SLAVE_INPD_CLOSE1:
        SLAVE_INPD_RAINSENSOR:
        SLAVE_INPD_CLOUDSENSOR:
        SLAVE_INPD_TELESCOPE_A_H;
        SLAVE_INPA_VOLTAGE064:
        SLAVE_INPA_T_PT100:
        SLAVE_INPA_T_PCB;
        SLAVE_ENCODER_A;
        SLAVE_THERMOMETER_PCB:
        SLAVE_BAROMETER_TEMPERATURE:
        SLAVE_THERMOMETER_ONEWIRE_MOTOR:
        SLAVE_THERMOMETER_ONEWIRE_OUTSIDE:
        SLAVE_THERMOMETER_ONEWIRE_MIRROR_1:
        SLAVE_THERMOMETER_ONEWIRE_MIRROR_2:
        SLAVE_HIGROMETER_TEMPERATURE:
        SLAVE_PIROMETER_AMBIENT:
        SLAVE_PIROMETER_SENSOR;
        SLAVE_BAROMETER_PRESSURE:
        SLAVE_HIGROMETER_HUMIDITY;
        SLAVE_CLOUDS;
        SLAVE_Vcc;
        SLAVE_BUTTON_CW:
        SLAVE_BUTTON_CCW:
        SLAVE_BUTTON_FREE1:
        SLAVE_BUTTON_FREE2:
        SLAVE_BUTTON_FREE3:
        SLAVE_BUTTON_PAIR;
        SLAVE_REL_OPEN1:
        SLAVE_REL_CLOSE1:
        SLAVE_REL_INBOX:
        SLAVE_REL_MOTOR:
        SLAVE_PWM_1:
        SLAVE_PWM_2:
        SLAVE_REL_FREE1:
        SLAVE_REL_FREE2:
        SLAVE_REL_FREE3;
        SLAVE_EMERGENCY;
        SLAVE_LOOP_TIME;
        SLAVE_FRESH_FLAG
        */

        auto flags = splitString(parts[2], ':');
        if (flags.size() == 4)
        {
            int i = 0;
            for(auto &s : flags)
            {
                LOGF_DEBUG("flags %d: %s", i++, s.c_str());
            }
            rotaryLink = (flags[0] == "1");
            homing = (flags[1] == "1");
            moveShutterOnHome = (flags[2] == "1");
            calibrating = (flags[3] == "1");
        }
        /*
        IS_SLAVE_ONLINE:
        IS_COMPLEX_COMMAND_FINDING_HOME:
        IS_COMPLEX_COMMAND_MOVE_SHUTTER_ON_HOME:
        IS_COMPLEX_COMMAND_CALIBRATING
        */
    }
    else
    {
        LOGF_DEBUG("invalid status response: %s", status.c_str());
    }
    return 0;
}

uint32_t ScopeDomeArduino::getStatus()
{
    uint32_t status = 0;
    if(homing)
    {
        status |= STATUS_HOMING | STATUS_MOVING;
    }
    if(calibrating)
    {
        status |= STATUS_CALIBRATING | STATUS_MOVING;
    }
    if(moving)
    {
        status |= STATUS_MOVING;
    }
    LOGF_DEBUG("getStatus: %x", status);
    return status;
}

// Abstract versions
ISState ScopeDomeArduino::getInputState(AbstractInput input)
{
    bool state = false;

    switch(input)
    {
        case HOME:
            state = inputs[2];
            break;
        case OPEN1:
            state = inputs[11];
            break;
        case CLOSED1:
            state = inputs[12];
            break;
        case OPEN2:
        case CLOSED2:
            break;
        case ROTARY_LINK:
            state = rotaryLink;
            break;
        default:
            LOG_ERROR("invalid input");
            break;
    }
    return state ? ISS_ON : ISS_OFF;
}

int ScopeDomeArduino::setOutputState(AbstractOutput output, ISState onOff)
{
    std::string res;
    switch(output)
    {
        case RESET:
            // performCommand("resetSoft", res);
            break;
        case CW:
            if(onOff == ISS_ON)
            {
                performCommand("moveDome=CW", res);
            }
            else
            {
                performCommand("stopDome", res);
            }
            break;
        case CCW:
            if(onOff == ISS_ON)
            {
                performCommand("moveDome=CCW", res);
            }
            else
            {
                performCommand("stopDome", res);
            }
            break;
        default:
            LOG_ERROR("invalid output");
            return -1;
    }
    return 0;
}

int ScopeDomeArduino::getRotationCounter()
{
    // Make rotary encoder value similar to USB Card 2.1
    return encoderBaseValue - rotaryEncoder;
}

int ScopeDomeArduino::getRotationCounterExt()
{
    // Make relative to home sensor position like with USB Card 2.1
    return encoderBaseValue - rotaryEncoder;
}

bool ScopeDomeArduino::isCalibrationNeeded()
{
    return false;
}

void ScopeDomeArduino::abort()
{
    std::string res;
    performCommand("stopDome", res);
    performCommand("stopShutter", res);
}

void ScopeDomeArduino::calibrate()
{
    std::string res;
    performCommand("calibrate", res);
}

void ScopeDomeArduino::findHome()
{
    std::string res;
    performCommand("findHome", res);
}

void ScopeDomeArduino::controlShutter(ShutterOperation operation)
{
    std::string res;
    switch(operation)
    {
        case OPEN_SHUTTER:
            performCommand("moveShutter=OPEN", res);
            break;
        case CLOSE_SHUTTER:
            performCommand("moveShutter=CLOSE", res);
            break;
        case STOP_SHUTTER:
            performCommand("stopShutter", res);
            break;
        default:
            LOG_ERROR("Unknown shutter operation");
            break;
    }
}

void ScopeDomeArduino::resetCounter()
{
    // Doesn't seem to be needed as the counter resets by itself when passing home
//    std::string res;
//    char cmd[32] = {0};
//    snprintf(cmd, 31, "setEncoderA=%d", encoderBaseValue);
//    performCommand(cmd, res);
}

void ScopeDomeArduino::move(int steps)
{
    std::string res;
    if(steps < 0 )
    {
        char cmd[32] = {0};
        snprintf(cmd, 31, "moveDome=CCW:%d", -steps);
        performCommand(cmd, res);
    }
    else
    {
        char cmd[32] = {0};
        snprintf(cmd, 31, "moveDome=CW:%d", steps);
        performCommand(cmd, res);
    }
}

size_t ScopeDomeArduino::getNumberOfSensors()
{
    return 28;
}

ScopeDomeCard::SensorInfo ScopeDomeArduino::getSensorInfo(size_t index)
{
    ScopeDomeCard::SensorInfo info;
    switch(index)
    {
        case 0:
            info.propName = "VOLTAGE064";
            info.label = "Master 64V";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 1:
            info.propName = "T_PT100";
            info.label = "T_PT100";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 2:
            info.propName = "T_PCB";
            info.label = "T_PCB";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 3:
            info.propName = "THERMOMETER_PCB";
            info.label = "PCB thermometer";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 4:
            info.propName = "BAROMETER_TEMPERATURE";
            info.label = "Barometer temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 5:
            info.propName = "THERMOMETER_ONEWIRE_MOTOR";
            info.label = "Motor temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 6:
            info.propName = "THERMOMETER_ONEWIRE_OUTSIDE";
            info.label = "Outside temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 7:
            info.propName = "THERMOMETER_ONEWIRE_MIRROR_1";
            info.label = "Mirror 1 temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 8:
            info.propName = "THERMOMETER_ONEWIRE_MIRROR_2";
            info.label = "Mirror 2 temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 9:
            info.propName = "HIGROMETER_TEMPERATURE";
            info.label = "Higrometer temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 10:
            info.propName = "PIROMETER_AMBIENT";
            info.label = "Pirometer ambient temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 11:
            info.propName = "PIROMETER_SENSOR";
            info.label = "Pirometer sensor temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 12:
            info.propName = "BAROMETER_PRESSURE";
            info.label = "Barometer pressure";
            info.format = "%4.2f";
            info.minValue = 0;
            info.maxValue = 2000;
            break;
        case 13:
            info.propName = "HIGROMETER_HUMIDITY";
            info.label = "Higrometer humidity";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 14:
            info.propName = "S_VOLTAGE064";
            info.label = "Slave 64V";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 15:
            info.propName = "S_T_PT100";
            info.label = "Slave T_PT100";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 16:
            info.propName = "S_T_PCB";
            info.label = "Slave T_PCB";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 17:
            info.propName = "S_THERMOMETER_PCB";
            info.label = "Slave PCB thermometer";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 18:
            info.propName = "S_BAROMETER_TEMPERATURE";
            info.label = "Slave barometer temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 19:
            info.propName = "S_THERMOMETER_ONEWIRE_MOTOR";
            info.label = "Slave motor temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 20:
            info.propName = "S_THERMOMETER_ONEWIRE_OUTSIDE";
            info.label = "Slave outside temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 21:
            info.propName = "S_THERMOMETER_ONEWIRE_MIRROR_1";
            info.label = "Slave mirror 1 temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 22:
            info.propName = "S_THERMOMETER_ONEWIRE_MIRROR_2";
            info.label = "Slave mirror 2 temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 23:
            info.propName = "S_HIGROMETER_TEMPERATURE";
            info.label = "Slave higrometer temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 24:
            info.propName = "S_PIROMETER_AMBIENT";
            info.label = "Slave pirometer ambient temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 25:
            info.propName = "S_PIROMETER_SENSOR";
            info.label = "Slave pirometer sensor temperature";
            info.format = "%3.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 26:
            info.propName = "S_BAROMETER_PRESSURE";
            info.label = "Slave barometer pressure";
            info.format = "%4.2f";
            info.minValue = 0;
            info.maxValue = 2000;
            break;
        case 27:
            info.propName = "S_HIGROMETER_HUMIDITY";
            info.label = "Slave higrometer humidity";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        default:
            LOG_ERROR("invalid sensor index");
            break;
    }
    return info;
}

double ScopeDomeArduino::getSensorValue(size_t index)
{
    return sensors[index];
}

size_t ScopeDomeArduino::getNumberOfRelays()
{
    return 6;
}

ScopeDomeCard::RelayInfo ScopeDomeArduino::getRelayInfo(size_t index)
{
    ScopeDomeCard::RelayInfo info;
    switch(index)
    {
        case 0:
            info.propName = "RELAY_1";
            info.label = "Relay 1";
            break;
        case 1:
            info.propName = "RELAY_2";
            info.label = "Relay 2";
            break;
        case 2:
            info.propName = "RELAY_3";
            info.label = "Relay 3";
            break;
        case 3:
            info.propName = "S_RELAY_1";
            info.label = "Slave relay 1";
            break;
        case 4:
            info.propName = "S_RELAY_2";
            info.label = "Slave relay 2";
            break;
        case 5:
            info.propName = "S_RELAY_3";
            info.label = "Slave relay 3";
            break;
        default:
            LOG_ERROR("invalid relay index");
            break;
    }
    return info;
}

ISState ScopeDomeArduino::getRelayState(size_t index)
{
    return relays[index] ? ISS_ON : ISS_OFF;
}

void ScopeDomeArduino::setRelayState(size_t index, ISState state)
{
    std::string res;
    std::string cmd;
    if(index >= 3)
    {
        cmd = "slave=";
        index -= 3;
    }
    if(state == ISS_ON)
    {
        cmd += "switchOnFreeRelay=";
    }
    else
    {
        cmd += "switchOffFreeRelay=";
    }
    cmd += std::to_string(index + 1);
    performCommand(cmd, res);
}

size_t ScopeDomeArduino::getNumberOfInputs()
{
    return 16;
}

ScopeDomeCard::InputInfo ScopeDomeArduino::getInputInfo(size_t index)
{
    ScopeDomeCard::InputInfo info;
    switch(index)
    {
        case 0:
            info.propName = "DETECT_230LOSS";
            info.label = "Detect 230V loss";
            break;
        case 1:
            info.propName = "ENCODERA";
            info.label = "Rotary encoder";
            break;
        case 2:
            info.propName = "HOMESENSOR";
            info.label = "Home sensor";
            break;
        case 3:
            info.propName = "FREE1";
            info.label = "Free 1";
            break;
        case 4:
            info.propName = "FREE2";
            info.label = "Free 2";
            break;
        case 5:
            info.propName = "RAINSENSOR";
            info.label = "Rain sensor";
            break;
        case 6:
            info.propName = "CLOUDSENSOR";
            info.label = "Cloud sensor";
            break;
        case 7:
            info.propName = "TELESCOPE_A_H";
            info.label = "Telescope at home";
            break;
        case 8:
            info.propName = "S_DETECT_230LOSS";
            info.label = "Slave detect 230V loss";
            break;
        case 9:
            info.propName = "S_ENCODERA";
            info.label = "Shutter encoder";
            break;
        case 10:
            info.propName = "S_HOMESENSOR";
            info.label = "Slave home sensor";
            break;
        case 11:
            info.propName = "OPEN1";
            info.label = "Shutter 1 open";
            break;
        case 12:
            info.propName = "CLOSED1";
            info.label = "Shutter 1 closed";
            break;
        case 13:
            info.propName = "S_RAINSENSOR";
            info.label = "Slave rain sensor";
            break;
        case 14:
            info.propName = "S_CLOUDSENSOR";
            info.label = "Slave cloud sensor";
            break;
        case 15:
            info.propName = "S_TELESCOPE_A_H";
            info.label = "Slave telescope at home";
            break;
        default:
            LOG_ERROR("invalid input index");
            break;
    }
    return info;
}

ISState ScopeDomeArduino::getInputValue(size_t index)
{
    return inputs[index] ? ISS_ON : ISS_OFF;
}

void ScopeDomeArduino::setHomeSensorPolarity(HomeSensorPolarity polarity)
{
    std::string res;
    std::string cmd = "setHomeSignalLow=";

    switch(polarity)
    {
        case ACTIVE_HIGH:
            cmd += "0";
            break;
        case ACTIVE_LOW:
            cmd += "1";
            break;
    }
    performCommand(cmd, res);
}

std::vector<std::string> ScopeDomeArduino::splitString(const std::string &src, char splitChar)
{
    std::vector<std::string> results;

    size_t start = 0;
    size_t end;
    while( (end = src.find(splitChar, start)) != std::string::npos)
    {
        results.push_back(src.substr(start, end - start));
        start = end + 1;
    }
    results.push_back(src.substr(start, std::string::npos));
    return results;
}
