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

#include <regex>
#include <inditelescope.h>
#include "command_interface.h"


namespace starbook {

    CommandInterface::CommandInterface(Connection::Curl *new_connection) : connection(new_connection) {}

    static std::string read_buffer;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        INDI_UNUSED(userp);
        size_t real_size = size * nmemb;
        read_buffer.append((char *) contents, real_size);
        return real_size;
    }

    std::string CommandInterface::SendCommand(std::string cmd) {
        int rc = 0;
        CURL *handle = connection->getHandle();
        std::ostringstream cmd_url;

        cmd_url << "http://" << connection->host() << ":" << connection->port() << "/" << cmd;
        last_cmd_url = cmd_url.str();

        last_response.clear();
        read_buffer.clear();
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "curl/7.58.0");

        /* send all data to this function  */
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(handle, CURLOPT_URL, cmd_url.str().c_str());

        rc = curl_easy_perform(handle);

        if (rc != CURLE_OK) {
            return "";
        }

        // all responses are hidden in HTML comments ...
        std::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
        std::smatch comment_match;
        if (!regex_search(read_buffer, comment_match, response_comment_re)) {
            return "";
        }

        last_response = comment_match[1].str();
        return comment_match[1].str();
    }

    ResponseCode CommandInterface::SendOkCommand(const std::string &cmd) {
        std::string response = CommandInterface::SendCommand(cmd);
        return ParseCommandResponse(response);
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
        std::string response = SendCommand("VERSION");
        if (response.empty()) {
//            LOG_ERROR("Version [ERROR]: Can't get firmware version");
            return ERROR_UNKNOWN;
        }

        std::regex param_re(R"(version=((\d+\.\d+)\w+))");
        std::smatch sm;
        if (!regex_search(response, sm, param_re)) {
//            LOGF_ERROR("Version [ERROR]: Can't parse firmware version %s", response.c_str());
            return ERROR_FORMAT;
        }

        res.full_str = sm[1].str();
        res.major_minor = std::stof(sm[2]);

        return OK;
    }

    ResponseCode CommandInterface::GetStatus(StatusResponse &res) {
        std::string cmd_res = SendCommand("GETSTATUS");
        if (cmd_res.empty()) {
            return ERROR_UNKNOWN;
        }

        lnh_equ_posn equ_posn = {{0, 0, 0},
                                 {0, 0, 0, 0}};
        res.executing_goto = false;

        std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
        std::smatch sm;

        while (regex_search(cmd_res, sm, param_re)) {
            std::string key = sm[1].str();
            std::string value = sm[2].str();

            if (key == "RA") {
                starbook::HMS ra(value);
                equ_posn.ra = ra;
            } else if (key == "DEC") {
                starbook::DMS dec(value);
                equ_posn.dec = dec;
            } else if (key == "STATE") {
                res.state = ParseState(value);
            } else if (key == "GOTO") {
                res.executing_goto = value == "1";
            }
            cmd_res = sm.suffix();
        }

        res.equ = {0, 0};
        ln_hequ_to_equ(&equ_posn, &res.equ);

        // we should be able to parse whole response
        if (cmd_res.empty()) {
            return OK;
        }
        return ERROR_UNKNOWN;
    }

    ResponseCode CommandInterface::SetTime(ln_date &utc) {
        std::ostringstream cmd;
        cmd << "SETTIME?TIME=" << static_cast<starbook::UTC &>(utc);
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

    ResponseCode CommandInterface::SetSpeed(int speed) {
        if (speed < MIN_SPEED || speed > MAX_SPEED)
            return ERROR_FORMAT;

        std::ostringstream cmd;
        cmd << "SETSPEED?speed=" << speed;
        return SendOkCommand(cmd.str());
    }

    const std::string &CommandInterface::getLastCmdUrl() const {
        return last_cmd_url;
    }

    const std::string &CommandInterface::getLastResponse() const {
        return last_response;
    }
}