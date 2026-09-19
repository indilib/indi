/*******************************************************************************
  Copyright(c) 2026 Philippe Bazart, Jérémie Klein. All rights reserved.
    With partial reuse of code by Jasem Mutlaq, originally written for the
    Alpaca CCD INDI driver.

  ASCOM Alpaca Dome INDI Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "alpaca_dome.h"

#include <httplib.h>
#include <string.h>
#include <chrono>
#include <thread>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

// We declare an auto pointer to AlpacaDome
std::unique_ptr<AlpacaDome> alpaca_dome(new AlpacaDome());

AlpacaDome::AlpacaDome()
{
    setVersion(2, 0);
    setDomeConnection(INDI::Dome::CONNECTION_NONE);
    SetDomeCapability(INDI::Dome::DOME_CAN_ABORT);
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::TimerHit()
{
    if (!isConnected())
        return;

    updateStatus();
    SetTimer(getCurrentPollingPeriod());
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    // Always define these properties
    defineProperty(ServerAddressTP);
    defineProperty(DeviceNumberNP);
    defineProperty(ConnectionSettingsNP);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && isDeviceNameMatch(dev))
    {
        if (OperationSP.isNameMatch(name))
        {
            const char *requestedOperation = IUFindOnSwitchName(states, names, n);
            if (OperationSP[OPERATION_FIND_HOME].isNameMatch(requestedOperation))
            {
                IPState rc = FindHome();
                if (rc != IPS_ALERT)
                {
                    OperationSP.reset();
                    OperationSP[OPERATION_FIND_HOME].s = ISS_ON;
                    setDomeState(DOME_MOVING);
                }
                OperationSP.setState(rc);
            }
            else if (OperationSP[OPERATION_CALIBRATE].isNameMatch(requestedOperation))
            {
                IPState rc = Calibrate();
                if (rc != IPS_ALERT)
                {
                    OperationSP.reset();
                    OperationSP[OPERATION_CALIBRATE].s = ISS_ON;
                    setDomeState(DOME_MOVING);
                }
                OperationSP.setState(rc);
            }

            OperationSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && isDeviceNameMatch(dev))
    {
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            // Save configuration after update
            saveConfig();
            LOG_INFO("Connection settings updated.");
            return true;
        }
        else if (DeviceNumberNP.isNameMatch(name))
        {
            DeviceNumberNP.update(values, names, n);
            DeviceNumberNP.setState(IPS_OK);
            DeviceNumberNP.apply();
            // Save configuration after update
            saveConfig();
            LOG_INFO("Alpaca device number updated.");
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && isDeviceNameMatch(dev) && ServerAddressTP.isNameMatch(name))
    {
        ServerAddressTP.update(texts, names, n);
        ServerAddressTP.setState(IPS_OK);
        ServerAddressTP.apply();
        // Save configuration after update
        saveConfig();
        return true;
    }

    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

///////////////////////////////////////////////////////////////////////////////

const char *AlpacaDome::getDefaultName()
{
    return "Alpaca Dome";
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::initProperties()
{
    INDI::Dome::initProperties();

    // Try to load server address from saved config (mirrors TCP plugin pattern).
    // m_ConfigHost/m_ConfigPort are empty if no config was previously saved.
    char configHost[MAXINDINAME] = {0};
    char configPort[MAXINDINAME] = {0};
    if (IUGetConfigText(getDeviceName(), "SERVER_ADDRESS", "HOST", configHost, MAXINDINAME) == 0)
        m_ConfigHost = configHost;
    if (IUGetConfigText(getDeviceName(), "SERVER_ADDRESS", "PORT", configPort, MAXINDINAME) == 0)
        m_ConfigPort = configPort;

    // Setup server address properties
    ServerAddressTP[HOST].fill("HOST", "Host", m_ConfigHost.c_str());
    ServerAddressTP[PORT].fill("PORT", "Port", m_ConfigPort.c_str());
    ServerAddressTP.fill(getDeviceName(), "SERVER_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    ServerAddressTP.load();

    // Setup device number property
    DeviceNumberNP[0].fill("DEVICE_NUMBER", "Device Number", "%.0f", 0, 10, 1, 0);
    DeviceNumberNP.fill(getDeviceName(), "DEVICE_NUMBER", "Alpaca Device", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    DeviceNumberNP.load();

    // Setup connection settings properties
    ConnectionSettingsNP[TIMEOUT].fill("TIMEOUT", "Connection timeout (ms)", "%.0f", 1, 10000, 100, 5000);
    ConnectionSettingsNP[RW_TIMEOUT].fill("RW_TIMEOUT", "Read/Write timeout (ms)", "%.0f", 1, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    ConnectionSettingsNP.load();

    // Device and driver Info property
    InfoTP[INFO_DEVICE_NAME].fill("INFO_DEVICE_NAME", "Name", "");
    InfoTP[INFO_DESCRIPTION].fill("INFO_DESCRIPTION", "Description", "");
    InfoTP[INFO_DRIVER_INFO].fill("INFO_DRIVER_INFO", "Driver", "");
    InfoTP[INFO_DRIVER_VERSION].fill("INFO_DRIVER_VERSION", "Driver Version", "");
    InfoTP[INFO_INTERFACE_VERSION].fill("INFO_INTERFACE_VERSION", "ASCOM Dome Interface Version", "");
    InfoTP.fill(getDeviceName(), "DEVICE_INFO", "Alpaca Device Info", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Configuring operations about positionning and home definition
    OperationSP[OPERATION_FIND_HOME].fill("OPERATION_FIND_HOME", "Find Home", ISS_OFF);
    OperationSP[OPERATION_CALIBRATE].fill("OPERATION_CALIBRATE", "Calibrate", ISS_OFF);
    OperationSP.fill(getDeviceName(), "OPERATION", "Operation", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    SetParkDataType(PARK_NONE);
    addAuxControls();

    // Default server address.
    setDefaultServerAddress("alpaca.local", "7843");

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        // State properties
        if (CanFindHome() || CanCalibrate())
        {
            defineProperty(OperationSP);
        }

        // Device Info properties
        defineProperty(InfoTP);
        InfoTP[INFO_DEVICE_NAME].setText(m_DeviceName.c_str());
        InfoTP[INFO_DESCRIPTION].setText(m_Description.c_str());
        InfoTP[INFO_DRIVER_INFO].setText(m_DriverInfo.c_str());
        InfoTP[INFO_DRIVER_VERSION].setText(m_DriverVersion.c_str());
        InfoTP[INFO_INTERFACE_VERSION].setText(std::to_string(m_InterfaceVersion).c_str());
        InfoTP.setState(IPS_OK);
        InfoTP.apply();

        LOG_INFO("Alpaca dome is ready for operation.");
    }
    else
    {
        deleteProperty(OperationSP);
        deleteProperty(InfoTP);

        LOG_INFO("Alpaca dome is disconnected.");
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    ServerAddressTP.save(fp);
    DeviceNumberNP.save(fp);
    ConnectionSettingsNP.save(fp);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::Connect()
{
    if (ServerAddressTP[HOST].isEmpty() || ServerAddressTP[PORT].isEmpty())
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Initialize HTTP client
    int connectionTimeout = static_cast<int>(ConnectionSettingsNP[TIMEOUT].getValue());
    int timeout = static_cast<int>(ConnectionSettingsNP[RW_TIMEOUT].getValue());
    httpClient = std::make_unique<httplib::Client>(ServerAddressTP[HOST].getText(), std::stoi(ServerAddressTP[PORT].getText()));
    httpClient->set_connection_timeout(std::chrono::milliseconds(connectionTimeout));
    httpClient->set_read_timeout(std::chrono::milliseconds(timeout));
    httpClient->set_write_timeout(std::chrono::milliseconds(timeout));

    // Test connection by getting camera status
    bool connected = false;
    if (!alpacaGetBool("/connected", connected))
    {
        LOG_ERROR("Failed to connect to Alpaca dome. Please check server address and port.");
        return false;
    }

    // Connect to the device if not already connected
    if (!connected)
    {
        if (!alpacaConnect())
            return false;
    }

    LOG_INFO("Successfully connected to Alpaca dome.");

    // Get device info
    alpacaGetString("/name", m_DeviceName);
    alpacaGetString("/description", m_Description);
    alpacaGetString("/driverinfo", m_DriverInfo);
    alpacaGetString("/driverversion", m_DriverVersion);
    alpacaGetInt("/interfaceversion", m_InterfaceVersion);

    // Update dome capabilities
    alpacaGetBool("/canfindhome", m_CanFindHome);
    alpacaGetBool("/canpark", m_CanPark);
    alpacaGetBool("/cansetaltitude", m_CanSetAltitude);
    alpacaGetBool("/cansetazimuth", m_CanSetAzimuth);
    alpacaGetBool("/cansetpark", m_CanSetPark);
    alpacaGetBool("/cansetshutter", m_CanSetShutter);
    alpacaGetBool("/canslave", m_CanSlave);
    alpacaGetBool("/cansyncazimuth", m_CanSyncAzimuth);
    uint32_t cap = INDI::Dome::DOME_CAN_ABORT;
    if (m_CanPark)
        cap |= INDI::Dome::DOME_CAN_PARK;
    if (m_CanSetShutter)
        cap |= INDI::Dome::DOME_HAS_SHUTTER;
    if (m_CanSetAzimuth)
        cap |= INDI::Dome::DOME_CAN_ABS_MOVE | INDI::Dome::DOME_CAN_REL_MOVE;
    if (m_CanSyncAzimuth)
        cap |= INDI::Dome::DOME_CAN_SYNC;
    SetDomeCapability(cap);

    // Setting parked flag once on connection
    if (m_CanPark)
    {
        bool slewing, parked;
        if (alpacaGetBool("/slewing", slewing, false) && alpacaGetBool("/atpark", parked, true))
            SetParked(!slewing && parked);
    }

    SetTimer(getCurrentPollingPeriod());
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::Disconnect()
{
    if (isConnected())
    {
        if (!alpacaDisconnect())
        {
            LOG_ERROR("Failed to set disconnected state on Alpaca dome.");
            return false;
        }
    }

    // Reset HTTP client object and close any existing connections
    httpClient.reset();

    LOG_INFO("Disconnected from Alpaca dome.");
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::SetSpeed(double rpm)
{
    INDI_UNUSED(rpm);
    return false;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::MoveRel(double azDiff)
{
    if (CanSetAzimuth() == false)
    {
        LOG_ERROR( "Dome does not support azimuth slewing.");
        return IPS_ALERT;
    }

    double targetAz = range360(DomeAbsPosNP[0].getValue() + azDiff);
    return MoveAbs(targetAz);
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::MoveAbs(double az)
{
    if (CanSetAzimuth() == false)
    {
        LOG_ERROR( "Dome does not support azimuth slewing.");
        return IPS_ALERT;
    }

    if (!alpacaDomeSlewToAzimuth(range360(az)))
        return IPS_ALERT;
    return IPS_BUSY;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (CanSetAzimuth() == false)
    {
        LOG_ERROR( "Dome does not support azimuth slewing.");
        return IPS_ALERT;
    }

    double targetAz = DomeAbsPosNP[0].getValue();

    if (operation == MOTION_START)
    {
        targetAz = DomeAbsPosNP[0].getValue();
        if (dir == DOME_CW)
            targetAz += 5;
        else
            targetAz -= 5;
        targetAz = range360(targetAz);
    }

    if (MoveAbs(targetAz) == IPS_ALERT)
        return IPS_ALERT;

    return ((operation == MOTION_START) ? IPS_BUSY : IPS_OK);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::Sync(double az)
{
    if (CanSync() == false)
    {
        LOG_ERROR( "Dome does not support syncing.");
        return false;
    }

    double targetAz = range360(az);
    bool success = alpacaDomeSyncToAzimuth(targetAz);
    if (success)
    {
        DomeSyncNP.setState(IPS_OK);
        LOGF_INFO( "Dome successfully synchronized to azimuth angle %.2f.", targetAz);
    }
    else
    {
        DomeSyncNP.setState(IPS_ALERT);
        LOGF_ERROR( "Error: cannot synchronize dome to azimuth angle %.2f.", targetAz);
    }

    DomeSyncNP.apply();
    return success;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::Abort()
{
    if (alpacaDomeAbortSlew())
    {
        if (OperationSP.getState() == IPS_BUSY)
        {
            if (OperationSP[OPERATION_FIND_HOME].getState() == ISS_ON)
            {
                LOG_WARN("Finding home operation is aborted.");
            }
            else if (OperationSP[OPERATION_CALIBRATE].getState() == ISS_ON)
            {
                LOG_WARN("Calibrating operation is aborted.");
            }
            OperationSP.reset();
            OperationSP.setState(IPS_ALERT);
            OperationSP.apply();
        }
        else if (getShutterState() == SHUTTER_MOVING)
        {
            LOG_WARN("Shutter motion aborted!");
            DomeShutterSP.setState(IPS_ALERT);
            DomeShutterSP.apply();
        }
        else
        {
            LOG_WARN("Dome motion aborted.");
        }

        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::Park()
{
    if (HasShutter() && ShutterParkPolicySP[SHUTTER_CLOSE_ON_PARK].getState() == ISS_ON)
    {
        if (ControlShutter(SHUTTER_CLOSE) == IPS_ALERT)
            return IPS_ALERT;
        LOG_INFO("Close shutter on park");
    }

    if (alpacaDomePark())
    {
        LOG_INFO("Parking dome...");
        setDomeState(DOME_PARKING);
        return IPS_BUSY;
    }

    LOG_ERROR("Failed to park dome");
    return IPS_ALERT;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::UnPark()
{
    if (HasShutter() && ShutterParkPolicySP[SHUTTER_OPEN_ON_UNPARK].getState() == ISS_ON)
    {
        if (ControlShutter(SHUTTER_OPEN) == IPS_ALERT)
            return IPS_ALERT;
        LOG_INFO("Open shutter on unpark");
    }

    LOG_INFO("Unparking dome...");
    IPState result = MoveAbs(DomeAbsPosNP[0].getValue());
    if (result != IPS_ALERT)
        setDomeState(DOME_UNPARKING);

    return result;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::ControlShutter(ShutterOperation operation)
{
    if (operation == SHUTTER_OPEN)
    {
        LOG_INFO("Opening dome...");
        if (alpacaDomeOpenShutter())
        {
            LOG_INFO("Dome is opening...");
            setDomeState(DOME_UNPARKING);
            return IPS_BUSY;
        }

        return IPS_ALERT;
    }
    else if (operation == SHUTTER_CLOSE)
    {
        if (isLocked())
        {
            LOG_WARN("Cannot close dome when mount is locking. See: Telescope parking policy, in options tab");
            return IPS_ALERT;
        }

        LOG_INFO("Closing dome...");
        if (alpacaDomeCloseShutter())
        {
            LOG_INFO("Dome is closing...");
            setDomeState(DOME_PARKING);
            return IPS_BUSY;
        }

        return IPS_ALERT;
    }

    return IPS_OK;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::SetCurrentPark()
{
    if (CanSetPark() == false)
    {
        LOG_ERROR( "Dome does not support setting park position.");
        return false;
    }

    if (alpacaDomeSetPark())
    {
        LOG_INFO( "Dome park position successfully set to current azimuth");
        return true;
    }

    LOG_ERROR( "Error: cannot set park position to current azimuth");
    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::SetDefaultPark()
{
    return SetCurrentPark();
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::FindHome()
{
    if (CanFindHome() == false)
    {
        LOG_ERROR( "Dome does not support homing.");
        return IPS_ALERT;
    }

    if (alpacaDomeFindHome())
    {
        LOG_INFO( "Dome is moving to home position...");
        setDomeState(DOME_MOVING);
        return IPS_BUSY;
    }

    LOG_ERROR( "Error: cannot move the dome to the home position");
    return IPS_ALERT;
}

///////////////////////////////////////////////////////////////////////////////

IPState AlpacaDome::Calibrate()
{
    LOG_ERROR( "Calibration is not supported by standard basic Alpaca dome.");
    return IPS_ALERT;
}

///////////////////////////////////////////////////////////////////////////////

std::string AlpacaDome::getAlpacaURL(const std::string& endpoint)
{
    return "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + endpoint;
}

///////////////////////////////////////////////////////////////////////////////

std::string AlpacaDome::buildAlpacaFormData(const nlohmann::json &request)
{
    // Convert JSON to form data for Alpaca compatibility
    std::string form_data;
    for (auto & [key, value] : request.items())
    {
        if (!form_data.empty()) form_data += "&";

        if (value.is_string())
        {
            form_data += key + "=" + value.get<std::string>();
        }
        else if (value.is_number_integer())
        {
            form_data += key + "=" + std::to_string(value.get<int>());
        }
        else if (value.is_number_float())
        {
            form_data += key + "=" + std::to_string(value.get<double>());
        }
        else if (value.is_boolean())
        {
            form_data += key + "=" + (value.get<bool>() ? "true" : "false");
        }
    }

    // Add ClientID and ClientTransactionID to form data
    if (!form_data.empty()) form_data += "&";
    form_data += "ClientID=" + std::to_string(getpid());
    form_data += "&ClientTransactionID=" + std::to_string(getTransactionId());

    return form_data;
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::setDefaultServerAddress(const char *host, const char *port, bool force)
{
    // Only apply defaults if no value was loaded from saved config (mirrors TCP plugin pattern).
    // m_ConfigHost/m_ConfigPort are empty when no config file entry exists for this device,
    // meaning this is either a first run or the user has never saved settings.
    if (host != nullptr)
    {
        if (force || m_ConfigHost.empty())
            ServerAddressTP[HOST].setText(host);
    }

    if (port != nullptr)
    {
        if (force || m_ConfigPort.empty())
            ServerAddressTP[PORT].setText(port);
    }
}

///////////////////////////////////////////////////////////////////////////////

uint32_t AlpacaDome::getTransactionId()
{
    return ++m_ClientTransactionID;
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::updateStatus()
{
    if (!isConnected())
    {
        setShutterState(SHUTTER_UNKNOWN);
        setDomeState(DOME_UNKNOWN);
        return;
    }

    // Request dome current state.
    AlapacaDomeState value;
    if (alpacaGetDeviceState(value))
    {
        // Shutter state update
        if (HasShutter())
            updateShutterStatus(value);

        // Dome state update
        updateDomeStatus(value);
    }
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::updateDomeStatus(AlapacaDomeState &state)
{
    if (getDomeState() == DOME_MOVING && !state.slewing)
    {
        bool isHoming = (OperationSP.getState() == IPS_BUSY && OperationSP[OPERATION_FIND_HOME].getState() == ISS_ON);
        bool isCalibrating = (OperationSP.getState() == IPS_BUSY && OperationSP[OPERATION_CALIBRATE].getState() == ISS_ON);

        if (isHoming && state.atHome)
        {
            LOG_INFO("Dome has found home position.");
            OperationSP.reset();
            OperationSP.setState(IPS_OK);
            OperationSP.apply();
            setDomeState(DOME_SYNCED);
        }
        else if (isCalibrating)
        {
            LOG_INFO("Dome calibration complete.");
            OperationSP.reset();
            OperationSP.setState(IPS_OK);
            OperationSP.apply();
            setDomeState(DOME_SYNCED);
        }
        else
        {
            LOGF_INFO("Dome reached requested azimuth: %.3f Degrees", state.azimuth);
            setDomeState(DOME_SYNCED);
        }
    }
    else if (getDomeState() == DOME_PARKING && !state.slewing && state.atPark)
    {
        SetParked(true);
    }
    else if (getDomeState() == DOME_UNPARKING && !state.slewing)
    {
        SetParked(false);
    }
    else if (getDomeState() == DOME_MOVING && state.slewing && DomeAbsPosNP.getState() != IPS_BUSY)
    {
        DomeAbsPosNP.setState(IPS_BUSY);
    }
    DomeAbsPosNP[0].setValue(state.azimuth);
    DomeAbsPosNP.apply();
}

///////////////////////////////////////////////////////////////////////////////

void AlpacaDome::updateShutterStatus(AlapacaDomeState &state)
{
    // Alpaca shutter states
    // 0 = Open
    // 1 = Closed
    // 2 = Opening
    // 3 = Closing
    // 4 = Error
    ShutterState previousState = getShutterState();
    switch(state.shutterStatus)
    {
        case 0: // Open
            if (previousState != SHUTTER_OPENED)
            {
                LOG_INFO("Shutter is fully open.");
                setShutterState(SHUTTER_OPENED);
            }
            break;

        case 1: // Closed
            if (previousState != SHUTTER_CLOSED)
            {
                LOG_INFO("Shutter is fully closed.");
                setShutterState(SHUTTER_CLOSED);
            }
            break;

        case 2: // Opening
            if (previousState != SHUTTER_MOVING)
            {
                LOG_INFO("Shutter is opening...");
                setShutterState(SHUTTER_MOVING);
            }
            break;

        case 3: // Closing
            if (previousState != SHUTTER_MOVING)
            {
                LOG_INFO("Shutter is closing...");
                setShutterState(SHUTTER_MOVING);
            }
            break;

        case 4: // Error
            if (previousState != SHUTTER_ERROR)
            {
                LOG_ERROR("Shutter is in error state.");
                setShutterState(SHUTTER_ERROR);
            }
            break;

        default:
            if (previousState != SHUTTER_UNKNOWN)
                setShutterState(SHUTTER_UNKNOWN);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::sendAlpacaGET(const std::string &endpoint, nlohmann::json &response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized.");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);
    url += "?ClientID=" + std::to_string(getpid()) + "&ClientTransactionID=" + std::to_string(getTransactionId());

    auto result = httpClient->Get(url.c_str());

    if (!result)
    {
        LOGF_ERROR("HTTP GET failed for %s: %s", endpoint.c_str(),
                   httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("HTTP GET %s returned status %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        // Check for Alpaca errors
        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error in %s: %d - %s", endpoint.c_str(),
                       response["ErrorNumber"].get<int>(),
                       response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }

        return true;
    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error for %s: %s", endpoint.c_str(), e.what());
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::sendAlpacaPUT(const std::string &endpoint, const nlohmann::json &request, nlohmann::json &response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized.");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);
    std::string form_data = buildAlpacaFormData(request);

    auto result = httpClient->Put(url, form_data, "application/x-www-form-urlencoded");

    if (!result)
    {
        LOGF_ERROR("HTTP PUT failed for %s: %s", endpoint.c_str(),
                   httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("HTTP PUT %s returned status %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        // Check for Alpaca errors
        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {

            // JM 2025.08.29: For some reason, I always get error 1025 when setting temperature
            // even though value is in range. Ignore this error for now.
            // if (response["ErrorNumber"].get<int>() == 1025)
            //     return true;

            LOGF_ERROR("Alpaca error in %s: %d - %s", endpoint.c_str(),
                       response["ErrorNumber"].get<int>(),
                       response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }

        return true;

    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error for %s: %s", endpoint.c_str(), e.what());
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaConnect()
{
    nlohmann::json response;

    // IDome Version 3 and later
    if (m_InterfaceVersion >= 3)
    {
        if (!sendAlpacaPUT("/connect", {}, response))
            return false;

        bool connecting = true;
        int retryDelay = 100;

        // Need waiting for connection to be fully established (by checking /connecting)
        while (connecting && retryDelay < 4000)
        {
            if (!alpacaGetBool("/connecting", connecting))
                return false;
            if (connecting)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                retryDelay *= 2;
            }
        }
    }
    else
    {
        if (!sendAlpacaPUT("/connected", {{"Connected", true}}, response))
            return false;

        bool connected = false;
        int retryDelay = 100;

        // Need waiting for connection to be fully established (by checking /connecting)
        while (!connected && retryDelay < 4000)
        {
            if (!alpacaGetBool("/connected", connected))
                return false;
            if (!connected)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                retryDelay *= 2;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDisconnect()
{
    nlohmann::json response;

    // IDome Version 3 and later
    if (m_InterfaceVersion >= 3)
    {
        if (!sendAlpacaPUT("/disconnect", {}, response))
            return false;
    }
    else
    {
        if (!sendAlpacaPUT("/connected", {{"Connected", false}}, response))
            return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaGetBool(const std::string &endpoint, bool& value, bool defaultValue)
{
    nlohmann::json response;
    if (!sendAlpacaGET(endpoint, response))
        return false;
    if (response.contains("Value") && response["Value"].is_boolean())
        value = response["Value"].get<bool>();
    else
        value = defaultValue;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaGetInt(const std::string &endpoint, int &value, int defaultValue)
{
    nlohmann::json response;
    if (!sendAlpacaGET(endpoint, response))
        return false;
    if (response.contains("Value") && response["Value"].is_number_integer())
        value = response["Value"].get<int>();
    else
        value = defaultValue;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaGetDouble(const std::string &endpoint, double &value, double defaultValue)
{
    nlohmann::json response;
    if (!sendAlpacaGET(endpoint, response))
        return false;
    if (response.contains("Value") && response["Value"].is_number_float())
        value = response["Value"].get<double>();
    else
        value = defaultValue;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaGetString(const std::string &endpoint, std::string &value, const std::string& defaultValue)
{
    nlohmann::json response;
    if (!sendAlpacaGET(endpoint, response))
        return false;
    if (response.contains("Value") && response["Value"].is_string())
        value = response["Value"].get<std::string>();
    else
        value = defaultValue;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaGetDeviceState(AlapacaDomeState &value, double defaultAz, double defaultAlt)
{
    value.atPark = value.atHome = value.slewing = false;
    value.azimuth = defaultAz;
    value.altitude = defaultAlt;
    value.shutterStatus = -1;

    // IDome Version 3 and later
    if (m_InterfaceVersion >= 3)
    {
        nlohmann::json response;
 
        if (!sendAlpacaGET("/devicestate", response))
            return false;

        if (!response.contains("Value") || !response["Value"].is_array())
        {
            LOG_ERROR("Error: an array of objects is expected in Value attribute of response");
            return false;
        }

        for (auto &item : response["Value"])
        {
            try
            {
                const std::string &key = item["Name"].get<std::string>();
                if (key == "Altitude")
                    value.altitude = item["Value"].get<double>();
                else if (key == "AtHome")
                    value.atHome = item["Value"].get<bool>();
                else if (key == "AtPark")
                    value.atPark = item["Value"].get<bool>();
                else if (key == "Azimuth")
                    value.azimuth = item["Value"].get<double>();
                else if (key == "ShutterStatus")
                    value.shutterStatus = item["Value"].get<int>();
                else if (key == "Slewing")
                    value.slewing = item["Value"].get<bool>();
            }
            catch (const std::bad_any_cast& e)
            {
                LOGF_ERROR("Error: unexpected data type in Value array returned by devicestate API operation (%s)", e.what());
            }
            catch (const std::out_of_range& e)
            {
                LOGF_ERROR("Error: missing element in Value array returned by devicestate API operation (%s)", e.what());
            }
        }
    }
    else
    {
        if (CanSetAzimuth() || CanAbsMove() || CanRelMove())
            alpacaGetBool("/slewing", value.slewing, false);
        if (CanFindHome())
            alpacaGetBool("/athome", value.atHome, false);
        if (CanSetPark())
            alpacaGetBool("/atpark", value.atPark, false);
        if (CanSetAzimuth())
            alpacaGetDouble("/azimuth", value.azimuth, defaultAz);
        if (CanSetAltitude())
            alpacaGetDouble("/altitude", value.altitude, defaultAlt);
        if (HasShutter())
            alpacaGetInt("/shutterstatus", value.shutterStatus, -1);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeAbortSlew()
{
    nlohmann::json response;
    return sendAlpacaPUT("/abortslew", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeCloseShutter()
{
    nlohmann::json response;
    return sendAlpacaPUT("/closeshutter", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeFindHome()
{
    nlohmann::json response;
    return sendAlpacaPUT("/findhome", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeOpenShutter()
{
    nlohmann::json response;
    return sendAlpacaPUT("/openshutter", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomePark()
{
    nlohmann::json response;
    return sendAlpacaPUT("/park", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeSetPark()
{
    nlohmann::json response;
    return sendAlpacaPUT("/setpark", {}, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeSlewToAltitude(double alt)
{
    nlohmann::json response;
    nlohmann::json request = {{"Altitude", alt}};
    return sendAlpacaPUT("/slewtoaltitude", request, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeSlewToAzimuth(double az)
{
    nlohmann::json response;
    nlohmann::json request = {{"Azimuth", az}};
    return sendAlpacaPUT("/slewtoazimuth", request, response);
}

///////////////////////////////////////////////////////////////////////////////

bool AlpacaDome::alpacaDomeSyncToAzimuth(double az)
{
    nlohmann::json response;
    nlohmann::json request = {{"Azimuth", az}};
    return sendAlpacaPUT("/synctoazimuth", request, response);
}
