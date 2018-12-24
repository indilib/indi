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

static std::unique_ptr<Starbook> starbook_driver(new Starbook());

void ISGetProperties(const char* dev)
{
    starbook_driver->ISGetProperties(dev);
}

void ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    starbook_driver->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n)
{
    starbook_driver->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n)
{
    starbook_driver->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char* dev, const char* name, int sizes[], int blobsizes[], char* blobs[],
    char* formats[], char* names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle* root)
{
    INDI_UNUSED(root);
}

Starbook::Starbook()
{
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MINOR);
    SetTelescopeCapability(
            TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME, 1);

    setTelescopeConnection(CONNECTION_TCP);
    curl_global_init(CURL_GLOBAL_ALL);
}

Starbook::~Starbook()
{
    curl_global_cleanup();
}

bool Starbook::initProperties()
{
    Telescope::initProperties();

    IUFillText(&VersionT[0], "Version", "Version", "");
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);


    if (getTelescopeConnection() & CONNECTION_TCP)
    {
        tcpConnection->setDefaultHost("127.0.0.1");
        tcpConnection->setDefaultPort(5000);
    }

    addDebugControl();

    state = starbook::UNKNOWN;

    return true;
}

bool Starbook::updateProperties() {
    Telescope::updateProperties();
    if (isConnected()) {
        defineText(&VersionInfo);
    } else {
        deleteProperty(VersionInfo.name);
    }
    return true;
}

bool Starbook::Connect()
{
    handle = curl_easy_init();
    bool rc = Telescope::Connect();
    if (rc) {
        getFirmwareVersion();
    }
    return rc;
}

bool Starbook::Disconnect()
{
    curl_easy_cleanup(handle);
    return Telescope::Disconnect();
}

