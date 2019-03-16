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

namespace starbook {

    typedef struct {
        ln_equ_posn equ;
        StarbookState state;
        bool executing_goto;
    } StatusResponse;

    typedef struct {
        std::string full_str;
        float major_minor;
    } VersionResponse;

    const int MIN_SPEED = 0;
    const int MAX_SPEED = 7;

    class CommandInterface {
    public:

        explicit CommandInterface(Connection::Curl *connection);

        const std::string &getLastCmdUrl() const;

        const std::string &getLastResponse() const;

        ResponseCode Start() {
            return SendOkCommand("START");
        }

        ResponseCode Reset() {
            return SendOkCommand("RESET");
        }

        ResponseCode GotoRaDec(double ra, double dec);

        ResponseCode Align(double ra, double dec);

        ResponseCode Move(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command);

        ResponseCode Move(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command);

        ResponseCode Home() {
            return SendOkCommand("HOME");
        }

        ResponseCode Stop() {
            return SendOkCommand("STOP");
        }

        ResponseCode GetStatus(StatusResponse &res);

        ResponseCode GetPlace();

        ResponseCode GetTime();

        ResponseCode GetRound();

        ResponseCode GetXY();

        ResponseCode Version(VersionResponse &res);

        ResponseCode SetSpeed(int speed);

        ResponseCode SetPlace();

        ResponseCode SetTime(ln_date &utc);

        ResponseCode SaveSetting() {
            return SendOkCommand("SAVESETTING");
        }

    private:

        Connection::Curl *connection = nullptr;

        std::string last_cmd_url;

        std::string last_response;

        std::string SendCommand(std::string command);

        ResponseCode SendOkCommand(const std::string &cmd);

        ResponseCode ParseCommandResponse(const std::string &response);

        StarbookState ParseState(const std::string &value);

    };

}
