/*
    Celestron Aux Command

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2021 Jasem Mutlaq

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

#include "celestron_dewpower_auxproto.h"

#include <indilogger.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define READ_TIMEOUT 1 		// s
#define CTS_TIMEOUT 100		// ms
#define RTS_DELAY 50		// ms

#define BUFFER_SIZE 512
int MAX_CMD_LEN = 32;

uint8_t AUXCommand::DEBUG_LEVEL = 0;
char AUXCommand::DEVICE_NAME[64] = {0};
//////////////////////////////////////////////////
/////// Utility functions
//////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void logBytes(unsigned char *buf, int n, const char *deviceName, uint32_t debugLevel)
{
    char hex_buffer[BUFFER_SIZE] = {0};
    for (int i = 0; i < n; i++)
        sprintf(hex_buffer + 3 * i, "%02X ", buf[i]);

    if (n > 0)
        hex_buffer[3 * n - 1] = '\0';

    DEBUGFDEVICE(deviceName, debugLevel, "[%s]", hex_buffer);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::logResponse()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < m_Data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", m_Data[i]);

    if (m_Data.size() > 0)
        hex_buffer[3 * m_Data.size() - 1] = '\0';

    const char * c = commandName(m_Command);
    const char * s = moduleName(m_Source);
    const char * d = moduleName(m_Destination);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", m_Command);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", m_Source);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", m_Destination);

    if (m_Data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s", part1, part2, part3);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::logCommand()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < m_Data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", m_Data[i]);

    if (m_Data.size() > 0)
        hex_buffer[3 * m_Data.size() - 1] = '\0';

    const char * c = commandName(m_Command);
    const char * s = moduleName(m_Source);
    const char * d = moduleName(m_Destination);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", m_Command);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", m_Source);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", m_Destination);

    if (m_Data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s", part1, part2, part3);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::setDebugInfo(const char *deviceName, uint8_t debugLevel)
{
    snprintf(DEVICE_NAME, sizeof(DEVICE_NAME), "%s", deviceName);
    DEBUG_LEVEL = debugLevel;
}
////////////////////////////////////////////////
//////  AUXCommand class
////////////////////////////////////////////////

AUXCommand::AUXCommand()
{
    m_Data.reserve(MAX_CMD_LEN);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(const AUXBuffer &buf)
{
    m_Data.reserve(MAX_CMD_LEN);
    parseBuf(buf);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination, const AUXBuffer &data)
{
    m_Command = command;
    m_Source = source;
    m_Destination = destination;
    m_Data.reserve(MAX_CMD_LEN);
    m_Data = data;
    len  = 3 + m_Data.size();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination)
{
    m_Command = command;
    m_Source = source;
    m_Destination = destination;
    m_Data.reserve(MAX_CMD_LEN);
    len = 3 + m_Data.size();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char * AUXCommand::commandName(AUXCommands command) const
{
    switch (command)
    {
        case PORTCTRL_GET_INPUT_POWER:
            return "PORTCTRL_GET_INPUT_POWER";
        case PORTCTRL_SET_POWER_ENABLED:
            return "PORTCTRL_SET_POWER_ENABLED";
        case PORTCTRL_GET_POWER_ENABLED:
            return "PORTCTRL_GET_POWER_ENABLED";
        case PORTCTRL_SET_EXT_CURRENT_LIMIT:
            return "PORTCTRL_SET_EXT_CURRENT_LIMIT";
        case PORTCTRL_GET_EXT_CURRENT_LIMIT:
            return "PORTCTRL_GET_EXT_CURRENT_LIMIT";
        case PORTCTRL_GET_NUMBER_OF_PORTS:
            return "PORTCTRL_GET_NUMBER_OF_PORTS";
        case PORTCTRL_GET_PORT_INFO:
            return "PORTCTRL_GET_PORT_INFO";
        case PORTCTRL_GET_DH_PORT_INFO:
            return "PORTCTRL_GET_DH_PORT_INFO";
        case PORTCTRL_RESET_FUSE:
            return "PORTCTRL_RESET_FUSE";
        case PORTCTRL_SET_PORT_ENABLED:
            return "PORTCTRL_SET_PORT_ENABLED";
        case PORTCTRL_SET_PORT_VOLTAGE:
            return "PORTCTRL_SET_PORT_VOLTAGE";
        case PORTCTRL_DH_ENABLE_AUTO:
            return "PORTCTRL_DH_ENABLE_AUTO";
        case PORTCTRL_DH_ENABLE_MANUAL:
            return "PORTCTRL_DH_ENABLE_MANUAL";
        case PORTCTRL_GET_ENVIRONMENT:
            return "PORTCTRL_GET_ENVIRONMENT";
        case PORTCTRL_ENABLE_SELF_HEATER:
            return "PORTCTRL_ENABLE_SELF_HEATER";
        case PORTCTRL_SELF_HEATER_STATUS:
            return "PORTCTRL_SELF_HEATER_STATUS";
        case PORTCTRL_GET_HEATER_LEVEL:
            return "PORTCTRL_GET_HEATER_LEVEL";
        case PORTCTRL_FACTORY_RESET:
            return "PORTCTRL_FACTORY_RESET";
        case PORTCTRL_GET_VERSION:
            return "PORTCTRL_GET_VERSION";
        case PORTCTRL_SET_LED_BRIGHTNESS:
            return "PORTCTRL_SET_LED_BRIGHTNESS";
        case PORTCTRL_GET_LED_BRIGHTNESS:
            return "PORTCTRL_GET_LED_BRIGHTNESS";
        default :
            return nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int AUXCommand::responseDataSize()
{
    switch (m_Command)
    {
        case PORTCTRL_GET_INPUT_POWER:
            return 6; // data[0:1] voltage, data[2:3] current, data[4] voltage status, data[5] overcurrent
        case PORTCTRL_GET_POWER_ENABLED:
            return 1; // <Enabled/Disabled boolean>
        case PORTCTRL_SET_EXT_CURRENT_LIMIT:
            return 0; // CMD
        case PORTCTRL_GET_EXT_CURRENT_LIMIT:
            return 4; // <0:1 LIMIT (mA)> <2:3 MAX LIMIT (mA)>
        case PORTCTRL_GET_NUMBER_OF_PORTS:
            return 1; // <Number of Ports>
        case PORTCTRL_GET_PORT_INFO:
            return 7; // <0 type><1 enabled><2 isShorted><3:4 power(mW)><5:6 VoltageLevel (mV)>
        case PORTCTRL_GET_DH_PORT_INFO:
            return 6; // <0 type><nmode><2 power level><3:4 power(mW)><5 aggression level (C)> (min size)
        case PORTCTRL_RESET_FUSE:
            return 0; // CMD
        case PORTCTRL_SET_PORT_ENABLED:
            return 0; // CMD
        case PORTCTRL_SET_PORT_VOLTAGE:
            return 0; // CMD
        case PORTCTRL_DH_ENABLE_AUTO:
            return 0; // CMD
        case PORTCTRL_DH_ENABLE_MANUAL:
            return 0; // CMD
        case PORTCTRL_GET_ENVIRONMENT:
            return 9; // <0:3 ambient temp mC><4:7 dew point mC><8 humidity>
        case PORTCTRL_ENABLE_SELF_HEATER:
            return 0; // CMD
        case PORTCTRL_SELF_HEATER_STATUS:
            return 1; // <true/false>
        case PORTCTRL_GET_HEATER_LEVEL:
            return 1; // <0 heater level (0 to 255)>
        case PORTCTRL_FACTORY_RESET:
            return 0; // CMD
        case PORTCTRL_GET_VERSION:
            return 2; // version number (assuming 2 bytes for version)
        case PORTCTRL_SET_LED_BRIGHTNESS:
            return 0; // CMD
        case PORTCTRL_GET_LED_BRIGHTNESS:
            return 1; // <0 brightness level>
        default :
            return -1;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char * AUXCommand::moduleName(AUXTargets n)
{
    switch (n)
    {
        case HC :
            return "HC";
        case APP :
            return "APP";
        case DEW_POWER_CTRL:
            return "DEW_POWER_CTRL";
        default :
            return nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::fillBuf(AUXBuffer &buf)
{
    buf.resize(len + 3);

    buf[0] = 0x3b;
    buf[1] = len;
    buf[2] = m_Source;
    buf[3] = m_Destination;
    buf[4] = m_Command;
    for (uint32_t i = 0; i < m_Data.size(); i++)
    {
        buf[i + 5] = m_Data[i];
    }
    buf.back() = checksum(buf);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::parseBuf(AUXBuffer buf)
{
    len   = buf[1];
    m_Source   = (AUXTargets)buf[2];
    m_Destination   = (AUXTargets)buf[3];
    m_Command   = (AUXCommands)buf[4];
    m_Data  = AUXBuffer(buf.begin() + 5, buf.end() - 1);
    valid = (checksum(buf) == buf.back());
    if (valid == false)
    {
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "Checksum error: %02x vs. %02x", checksum(buf), buf.back());
    };
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::parseBuf(AUXBuffer buf, bool do_checksum)
{
    (void)do_checksum;

    len   = buf[1];
    m_Source   = (AUXTargets)buf[2];
    m_Destination   = (AUXTargets)buf[3];
    m_Command   = (AUXCommands)buf[4];
    if (buf.size() > 5)
        m_Data  = AUXBuffer(buf.begin() + 5, buf.end());
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
unsigned char AUXCommand::checksum(AUXBuffer buf)
{
    int l  = buf[1];
    int cs = 0;
    for (int i = 1; i < l + 2; i++)
    {
        cs += buf[i];
    }
    return (unsigned char)(((~cs) + 1) & 0xFF);
}

/////////////////////////////////////////////////////////////////////////////////////
/// Return 8, 16, or 24bit value as dictacted by the data response size.
/////////////////////////////////////////////////////////////////////////////////////
uint32_t AUXCommand::getData()
{
    uint32_t value = 0;
    switch (m_Data.size())
    {
        case 3:
            value = (m_Data[0] << 16) | (m_Data[1] << 8) | m_Data[2];
            break;

        case 2:
            value = (m_Data[0] << 8) | m_Data[1];
            break;

        case 1:
            value = m_Data[0];
            break;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////////////
/// Set encoder position in steps.
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::setData(uint32_t value, uint8_t bytes)
{
    m_Data.resize(bytes);
    switch (bytes)
    {
        case 1:
            len = 4;
            m_Data[0] = static_cast<uint8_t>(value >> 0 & 0xff);
            break;
        case 2:
            len = 5;
            m_Data[1] = static_cast<uint8_t>(value >>  0 & 0xff);
            m_Data[0] = static_cast<uint8_t>(value >>  8 & 0xff);
            break;

        case 3:
        default:
            len = 6;
            m_Data[2] = static_cast<uint8_t>(value >>  0 & 0xff);
            m_Data[1] = static_cast<uint8_t>(value >>  8 & 0xff);
            m_Data[0] = static_cast<uint8_t>(value >> 16 & 0xff);
            break;
    }
}
