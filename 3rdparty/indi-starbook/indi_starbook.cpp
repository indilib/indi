#include <cmath>

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


static std::unique_ptr<StarbookDriver> starbook_driver(new StarbookDriver());

void log_exception(const char *dev, const char *what) {
    // NOTE: temporary solution to log driver failing and being silently restarted by server
    INDI::Logger::getInstance().print(dev, INDI::Logger::DBG_ERROR, __FILE__, __LINE__, what);
}

void ISGetProperties(const char* dev)
{
    try {
        starbook_driver->ISGetProperties(dev);
    }
    catch (std::exception &e) {
        log_exception(starbook_driver->getDeviceName(), e.what());
        throw e;
    }
}

void ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    try {
        starbook_driver->ISNewSwitch(dev, name, states, names, n);
    }
    catch (std::exception &e) {
        log_exception(starbook_driver->getDeviceName(), e.what());
        throw e;
    }
}

void ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n)
{
    try {
        starbook_driver->ISNewText(dev, name, texts, names, n);
    }
    catch (std::exception &e) {
        log_exception(starbook_driver->getDeviceName(), e.what());
        throw e;
    }
}

void ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n)
{
    try {
        starbook_driver->ISNewNumber(dev, name, values, names, n);
    }
    catch (std::exception &e) {
        log_exception(starbook_driver->getDeviceName(), e.what());
        throw e;
    }
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

StarbookDriver::StarbookDriver()
{
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MINOR);
    SetTelescopeCapability(
            TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME,
            starbook::MAX_SPEED);

//    we are using custom Connection::Curl
    setTelescopeConnection(CONNECTION_NONE);
}

StarbookDriver::~StarbookDriver() = default;

