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
//#include <connectionplugins/connectiontcp.h>
#include <cstring>
#include <regex>

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

//    we are using custom connection
    setTelescopeConnection(CONNECTION_NONE);
}

Starbook::~Starbook() = default;

bool Starbook::initProperties()
{
    Telescope::initProperties();

    IUFillText(&VersionT[0], "Version", "Version", "");
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);


    curlConnection = new Connection::Curl(this);
    curlConnection->registerHandshake([&]() { return callHandshake(); });
    registerConnection(curlConnection);

    curlConnection->setDefaultHost("192.168.0.102");
    curlConnection->setDefaultPort(80);

    cmd_interface = std::unique_ptr<starbook::CommandInterface>(
            new starbook::CommandInterface(curlConnection)
    );

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
    bool rc = Telescope::Connect();

    getFirmwareVersion();
    return rc;
}

bool Starbook::Disconnect()
{
    return Telescope::Disconnect();
}

const char* Starbook::getDefaultName()
{
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus()
{
    LOG_DEBUG("Status! Sending GETSTATUS command");
    std::string response = cmd_interface->SendCommand("GETSTATUS");
    if (response.empty()) {
        return false;
    }

    LOG_DEBUG(response.c_str());

    std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
    std::smatch sm;

    lnh_equ_posn equ_posn = { { 0, 0, 0 }, { 0, 0, 0, 0 } };
    bool goto_status = false;

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
            state = cmd_interface->ParseState(value);
            LOGF_DEBUG("Parsed STATE %i", state);
        } else if (key == "GOTO") {
            goto_status = value == "1";
        }
        response = sm.suffix();
    }

    switch (state) {
        case starbook::INIT:
        case starbook::USER:
            TrackState = SCOPE_IDLE;
            break;
        case starbook::SCOPE:
        case starbook::GUIDE:
            TrackState = goto_status ? SCOPE_SLEWING : SCOPE_TRACKING;
            break;
        case starbook::UNKNOWN:
            TrackState = SCOPE_IDLE;
            break;
    }

    ln_equ_posn d_equ_posn = {0, 0};
    ln_hequ_to_equ(&equ_posn, &d_equ_posn);
    LOGF_DEBUG("Parsed RADEC %d, %d", d_equ_posn.ra / 15, d_equ_posn.dec);
    NewRaDec(d_equ_posn.ra / 15, d_equ_posn.dec); // CONVERSION
    return true;
}

bool Starbook::Goto(double ra, double dec)
{
    starbook::ResponseCode rc;
    std::ostringstream params;
    ra = ra * 15; // CONVERSION
    params << "?" << starbook::Equ{ ra, dec };


    rc = cmd_interface->GotoRaDec(ra, dec);
    LogResponse("Goto", rc);
    return rc == starbook::OK;
}

bool Starbook::Sync(double ra, double dec)
{
    starbook::ResponseCode rc;
    std::ostringstream params;
    ra = ra * 15; // CONVERSION
    params << "?" << starbook::Equ{ ra, dec };


    rc = cmd_interface->Align(ra, dec);
    LogResponse("Sync", rc);
    return rc == starbook::OK;
}

bool Starbook::Abort()
{
    starbook::ResponseCode rc;
    rc = cmd_interface->Stop();
    LogResponse("Abort", rc);
    return rc == starbook::OK;
}

bool Starbook::Park()
{
    // TODO Park
    LOG_WARN("HOME command is unstable");
    starbook::ResponseCode rc;
    rc = cmd_interface->Home();
    LogResponse("Park", rc);
    return rc == starbook::OK;
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
    return cmd_interface->SendOkCommand("MOVE" + params.str());
}

bool Starbook::MoveWE(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
    std::ostringstream params;
    params << "?WEST="
           << ((dir == DIRECTION_WEST && command == MOTION_START) ? 1 : 0)
           << "&EAST="
           << ((dir == DIRECTION_EAST && command == MOTION_START) ? 1 : 0);

    LOGF_INFO("Move! %s", params.str().c_str());
    return cmd_interface->SendOkCommand("MOVE" + params.str());
}

bool Starbook::updateTime(ln_date *utc, double utc_offset) {
    INDI_UNUSED(utc_offset);

    std::ostringstream params;
    params << "?TIME=" << *static_cast<starbook::UTC *>(utc);

    LOGF_INFO("Time! %s", params.str().c_str());

    return cmd_interface->SendOkCommand("SETTIME" + params.str());

}

bool Starbook::getFirmwareVersion() {
    std::string response = cmd_interface->SendCommand("VERSION");
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

bool Starbook::Handshake()
{
    LOG_DEBUG("Handshake");
    return Telescope::Handshake();
}

void Starbook::LogResponse(const std::string &cmd, const starbook::ResponseCode &rc) {
    std::stringstream msg;

    msg << cmd << " [";
    switch (rc) {
        case starbook::OK:
            msg << "OK";
            break;
        case starbook::ERROR_ILLEGAL_STATE:
            msg << "ERROR_ILLEGAL_STATE";
            break;
        case starbook::ERROR_FORMAT:
            msg << "ERROR_FORMAT";
            break;
        case starbook::ERROR_BELOW_HORIZON:
            msg << "ERROR_BELOW_HORIZON";
            break;
        case starbook::ERROR_UNKNOWN:
            msg << "ERROR_UNKNOWN";
            break;
    }
    msg << "]";

    if (rc != starbook::OK) {
        msg << ": " << cmd_interface->last_cmd_url;
        LOG_ERROR(msg.str().c_str());
    } else {
        LOG_INFO(msg.str().c_str());
    }
}
