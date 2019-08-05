/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

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

#include <inditelescope.h>
#include "starbook_types.h"
#include "connectioncurl.h"

namespace starbook
{

typedef struct
{
    ln_equ_posn equ;
    StarbookState state;
    bool executing_goto;
} StatusResponse;

typedef struct
{
    std::string full_str;
    float major_minor;
} VersionResponse;

typedef struct
{
    LnLat posn;
    int tz;
} PlaceResponse;

typedef struct
{
    double x;
    double y;
} XYResponse;

constexpr int MIN_SPEED = 0;
constexpr int MAX_SPEED = 7;

class CommandInterface
{
    public:

        explicit CommandInterface(Connection::Curl *connection);

        const std::string &getLastCmdUrl() const;

        const std::string &getLastResponse() const;

        ResponseCode Start()
        {
            return SendOkCommand("START");
        }

        ResponseCode Reset()
        {
            return SendOkCommand("RESET");
        }

        ResponseCode GotoRaDec(double ra, double dec);

        ResponseCode Align(double ra, double dec);

        ResponseCode Move(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command);

        ResponseCode Move(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command);

        ResponseCode Home()
        {
            // as seen in https://github.com/farhi/matlab-starbook
            return SendOkCommand("GOHOME?HOME=0");
        }

        ResponseCode Stop()
        {
            return SendOkCommand("STOP");
        }

        ResponseCode GetStatus(StatusResponse &res);

        ResponseCode GetPlace(PlaceResponse &res);

        ResponseCode GetTime(ln_date &res);

        ResponseCode GetRound(long int &res);

        ResponseCode GetXY(XYResponse &res);

        ResponseCode Version(VersionResponse &res);

        ResponseCode SetSpeed(int speed);

        ResponseCode SetPlace(LnLat posn, short tz);

        ResponseCode SetTime(ln_date &local_time);

        void setDevice(std::string deviceName)
        {
            m_Device = deviceName;
        }

        ResponseCode SaveSetting()
        {
            return SendOkCommand("SAVESETTING");
        }

    private:

        Connection::Curl *connection = nullptr;

        std::string last_cmd_url;

        std::string last_response;

        std::string m_Device {"Starbook"};

        CommandResponse SendCommand(const std::string &command);

        ResponseCode SendOkCommand(const std::string &cmd);

        StarbookState ParseState(const std::string &value);

        VersionResponse ParseVersionResponse(const CommandResponse &response);

        StatusResponse ParseStatusResponse(const CommandResponse &response);

        PlaceResponse ParsePlaceResponse(const CommandResponse &response);

        ln_date ParseTimeResponse(const CommandResponse &response);

        XYResponse ParseXYResponse(const CommandResponse &response);

        long int ParseRoundResponse(const CommandResponse &response);

};

}
