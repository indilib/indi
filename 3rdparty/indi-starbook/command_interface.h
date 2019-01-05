//
// Created by not7cd on 05/01/19.
//

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

    class CommandInterface {
    public:

        explicit CommandInterface(Connection::Curl *connection);

        std::string last_cmd_url;

        std::string last_response;

        ResponseCode Start() {
            return SendOkCommand("START");
        }

        ResponseCode Reset() {
            return SendOkCommand("RESET");
        }

        ResponseCode GotoRaDec(double ra, double dec);

        ResponseCode Align(double ra, double dec);

        ResponseCode Move();

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

        ResponseCode SetSpeed();

        ResponseCode SetPlace();

        ResponseCode SetTime();

    private:

        std::string SendCommand(std::string command);

        ResponseCode SendOkCommand(const std::string &cmd);

        ResponseCode ParseCommandResponse(const std::string &response);

        StarbookState ParseState(const std::string &value);

        Connection::Curl *connection = nullptr;

    };

}
