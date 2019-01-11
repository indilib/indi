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
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware", "Firmware", INFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillSwitch(&StartS[0], "Initialize", "Initialize", ISS_OFF);
    IUFillSwitchVector(&StartSP, StartS, 1, getDeviceName(), "Basic", "Basic control", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);


    curlConnection = new Connection::Curl(this);
    curlConnection->registerHandshake([&]() { return callHandshake(); });
    registerConnection(curlConnection);

    curlConnection->setDefaultHost("192.168.0.102");
    curlConnection->setDefaultPort(80);

    cmd_interface = std::unique_ptr<starbook::CommandInterface>(
            new starbook::CommandInterface(curlConnection)
    );

    addDebugControl();

    last_known_state = starbook::UNKNOWN;

    return true;
}

bool Starbook::updateProperties() {
    Telescope::updateProperties();
    if (isConnected()) {
        defineText(&VersionInfo);
        defineSwitch(&StartSP);
    } else {
        deleteProperty(VersionInfo.name);
        deleteProperty(StartSP.name);
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
    starbook::StatusResponse res;
    starbook::ResponseCode rc = cmd_interface->GetStatus(res);
    if (rc != starbook::OK) return false;

    last_known_state = res.state;
    switch (last_known_state) {
        case starbook::INIT:
        case starbook::USER:
            TrackState = SCOPE_IDLE;
            break;
        case starbook::SCOPE:
        case starbook::GUIDE:
            TrackState = res.executing_goto ? SCOPE_SLEWING : SCOPE_TRACKING;
            break;
        case starbook::UNKNOWN:
            TrackState = SCOPE_IDLE;
            break;
    }

    NewRaDec(res.equ.ra / 15, res.equ.dec); // CONVERSION
    return true;
}

bool Starbook::Goto(double ra, double dec)
{
    ra = ra * 15; // CONVERSION

    starbook::ResponseCode rc = cmd_interface->GotoRaDec(ra, dec);
    LogResponse("Goto", rc);
    return rc == starbook::OK;
}

bool Starbook::Sync(double ra, double dec)
{
    ra = ra * 15; // CONVERSION

    starbook::ResponseCode rc = cmd_interface->Align(ra, dec);
    LogResponse("Sync", rc);
    return rc == starbook::OK;
}

bool Starbook::Abort()
{
    starbook::ResponseCode rc = cmd_interface->Stop();
    LogResponse("Abort", rc);
    return rc == starbook::OK;
}

bool Starbook::Park()
{
    // TODO Park
    LOG_WARN("HOME command is unstable");
    starbook::ResponseCode rc = cmd_interface->Home();
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
    starbook::ResponseCode rc = cmd_interface->Move(dir, command);
    LogResponse("MoveNS", rc);
    return rc == starbook::OK;
}

bool Starbook::MoveWE(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
    starbook::ResponseCode rc = cmd_interface->Move(dir, command);
    LogResponse("MoveWE", rc);
    return rc == starbook::OK;
}

bool Starbook::updateTime(ln_date *utc, double utc_offset) {
    INDI_UNUSED(utc_offset);

    starbook::ResponseCode rc = cmd_interface->SetTime(*utc);
    LogResponse("updateTime", rc);
    return rc == starbook::OK;

}

bool Starbook::getFirmwareVersion() {
    starbook::VersionResponse res;
    starbook::ResponseCode rc = cmd_interface->Version(res);

    if (rc != starbook::OK) {
        if (rc == starbook::ERROR_FORMAT) {
            LOGF_ERROR("Version [ERROR]: Can't parse firmware version %s", cmd_interface->last_response.c_str());
        } else {
            LOG_ERROR("Version [ERROR]: Can't get firmware version");
        }
        return false;
    }
    if (res.major_minor < 2.7) {
        LOGF_WARN("Version [OK]: %s (< 2.7) not well supported", res.full_str.c_str());
    } else {
        LOGF_INFO("Version [OK]: %s", res.full_str.c_str());
    }
    IUSaveText(&VersionT[0], res.full_str.c_str());
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

    if (rc == starbook::ERROR_ILLEGAL_STATE) {
        msg << " (";
        switch (last_known_state) {

            case starbook::INIT:
                msg << "INIT";
                break;
            case starbook::GUIDE:
                msg << "GUIDE";
                break;
            case starbook::SCOPE:
                msg << "SCOPE";
                break;
            case starbook::USER:
                msg << "USER";
                break;
            case starbook::UNKNOWN:
                msg << "UNKNOWN";
                break;
        }
        msg << ")";
    }

    msg << "]";

    if (rc != starbook::OK) {
        msg << ": " << cmd_interface->last_cmd_url;
        LOG_ERROR(msg.str().c_str());
    } else {
        LOG_INFO(msg.str().c_str());
    }
//    LOGF_DEBUG("extracted response:", cmd_interface->last_response.c_str());
}

bool Starbook::ISNewSwitch(const char *dev, const char *name, ISState *states, char **names, int n) {

    if (!strcmp(name, StartSP.name)) {
        IUResetSwitch(&StartSP);

        if (last_known_state == starbook::INIT) {
            if (cmd_interface->Start()) {
                StartSP.s = IPS_OK;
            }
        } else {
            LOG_ERROR("Already initialized");
            StartSP.s = IPS_ALERT;
        }
        IDSetSwitch(&StartSP, nullptr);
        return true;
    }

    return Telescope::ISNewSwitch(dev, name, states, names, n);
}