bool StarbookDriver::initProperties()
{
    Telescope::initProperties();

    IUFillText(&VersionT[0], "Version", "Version", "");
    IUFillTextVector(&VersionTP, VersionT, 1, getDeviceName(), "Firmware", "Firmware", INFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillText(&StateT[0], "State", "State", "");
    IUFillTextVector(&StateTP, StateT, 1, getDeviceName(), "Status", "Status", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillSwitch(&StartS[0], "Initialize", "Initialize", ISS_OFF);
    IUFillSwitchVector(&StartSP, StartS, 1, getDeviceName(), "Basic", "Basic control", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);


    curlConnection = new Connection::Curl(this);
    curlConnection->registerHandshake([&]() { return callHandshake(); });
    registerConnection(curlConnection);

    curlConnection->setDefaultHost(DEFAULT_STARBOOK_ADDRESS);
    curlConnection->setDefaultPort(DEFAULT_STARBOOK_PORT);

    cmd_interface = std::unique_ptr<starbook::CommandInterface>(
            new starbook::CommandInterface(curlConnection)
    );

    addDebugControl();

    last_known_state = starbook::UNKNOWN;

    return true;
}

bool StarbookDriver::updateProperties() {
    Telescope::updateProperties();
    if (isConnected()) {
        defineText(&VersionTP);
        defineText(&StateTP);
        defineSwitch(&StartSP);
    } else {
        deleteProperty(VersionTP.name);
        deleteProperty(StateTP.name);
        deleteProperty(StartSP.name);
    }
    return true;
}

bool StarbookDriver::Connect()
{
    failed_res = 0;
    bool rc = Telescope::Connect();
    if (rc) {
        getFirmwareVersion();
        // TODO: resolve this in less hacky way https://github.com/indilib/indi/issues/810
        saveConfig(false, "DEVICE_ADDRESS");
    } else {
        LOG_ERROR("Connection failed");
    }
    return rc;
}

bool StarbookDriver::Disconnect()
{
    if (isConnected()) {
        bool rc = Telescope::Disconnect();
        // Disconnection is successful, set it IDLE and updateProperties.
        if (rc) {
            setConnected(false, IPS_IDLE);
            updateProperties();
        } else
            setConnected(true, IPS_ALERT);
        return rc;
    } else {
        return false;
    }
}

const char *StarbookDriver::getDefaultName()
{
    return "Starbook mount controller";
}

bool StarbookDriver::ReadScopeStatus()
{
    LOG_DEBUG("Status! Sending GETSTATUS command");
    starbook::StatusResponse res;
    starbook::ResponseCode rc;
    try {
        rc = cmd_interface->GetStatus(res);
    } catch (std::exception &e) {
        StateTP.s = IPS_ALERT;
        failed_res++;
        LOG_ERROR(e.what());

        if (failed_res > 3) {
            LOG_ERROR("Failed to keep connection, disconnecting");
            StarbookDriver::Disconnect();
            failed_res = 0;
        }
        return false;
    }

    last_known_state = res.state;
    switch (last_known_state) {
        case starbook::INIT:
        case starbook::USER:
            TrackState = SCOPE_IDLE;
            break;
        case starbook::SCOPE:
        case starbook::GUIDE:
        case starbook::CHART:
        case starbook::ALTAZ:
            TrackState = res.executing_goto ? SCOPE_SLEWING : SCOPE_TRACKING;
            break;
        case starbook::UNKNOWN:
            TrackState = SCOPE_IDLE;
            break;
    }

    switch (last_known_state) {
        case starbook::INIT:
            IUSaveText(&StateT[0], "INIT");
            break;
        case starbook::GUIDE:
            IUSaveText(&StateT[0], "GUIDE");
            break;
        case starbook::SCOPE:
            IUSaveText(&StateT[0], "SCOPE");
            break;
        case starbook::USER:
            IUSaveText(&StateT[0], "USER");
            break;
        case starbook::UNKNOWN:
            IUSaveText(&StateT[0], "UNKNOWN");
            break;
        case starbook::ALTAZ:
            IUSaveText(&StateT[0], "ALTAZ");
            break;
        case starbook::CHART:
            IUSaveText(&StateT[0], "CHART");
            break;
    }
    failed_res = 0;
    StateTP.s = IPS_OK;
    IDSetText(&StateTP, nullptr);

    NewRaDec(res.equ.ra / 15, res.equ.dec); // CONVERSION

    LOG_DEBUG("STATUS");
//    LOGF_DEBUG("REQ: %s RES: %s", cmd_interface->last_cmd_url.c_str(), cmd_interface->last_response.c_str());
    return true;
}

bool StarbookDriver::Goto(double ra, double dec)
{
    ra = ra * 15; // CONVERSION

    starbook::ResponseCode rc = cmd_interface->GotoRaDec(ra, dec);
    LogResponse("Goto", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::Sync(double ra, double dec)
{
    ra = ra * 15; // CONVERSION

    starbook::ResponseCode rc = cmd_interface->Align(ra, dec);
    LogResponse("Sync", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::Abort()
{
    starbook::ResponseCode rc = cmd_interface->Stop();
    LogResponse("Abort", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::Park()
{
    // TODO Park
    LOG_WARN("HOME command is unstable");
    starbook::ResponseCode rc = cmd_interface->Home();
    LogResponse("Park", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::UnPark()
{
    // TODO UnPark
    LOG_WARN("Always unparked");
    return true;
}

bool StarbookDriver::MoveNS(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command) {
    starbook::ResponseCode rc = cmd_interface->Move(dir, command);
    LogResponse("MoveNS", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::MoveWE(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
    starbook::ResponseCode rc = cmd_interface->Move(dir, command);
    LogResponse("MoveWE", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::SetSlewRate(int index) {
    ++index; // MIN_SPEED is 1, index starts at 0
    starbook::ResponseCode rc = cmd_interface->SetSpeed(index);
    LogResponse("SetSlewRate", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::updateTime(ln_date *utc, double utc_offset) {
    INDI_UNUSED(utc_offset);
    if (last_known_state != starbook::INIT) {
        LogResponse("updateTime", starbook::ERROR_ILLEGAL_STATE);
        return false;
    }
    starbook::ResponseCode rc;
    utc->hours += floor(utc_offset); // starbook stores local time, go figure
//    utc->minutes += floor((utc_offset - floor(utc_offset)) * 60);
    rc = cmd_interface->SetTime(*utc);
    LogResponse("updateTime", rc);
    return rc == starbook::OK;

}

bool StarbookDriver::updateLocation(double latitude, double longitude, double elevation) {
    INDI_UNUSED(elevation);
    if (last_known_state != starbook::INIT) {
        LogResponse("updateLocation", starbook::ERROR_ILLEGAL_STATE);
        return false;
    }
    int utc_offset = static_cast<int>(std::floor(std::strtof(TimeT[1].text, nullptr)));
    starbook::LnLat posn(latitude, longitude);
    starbook::ResponseCode rc = cmd_interface->SetPlace(posn, utc_offset);
    LogResponse("updateLocation", rc);
    return rc == starbook::OK;
}

bool StarbookDriver::getFirmwareVersion() {
    starbook::VersionResponse res;
    starbook::ResponseCode rc = cmd_interface->Version(res);

    if (rc != starbook::OK) {
        if (rc == starbook::ERROR_FORMAT) {
            LOGF_ERROR("Version [ERROR]: Can't parse firmware version %s", cmd_interface->getLastResponse().c_str());
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
    IDSetText(&VersionTP, nullptr);

    return true;
}

bool StarbookDriver::Handshake()
{
    LOG_DEBUG("Handshake");
    return Telescope::Handshake();
}

void StarbookDriver::LogResponse(const std::string &cmd, const starbook::ResponseCode &rc) {
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
        case starbook::ERROR_POINT:
            msg << "ERROR_POINT";
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
            case starbook::ALTAZ:
                msg << "ALTAZ";
                break;
            case starbook::CHART:
                msg << "CHART";
                break;
        }
        msg << ")";
    }

    msg << "]";

    if (rc != starbook::OK) {
        msg << ": \"" << cmd_interface->getLastResponse() << "\"";
        LOG_ERROR(msg.str().c_str());
    } else {
        LOG_INFO(msg.str().c_str());
    }
}

bool StarbookDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char **names, int n) {

    if (!strcmp(name, StartSP.name)) {
        return performStart();
    }

    if (!strcmp(name, "CONNECTION_MODE")) {
        // TODO: resolve this in less hacky way https://github.com/indilib/indi/issues/810
        loadConfig(false, "DEVICE_ADDRESS");
        // we pass rest of the work to parent function
        // hopefully, loading property before connection mode setups won't break anything
    }

    return Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool StarbookDriver::performStart() {
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

void StarbookDriver::TimerHit() {
    try {
        Telescope::TimerHit();
    }
    catch (std::exception &e) {
        log_exception(getDeviceName(), e.what());
        throw e;
    }

}
