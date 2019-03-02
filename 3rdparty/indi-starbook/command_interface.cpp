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

#include <exception>
#include <regex>
#include <inditelescope.h>
#include "command_interface.h"


namespace starbook {

    CommandInterface::CommandInterface(Connection::Curl *new_connection) : connection(new_connection) {}

    static std::string read_buffer;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        INDI_UNUSED(userp);
        size_t real_size = size * nmemb;
        read_buffer.append(static_cast<char *>(contents), real_size);
        return real_size;
    }

    std::string CommandInterface::SendCommand(std::string cmd) {
        int rc = 0;
        CURL *handle = connection->getHandle();
        last_response.clear();
        read_buffer.clear();
        std::ostringstream cmd_url;

        cmd_url << "http://" << connection->host() << ":" << connection->port() << "/" << cmd;
        last_cmd_url = cmd_url.str();


        curl_easy_setopt(handle, CURLOPT_USERAGENT, "curl/7.58.0");
        /* send all data to this function  */
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(handle, CURLOPT_URL, cmd_url.str().c_str());

        rc = curl_easy_perform(handle);

        if (rc != CURLE_OK) {
            throw std::runtime_error("curl error " + std::to_string(rc));
        }

        // all responses are hidden in HTML comments ...
        std::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
        std::smatch comment_match;
        if (!regex_search(read_buffer, comment_match, response_comment_re)) {
            throw std::runtime_error("parsing error, response not found ");
        }

        last_response = comment_match[1].str();
        if (last_response.empty()) {
            throw std::runtime_error("parsing error, response empty");
        }

        return comment_match[1].str();
    }

    ResponseCode CommandInterface::SendOkCommand(const std::string &cmd) {
        std::string response = CommandInterface::SendCommand(cmd);
        return ParseCommandResponse(response);
    }

    ResponseCode CommandInterface::GotoRaDec(double ra, double dec) {
        std::ostringstream cmd;
        cmd << "GOTORADEC?" << starbook::Equ{ra, dec};
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Align(double ra, double dec) {
        std::ostringstream cmd;
        cmd << "ALIGN?" << starbook::Equ{ra, dec};
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Version(VersionResponse &res) {
        std::string cmd_res = SendCommand("VERSION");

        try {
            res = ParseVersionResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::GetStatus(StatusResponse &res) {
        std::string cmd_res = SendCommand("GETSTATUS");

        try {
            res = ParseStatusResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::GetPlace(PlaceResponse &res) {
        std::string cmd_res = SendCommand("GETSTATUS");

        try {
            res = ParsePlaceResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::GetTime(ln_date &res) {
        std::string cmd_res = SendCommand("GETSTATUS");

        try {
            res = ParseTimeResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::GetRound(long int &res) {
        std::string cmd_res = SendCommand("GETSTATUS");

        try {
            res = ParseRoundResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::GetXY(XYResponse &res) {
        std::string cmd_res = SendCommand("GETSTATUS");

        try {
            res = ParseXYResponse(cmd_res);
        }
        catch (std::exception &e) {
            return ERROR_FORMAT;
        }
        return OK;
    }

    ResponseCode CommandInterface::SetTime(ln_date &utc) {
        std::ostringstream cmd;
        cmd << "SETTIME?TIME=" << static_cast<starbook::UTC &>(utc);
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::SetSpeed(int speed) {
        if (speed < MIN_SPEED || speed > MAX_SPEED)
            return ERROR_FORMAT;

        std::ostringstream cmd;
        cmd << "SETSPEED?speed=" << speed;
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Move(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command) {
        std::ostringstream cmd;
        cmd << "MOVE?NORTH="
            << (dir == DIRECTION_NORTH && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0)
            << "&SOUTH="
            << (dir == DIRECTION_SOUTH && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0);

        return SendOkCommand(cmd.str());
    }


    ResponseCode CommandInterface::Move(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
        std::ostringstream cmd;
        cmd << "MOVE?WEST="
            << (dir == DIRECTION_WEST && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0)
            << "&EAST="
            << (dir == DIRECTION_EAST && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0);
        return SendOkCommand(cmd.str());
    }

    const std::string &CommandInterface::getLastCmdUrl() const {
        return last_cmd_url;
    }

    const std::string &CommandInterface::getLastResponse() const {
        return last_response;
    }

    StatusResponse CommandInterface::ParseStatusResponse(const std::string &str) {
        StatusResponse result;
        std::string str_remaining = str;

        lnh_equ_posn equ_posn = {{0, 0, 0},
                                 {0, 0, 0, 0}};
        result.executing_goto = false;

        std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
        std::smatch sm;

        while (regex_search(str_remaining, sm, param_re)) {
            std::string key = sm[1].str();
            std::string value = sm[2].str();

            if (key == "RA") {
                starbook::HMS ra(value);
                equ_posn.ra = ra;
            } else if (key == "DEC") {
                starbook::DMS dec(value);
                equ_posn.dec = dec;
            } else if (key == "STATE") {
                result.state = ParseState(value);
            } else if (key == "GOTO") {
                result.executing_goto = value == "1";
            }
            str_remaining = sm.suffix();
        }

        result.equ = {0, 0};
        ln_hequ_to_equ(&equ_posn, &result.equ);

        if (!str_remaining.empty()) throw std::runtime_error("parsing error, couldn't parse full response");

        return result;
    }

    VersionResponse CommandInterface::ParseVersionResponse(const std::string &response) {
        VersionResponse result;

        std::regex param_re(R"(version=((\d+\.\d+)\w+))");
        std::smatch sm;
        if (!regex_search(response, sm, param_re)) {
            throw;
        }

        result.full_str = sm[1].str();
        result.major_minor = std::stof(sm[2]);
        return result;
    }

    StarbookState CommandInterface::ParseState(const std::string &value) {
        if (value == "SCOPE")
            return SCOPE;
        else if (value == "GUIDE")
            return GUIDE;
        else if (value == "USER")
            return USER;
        else if (value == "INIT")
            return INIT;

        return UNKNOWN;
    }

    ResponseCode CommandInterface::ParseCommandResponse(const std::string &response) {
        if (response == "OK")
            return OK;
        else if (response == "ERROR:FORMAT")
            return ERROR_FORMAT;
        else if (response == "ERROR:ILLEGAL STATE")
            return ERROR_ILLEGAL_STATE;
        else if (response == "ERROR:BELOW HORIZONE") /* it's not a typo */
            return ERROR_BELOW_HORIZON;

        return ERROR_UNKNOWN;
    }
}