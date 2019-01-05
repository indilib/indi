//
// Created by not7cd on 05/01/19.
//

#include <regex>
#include "indi_starbook.h"

namespace starbook {

    CommandInterface::CommandInterface(Connection::Curl *new_connection) {
        connection = new_connection;
    }

    static std::string read_buffer;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        INDI_UNUSED(userp);
        size_t real_size = size * nmemb;
        read_buffer.append((char *) contents, real_size);
        return real_size;
    }

    std::__cxx11::string CommandInterface::SendCommand(std::__cxx11::string cmd) {
        int rc = 0;
        CURL *handle = connection->getHandle();
        std::ostringstream cmd_url;

        cmd_url << "http://" << connection->host() << ":" << connection->port() << "/" << cmd;
        last_cmd_url = cmd_url.str();

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
        std::__cxx11::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
        std::__cxx11::smatch comment_match;
        if (!regex_search(read_buffer, comment_match, response_comment_re)) {
            return "";
        }

        return comment_match[1].str();
    }

    ResponseCode CommandInterface::SendOkCommand(const std::__cxx11::string &cmd) {
        std::__cxx11::string response = CommandInterface::SendCommand(cmd);
        return ParseCommandResponse(response);
    }

    ResponseCode CommandInterface::ParseCommandResponse(const std::__cxx11::string &response) {
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

    starbook::StarbookState starbook::CommandInterface::ParseState(const std::__cxx11::string &value) {
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
}