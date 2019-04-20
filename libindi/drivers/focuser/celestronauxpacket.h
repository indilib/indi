/*
    Celestron Focuser for SCT and EDGEHD

    Copyright (C) 2019 Chris Rowland
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include <vector>
#include <stdint.h>
#include <cstring>
#include <string>

/**
 * The Celestron Aux namespace contains classes required to communication with Celestron devices using the Auxiliary
 * command set. This includes communicating with the mount motors in addition to any auxiliary devices
 * such as focusers..etc.
 */
namespace Aux
{

typedef std::vector<uint8_t> buffer;

/**
 * @brief The Command enum includes all the command types sent to the various devices (motor, focuser..etc)
 */
enum Command
{
    MC_GET_POSITION = 0x01,         ///< return 24 bit position
    MC_GOTO_FAST = 0x02,            ///< send 24 bit target
    MC_SET_POSITION = 0x04,         ///< send 24 bit new position
    MC_SET_POS_GUIDERATE = 0x06,
    MC_SET_NEG_GUIDERATE = 0x07,
    MC_LEVEL_START = 0x0b,
    MC_SET_POS_BACKLASH = 0x10,    ///< 1 byte, 0-99
    MC_SET_NEG_BACKLASH = 0x11,    ///< 1 byte, 0-99
    MC_SLEW_DONE = 0x13,          ///< return 0xFF when move finished
    MC_GOTO_SLOW = 0x17,          ///< send 24 bit target
    MC_SEEK_INDEX = 0x19,
    MC_MOVE_POS = 0x24,           ///< send move rate 0-9
    MC_MOVE_NEG = 0x25,           ///< send move rate 0-9
    MC_GET_POS_BACKLASH = 0x40,   ///< 1 byte, 0-99
    MC_GET_NEG_BACKLASH = 0x41,   ///<  1 byte, 0-99

    // common to all devices (maybe)
    GET_VER = 0xfe,             ///< return 2 or 4 bytes major.minor.build

    // GPS device commands
    //GPS_GET_LAT = 0x01,
    //GPS_GET_LONG = 0x02,
    //GPS_GET_DATE = 0x03,
    //GPS_GET_YEAR = 0x04,
    //GPS_GET_TIME = 0x33,
    //GPS_TIME_VALID = 0x36,
    //GPS_LINKED = 0x37,

    //@{
    /** Focuser Commands */
    FOC_CALIB_ENABLE = 42,      ///< send 0 to start or 1 to stop
    FOC_CALIB_DONE = 43,        ///< returns 2 bytes [0] done, [1] state 0-12
    FOC_GET_HS_POSITIONS = 44,  ///< returns 2 ints low and high limits
    //@}
};

/**
 * @brief The Target enum Specifies the target device of the command.
 */
enum Target
{
    ANY = 0x00,
    MB = 0x01,
    HC = 0x04,        ///< Hand Controller
    HCP = 0x0d,
    AZM = 0x10,       ///< azimuth|hour angle axis motor
    ALT = 0x11,       ///< altitude|declination axis motor
    FOCUSER = 0x12,   ///< focuser motor
    APP = 0x20,
    NEX_REMOTE = 0x22,
    GPS = 0xb0,       ///< GPS Unit
    WiFi = 0xb5,      ///< WiFi Board
    BAT = 0xb6,
    CHG = 0xb7,
    LIGHT = 0xbf
};

/**
 * @brief The Packet class handles low-level communication with the Celestron devices
 */
class Packet
{
    public:
        Packet() {}
        Packet(Target source, Target destination, Command command, buffer data);
        Packet(Target source, Target destination, Command command);

        // packet contents
        static const uint8_t AUX_HDR = 0x3b;
        uint32_t length;
        Target source;
        Target destination;
        Command command;
        buffer data;

        void FillBuffer(buffer &buf);

        bool Parse (buffer buf);

    private:
        uint8_t checksum(buffer data);
};

/**
 * @brief The Communicator class handles high-level communication with the Celestron devices
 */
class Communicator
{
    public:
        Communicator();
        Communicator(Target source);
        // send command with data and reply
        bool sendCommand(int port, Target dest, Command cmd, buffer data, buffer &reply);
        // send command with reply but no data
        bool sendCommand(int port, Target dest, Command cmd, buffer &reply);
        // send command with data but no reply
        bool commandBlind(int port, Target dest, Command cmd, buffer data);

        Target source;

        static std::string Device;
        static void setDeviceName(const std::string &device)
        {
            Device =  device;
        }

    private:
        bool sendPacket(int port, Target dest, Command cmd, buffer data);
        bool readPacket(int port, Packet &reply);
};

}