const char* Starbook::getDefaultName()
{
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus()
{
    LOG_DEBUG("Status! Sending GETSTATUS command");
    std::string response = SendCommand("GETSTATUS");
    if (response.empty()) {
        return false;
    }

    LOG_DEBUG(response.c_str());

    std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
    std::smatch sm;

    lnh_equ_posn equ_posn = { { 0, 0, 0 }, { 0, 0, 0, 0 } };

    while (regex_search(response, sm, param_re))
    {
        std::string key = sm[1].str();
        std::string value = sm[2].str();

        if (key == "RA")
        {
            starbook::HMS ra(value);
            equ_posn.ra = ra;
        }
        else if (key == "DEC")
        {
            starbook::DMS dec(value);
            equ_posn.dec = dec;
        }
        else if (key == "STATE")
        {
            state = ParseState(value);
            LOGF_DEBUG("Parsed STATE %i", state);
        }
        response = sm.suffix();
    }

    ln_equ_posn d_equ_posn = {0, 0};
    ln_hequ_to_equ(&equ_posn, &d_equ_posn);
    LOGF_DEBUG("Parsed RADEC %d, %d", d_equ_posn.ra / 15, d_equ_posn.dec);
    NewRaDec(d_equ_posn.ra / 15, d_equ_posn.dec); // CONVERSION
    return true;
}

bool Starbook::Goto(double ra, double dec)
{
    ra = ra * 15; // CONVERSION
    std::ostringstream params;
    params << "?" << starbook::Equ{ ra, dec };

    LOGF_INFO("GoTo! %s", params.str().c_str());

    return SendOkCommand("GOTORADEC" + params.str());
}

bool Starbook::Sync(double ra, double dec)
{
    ra = ra * 15; // CONVERSION
    std::ostringstream params;
    params << "?" << starbook::Equ{ ra, dec };

    LOGF_INFO("Sync! %s", params.str().c_str());
    // TODO: check if distance to new ra, dec > 10 degrees
    return SendOkCommand("ALIGN" + params.str());
}

bool Starbook::Abort()
{
    LOG_INFO("Aborting!");
    return SendOkCommand("STOP");
}

bool Starbook::Park()
{
    // TODO Park
    LOG_WARN("HOME command is unstable");
    return SendOkCommand("HOME");
}

bool Starbook::UnPark()
{
    // TODO UnPark
    LOG_WARN("Always unparked");
    return true;
}

bool Starbook::MoveNS(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command) {
    std::ostringstream params;
    params << "?NORTH="
           << ((dir == DIRECTION_NORTH && command == MOTION_START) ? 1 : 0)
           << "&SOUTH="
           << ((dir == DIRECTION_SOUTH && command == MOTION_START) ? 1 : 0);

    LOGF_INFO("Move! %s", params.str().c_str());
    return SendOkCommand("MOVE" + params.str());
}

bool Starbook::MoveWE(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
    std::ostringstream params;
    params << "?WEST="
           << ((dir == DIRECTION_WEST && command == MOTION_START) ? 1 : 0)
           << "&EAST="
           << ((dir == DIRECTION_EAST && command == MOTION_START) ? 1 : 0);

    LOGF_INFO("Move! %s", params.str().c_str());
    return SendOkCommand("MOVE" + params.str());
}

bool Starbook::updateTime(ln_date *utc, double utc_offset) {
    INDI_UNUSED(utc_offset);

    std::ostringstream params;
    params << "?TIME=" << *static_cast<starbook::UTC *>(utc);

    LOGF_INFO("Time! %s", params.str().c_str());

    return SendOkCommand("SETTIME" + params.str());

}

bool Starbook::getFirmwareVersion() {
    std::string response = SendCommand("VERSION");
    if (response.empty()) {
        LOG_ERROR("Can't get firmware version");
        return false;
    }

    std::regex param_re(R"(version=((\d+\.\d+)\w+))");
    std::smatch sm;
    if (!regex_search(response, sm, param_re)) {
        LOG_ERROR("Can't parse firmware version");
        return false;
    }

    std::string version_full = sm[1].str();
    float version = std::stof(sm[2]);

    if (version < 2.7) {
        LOGF_WARN("Version %s (< 2.7) not well supported", version_full.c_str());
    }

    IUSaveText(&VersionT[0], version_full.c_str());
    IDSetText(&VersionInfo, nullptr);

    return true;
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    INDI_UNUSED(userp);
    size_t real_size = size * nmemb;
    readBuffer.append((char*)contents, real_size);
    return real_size;
}

std::string Starbook::SendCommand(std::string cmd)
{
    int rc = 0;

    readBuffer.clear();
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "curl/7.58.0");

    /* send all data to this function  */
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);

    // TODO: https://github.com/indilib/indi/pull/779
    // HACK: Just use CURL, and use tcpConnection as input
    std::ostringstream cmd_url;
    cmd_url << "http://" << tcpConnection->host() << "/" << cmd;
    curl_easy_setopt(handle, CURLOPT_URL, cmd_url.str().c_str());

    rc = curl_easy_perform(handle);

    if (rc != CURLE_OK) {
        return "";
    }

    // all responses are hidden in HTML comments ...
    std::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
    std::smatch comment_match;
    if (!std::regex_search(readBuffer, comment_match, response_comment_re)) {
        return "";
    }

    return comment_match[1].str();
}

bool Starbook::SendOkCommand(const std::string &cmd) {
    std::string response = SendCommand(cmd);
    starbook::ResponseCode code = ParseCommandResponse(response);
    if (code == starbook::OK) {
        return true;
    }
    LOGF_ERROR("%s failed: %s", cmd.c_str(), response.c_str());
    return false;
}

starbook::ResponseCode Starbook::ParseCommandResponse(const std::string &response) {
    if (response == "OK")
        return starbook::OK;
    else if (response == "ERROR:FORMAT")
        return starbook::ERROR_FORMAT;
    else if (response == "ERROR:ILLEGAL STATE")
        return starbook::ERROR_ILLEGAL_STATE;
    else if (response == "ERROR:BELOW HORIZONE") /* it's not a typo */
        return starbook::ERROR_BELOW_HORIZON;

    return starbook::ERROR_UNKNOWN;
}

starbook::StarbookState Starbook::ParseState(const std::string &value) {
    if (value == "SCOPE")
        return starbook::SCOPE;
    else if (value == "GUIDE")
        return starbook::GUIDE;
    else if (value == "INIT")
        return starbook::INIT;

    LOGF_ERROR("Unknown state %s", value.c_str());
    return starbook::UNKNOWN;
}

bool Starbook::Handshake()
{
    LOG_DEBUG("Handshake");
    return Telescope::Handshake();
}
