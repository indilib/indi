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

#include "indi_starbook.h"
#include "config.h"

#include <indicom.h>
#include <connectionplugins/connectiontcp.h>
#include <memory>
#include <cstring>
#include <regex>

static std::string readBuffer;

std::unique_ptr<Starbook> starbook_driver(new Starbook());

void ISGetProperties(const char *dev) {
    starbook_driver->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
    starbook_driver->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
    starbook_driver->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
    starbook_driver->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n) {
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}


void ISSnoopDevice(XMLEle *root) {
    INDI_UNUSED(root);
}

Starbook::Starbook() {
    LOG_INFO("Staring driver");
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT,
                           1);

    setTelescopeConnection(CONNECTION_TCP);
    curl_global_init(CURL_GLOBAL_ALL);

}

Starbook::~Starbook() {
    curl_global_cleanup();
}

bool Starbook::initProperties() {
    Telescope::initProperties();

    if (getTelescopeConnection() & CONNECTION_TCP) {
        tcpConnection->setDefaultHost("127.0.0.1");
        tcpConnection->setDefaultPort(5000);
    }

    addDebugControl();

    state = SB_INIT;

    return true;
}

bool Starbook::Connect() {
    handle = curl_easy_init();
    return Telescope::Connect();
}

bool Starbook::Disconnect() {
    curl_easy_cleanup(handle);
    return Telescope::Disconnect();
}

const char *Starbook::getDefaultName() {
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus() {
    LOG_DEBUG("Status! Sending GETSTATUS command");
    bool res = SendCommand("GETSTATUS");

    // Not safe I think
    std::string response = readBuffer;
    std::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
    std::smatch comment_match;
    if (!std::regex_search(response, comment_match, response_comment_re)) {
        return false;
    }

    response = comment_match[1].str();

    LOG_INFO(response.c_str());

    std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
    std::smatch sm;

    lnh_equ_posn equ_posn = {{0, 0, 0},
                             {0, 0, 0, 0}};

    while (regex_search(response, sm, param_re)) {
//        LOG_INFO(sm.str().c_str());
        std::string key = sm[1].str();
        std::string value = sm[2].str();

        if (key == "RA") {
            starbook::HMS ra(value);
            equ_posn.ra = ra;
        } else if (key == "DEC") {
            starbook::DMS dec(value);
            equ_posn.dec = dec;
        } else if (key == "STATE") {
            if (value == "SCOPE") {
                state = StarbookState::SB_SCOPE;
            } else if (value == "GUIDE") {
                state = StarbookState::SB_GUIDE;
            } else if (value == "INIT") {
                state = StarbookState::SB_INIT;
            } else {
                LOGF_ERROR("Unknown state %s", value.c_str());
            }
            LOGF_DEBUG("Parsed STATE %i", state);
        }

        response = sm.suffix();
    }

    if (res) {
        ln_equ_posn d_equ_posn = {0, 0};
        ln_hequ_to_equ(&equ_posn, &d_equ_posn);
        LOGF_DEBUG("Parsed RADEC %d, %d", d_equ_posn.ra / 15, d_equ_posn.dec);
        NewRaDec(d_equ_posn.ra / 15, d_equ_posn.dec); // CONVERSION
        return true;
    }
    return false;
}

bool Starbook::Goto(double ra, double dec) {
    ra = ra * 15; // CONVERSION
    std::ostringstream params;
    params << "?" << starbook::Equ{ra, dec};

    LOGF_INFO("GoTo! %s", params.str().c_str());

    bool res = SendCommand("GOTORADEC" + params.str());
    return res;
}

bool Starbook::Abort() {
    LOG_INFO("Aborting!");
    bool res = SendCommand("STOP");
    return res;
}


bool Starbook::Sync(double ra, double dec) {
    ra = ra * 15; // CONVERSION
    std::ostringstream params;
    params << "?" << starbook::Equ{ra, dec};

    LOGF_INFO("Sync! %s", params.str().c_str());

    bool res = SendCommand("ALIGN" + params.str());
    return res;
}

bool Starbook::Park() {
    // TODO Park
    LOG_WARN("HOME command is unstable");
    return SendCommand("HOME");
}


bool Starbook::UnPark() {
    // TODO UnPark
    LOG_WARN("Always unparked");
    return true;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    INDI_UNUSED(userp);
    size_t real_size = size * nmemb;
    readBuffer.append((char *) contents, real_size);
    return real_size;
}

bool Starbook::SendCommand(std::string cmd) {

    int rc = 0;

    readBuffer.clear();
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "curl/7.58.0");

    /* send all data to this function  */
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);

    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);

    // TODO: https://github.com/indilib/indi/pull/779
    // HACK: Just use CURL, and use tcpConnection as input
    std::ostringstream cmd_url;
    cmd_url << "http://" << tcpConnection->host() << "/" << cmd;

    curl_easy_setopt(handle, CURLOPT_URL, cmd_url.str().c_str());

    rc = curl_easy_perform(handle);

    return rc == CURLE_OK;
}

bool Starbook::Handshake() {
    LOG_DEBUG("Handshake");
    return Telescope::Handshake();
}
